// DirectX 12 PoC 向け最小単一フレーム処理。
#ifndef EXEDIT2_DX12_POC_PROCESSOR_H
#define EXEDIT2_DX12_POC_PROCESSOR_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "Dx12PocBackend.h"

// 既定経路の DLL キャッシュ時に FreeLibrary を抑止するための no-op 関数。
inline BOOL WINAPI dx12_poc_noop_free_library(HMODULE)
{
	return TRUE;
}

// DX12 PoC の最小経路として 1 フレーム分を転送する。
// 現段階では計算処理を持たず、実行経路の成立確認を目的とする。
inline bool process_dx12_poc_single_frame(
	const std::uint32_t* inputPixels,
	std::uint32_t* outputPixels,
	int width,
	int height,
	bool userRequestedDx12Poc,
	const Dx12PocProbeResult& probe)
{
	if (!can_enable_dx12_poc(userRequestedDx12Poc, probe)) {
		return false;
	}
	if (inputPixels == nullptr || outputPixels == nullptr || width <= 0 || height <= 0) {
		return false;
	}

	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	std::memcpy(outputPixels, inputPixels, pixelCount * sizeof(std::uint32_t));
	return true;
}

// DX12 PoC の最小初期化として D3D12Device 作成可否を確認する。
// 現段階では compute 実行前の技術検証目的で使用する。
inline bool try_create_dx12_device_for_poc(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	HMODULE d3d12Module = loadLibraryFn(L"d3d12.dll");
	if (d3d12Module == nullptr) {
		return false;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	const FARPROC createDeviceProcRaw = getProcAddressFn(d3d12Module, "D3D12CreateDevice");
	const Dx12CreateDeviceFn createDeviceFn = reinterpret_cast<Dx12CreateDeviceFn>(createDeviceProcRaw);
	if (createDeviceFn == nullptr) {
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12Device* device = nullptr;
	const HRESULT hr = createDeviceFn(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&device));
	const bool available = SUCCEEDED(hr) && device != nullptr;
	if (device != nullptr) {
		device->Release();
	}
	freeLibraryFn(d3d12Module);
	return available;
}

// DX12 PoC の前段として HLSL コンパイル可否を確認する。
// d3dcompiler_47.dll を遅延ロードし、最小 compute shader をコンパイルする。
inline bool try_compile_dx12_poc_shader_for_poc(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	HMODULE compilerModule = loadLibraryFn(L"d3dcompiler_47.dll");
	if (compilerModule == nullptr) {
		return false;
	}

	using D3DCompileFn = HRESULT(WINAPI*)(
		LPCVOID,
		SIZE_T,
		LPCSTR,
		const D3D_SHADER_MACRO*,
		ID3DInclude*,
		LPCSTR,
		LPCSTR,
		UINT,
		UINT,
		ID3DBlob**,
		ID3DBlob**);
	const FARPROC compileProcRaw = getProcAddressFn(compilerModule, "D3DCompile");
	const D3DCompileFn compileFn = reinterpret_cast<D3DCompileFn>(compileProcRaw);
	if (compileFn == nullptr) {
		freeLibraryFn(compilerModule);
		return false;
	}

	static const char* shaderSource =
		"RWStructuredBuffer<uint> outputData : register(u0);\n"
		"[numthreads(1,1,1)]\n"
		"void main(uint3 tid : SV_DispatchThreadID)\n"
		"{\n"
		"	outputData[0] = tid.x;\n"
		"}\n";

	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	const HRESULT hr = compileFn(
		shaderSource,
		std::strlen(shaderSource),
		"Dx12PocEmbedded.hlsl",
		nullptr,
		nullptr,
		"main",
		"cs_5_0",
		0,
		0,
		&shaderBlob,
		&errorBlob);
	const bool compiled = SUCCEEDED(hr) && shaderBlob != nullptr;
	if (shaderBlob != nullptr) {
		shaderBlob->Release();
	}
	if (errorBlob != nullptr) {
		errorBlob->Release();
	}
	freeLibraryFn(compilerModule);
	return compiled;
}

// DX12 PoC の前段としてコマンドキュー/リスト生成可否を確認する。
// 実 dispatch 前に実行基盤の生成可否を検証する。
inline bool try_create_dx12_command_objects_for_poc(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	HMODULE d3d12Module = loadLibraryFn(L"d3d12.dll");
	if (d3d12Module == nullptr) {
		return false;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	const FARPROC createDeviceProcRaw = getProcAddressFn(d3d12Module, "D3D12CreateDevice");
	const Dx12CreateDeviceFn createDeviceFn = reinterpret_cast<Dx12CreateDeviceFn>(createDeviceProcRaw);
	if (createDeviceFn == nullptr) {
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12Device* device = nullptr;
	const HRESULT createDeviceHr = createDeviceFn(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		__uuidof(ID3D12Device),
		reinterpret_cast<void**>(&device));
	if (FAILED(createDeviceHr) || device == nullptr) {
		if (device != nullptr) {
			device->Release();
		}
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;

	ID3D12CommandQueue* queue = nullptr;
	ID3D12CommandAllocator* allocator = nullptr;
	ID3D12GraphicsCommandList* commandList = nullptr;
	const HRESULT queueHr = device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&queue));
	const HRESULT allocatorHr = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_COMPUTE,
		__uuidof(ID3D12CommandAllocator),
		reinterpret_cast<void**>(&allocator));
	const HRESULT listHr = device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COMPUTE,
		allocator,
		nullptr,
		__uuidof(ID3D12GraphicsCommandList),
		reinterpret_cast<void**>(&commandList));
	const bool ok = SUCCEEDED(queueHr) && SUCCEEDED(allocatorHr) && SUCCEEDED(listHr) &&
		queue != nullptr && allocator != nullptr && commandList != nullptr;
	if (commandList != nullptr) {
		commandList->Close();
		commandList->Release();
	}
	if (allocator != nullptr) {
		allocator->Release();
	}
	if (queue != nullptr) {
		queue->Release();
	}
	device->Release();
	freeLibraryFn(d3d12Module);
	return ok;
}

// DX12 PoC の前段として compute pipeline（RootSignature + PSO）生成可否を確認する。
// 実 dispatch 実装へ進む前に、最小パイプライン構築の成立を検証する。
inline bool try_create_dx12_compute_pipeline_for_poc(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	const bool useCachedModules =
		(loadLibraryFn == ::LoadLibraryW) &&
		(getProcAddressFn == ::GetProcAddress) &&
		(freeLibraryFn == ::FreeLibrary);
	HMODULE d3d12Module = nullptr;
	HMODULE compilerModule = nullptr;
	if (useCachedModules) {
		static HMODULE s_d3d12Module = ::LoadLibraryW(L"d3d12.dll");
		static HMODULE s_compilerModule = ::LoadLibraryW(L"d3dcompiler_47.dll");
		d3d12Module = s_d3d12Module;
		compilerModule = s_compilerModule;
		// キャッシュ経路ではモジュールを解放しない。
		freeLibraryFn = dx12_poc_noop_free_library;
	} else {
		d3d12Module = loadLibraryFn(L"d3d12.dll");
		compilerModule = loadLibraryFn(L"d3dcompiler_47.dll");
	}
	if (d3d12Module == nullptr || compilerModule == nullptr) {
		if (compilerModule != nullptr) {
			freeLibraryFn(compilerModule);
		}
		if (d3d12Module != nullptr) {
			freeLibraryFn(d3d12Module);
		}
		return false;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	using Dx12SerializeRootSignatureFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
	using D3DCompileFn = HRESULT(WINAPI*)(
		LPCVOID,
		SIZE_T,
		LPCSTR,
		const D3D_SHADER_MACRO*,
		ID3DInclude*,
		LPCSTR,
		LPCSTR,
		UINT,
		UINT,
		ID3DBlob**,
		ID3DBlob**);

	const Dx12CreateDeviceFn createDeviceFn = reinterpret_cast<Dx12CreateDeviceFn>(getProcAddressFn(d3d12Module, "D3D12CreateDevice"));
	const Dx12SerializeRootSignatureFn serializeRootSignatureFn = reinterpret_cast<Dx12SerializeRootSignatureFn>(getProcAddressFn(d3d12Module, "D3D12SerializeRootSignature"));
	const D3DCompileFn compileFn = reinterpret_cast<D3DCompileFn>(getProcAddressFn(compilerModule, "D3DCompile"));
	if (createDeviceFn == nullptr || serializeRootSignatureFn == nullptr || compileFn == nullptr) {
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12Device* device = nullptr;
	const HRESULT createDeviceHr = createDeviceFn(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		__uuidof(ID3D12Device),
		reinterpret_cast<void**>(&device));
	if (FAILED(createDeviceHr) || device == nullptr) {
		if (device != nullptr) {
			device->Release();
		}
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = 0;
	rootDesc.pParameters = nullptr;
	rootDesc.NumStaticSamplers = 0;
	rootDesc.pStaticSamplers = nullptr;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* rootSigBlob = nullptr;
	ID3DBlob* rootSigErrorBlob = nullptr;
	const HRESULT serializeHr = serializeRootSignatureFn(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&rootSigBlob,
		&rootSigErrorBlob);
	if (rootSigErrorBlob != nullptr) {
		rootSigErrorBlob->Release();
	}
	if (FAILED(serializeHr) || rootSigBlob == nullptr) {
		if (rootSigBlob != nullptr) {
			rootSigBlob->Release();
		}
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12RootSignature* rootSignature = nullptr;
	const HRESULT createRootSigHr = device->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		__uuidof(ID3D12RootSignature),
		reinterpret_cast<void**>(&rootSignature));
	rootSigBlob->Release();
	if (FAILED(createRootSigHr) || rootSignature == nullptr) {
		if (rootSignature != nullptr) {
			rootSignature->Release();
		}
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	static const char* shaderSource =
		"[numthreads(1,1,1)]\n"
		"void main(uint3 tid : SV_DispatchThreadID)\n"
		"{\n"
		"	uint keep = tid.x;\n"
		"	if (keep == 0xffffffffu) {\n"
		"		return;\n"
		"	}\n"
		"}\n";

	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* shaderErrorBlob = nullptr;
	const HRESULT compileHr = compileFn(
		shaderSource,
		std::strlen(shaderSource),
		"Dx12PocPipeline.hlsl",
		nullptr,
		nullptr,
		"main",
		"cs_5_0",
		0,
		0,
		&shaderBlob,
		&shaderErrorBlob);
	if (shaderErrorBlob != nullptr) {
		shaderErrorBlob->Release();
	}
	if (FAILED(compileHr) || shaderBlob == nullptr) {
		if (shaderBlob != nullptr) {
			shaderBlob->Release();
		}
		rootSignature->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
	psoDesc.CS.BytecodeLength = shaderBlob->GetBufferSize();
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ID3D12PipelineState* pipelineState = nullptr;
	const HRESULT createPsoHr = device->CreateComputePipelineState(
		&psoDesc,
		__uuidof(ID3D12PipelineState),
		reinterpret_cast<void**>(&pipelineState));
	const bool ok = SUCCEEDED(createPsoHr) && pipelineState != nullptr;
	if (pipelineState != nullptr) {
		pipelineState->Release();
	}
	shaderBlob->Release();
	rootSignature->Release();
	device->Release();
	freeLibraryFn(compilerModule);
	freeLibraryFn(d3d12Module);
	return ok;
}

// DX12 PoC の前段として入出力バッファ（default/upload/readback）生成可否を確認する。
// dispatch 前に必要となる最低限のリソース確保可否を検証する。
inline bool try_create_dx12_buffer_resources_for_poc(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	HMODULE d3d12Module = loadLibraryFn(L"d3d12.dll");
	if (d3d12Module == nullptr) {
		return false;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	const Dx12CreateDeviceFn createDeviceFn =
		reinterpret_cast<Dx12CreateDeviceFn>(getProcAddressFn(d3d12Module, "D3D12CreateDevice"));
	if (createDeviceFn == nullptr) {
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12Device* device = nullptr;
	const HRESULT createDeviceHr = createDeviceFn(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		__uuidof(ID3D12Device),
		reinterpret_cast<void**>(&device));
	if (FAILED(createDeviceHr) || device == nullptr) {
		if (device != nullptr) {
			device->Release();
		}
		freeLibraryFn(d3d12Module);
		return false;
	}

	const UINT64 byteSize = 256;
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = byteSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES defaultHeap = {};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	defaultHeap.CreationNodeMask = 1;
	defaultHeap.VisibleNodeMask = 1;

	D3D12_HEAP_PROPERTIES uploadHeap = {};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeap.CreationNodeMask = 1;
	uploadHeap.VisibleNodeMask = 1;

	D3D12_HEAP_PROPERTIES readbackHeap = {};
	readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
	readbackHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	readbackHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	readbackHeap.CreationNodeMask = 1;
	readbackHeap.VisibleNodeMask = 1;

	ID3D12Resource* defaultBuffer = nullptr;
	ID3D12Resource* uploadBuffer = nullptr;
	ID3D12Resource* readbackBuffer = nullptr;
	const HRESULT defaultHr = device->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&defaultBuffer));

	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	const HRESULT uploadHr = device->CreateCommittedResource(
		&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&uploadBuffer));
	const HRESULT readbackHr = device->CreateCommittedResource(
		&readbackHeap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		__uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&readbackBuffer));

	const bool ok = SUCCEEDED(defaultHr) && SUCCEEDED(uploadHr) && SUCCEEDED(readbackHr) &&
		defaultBuffer != nullptr && uploadBuffer != nullptr && readbackBuffer != nullptr;
	if (readbackBuffer != nullptr) {
		readbackBuffer->Release();
	}
	if (uploadBuffer != nullptr) {
		uploadBuffer->Release();
	}
	if (defaultBuffer != nullptr) {
		defaultBuffer->Release();
	}
	device->Release();
	freeLibraryFn(d3d12Module);
	return ok;
}

// DX12 PoC の前段として descriptor heap（CBV/SRV/UAV）と fence 生成可否を確認する。
// dispatch/同期実装前に、GPU 可視ディスクリプタと同期オブジェクトの作成可否を検証する。
inline bool try_create_dx12_descriptor_and_sync_for_poc(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	HMODULE d3d12Module = loadLibraryFn(L"d3d12.dll");
	if (d3d12Module == nullptr) {
		return false;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	const Dx12CreateDeviceFn createDeviceFn =
		reinterpret_cast<Dx12CreateDeviceFn>(getProcAddressFn(d3d12Module, "D3D12CreateDevice"));
	if (createDeviceFn == nullptr) {
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12Device* device = nullptr;
	const HRESULT createDeviceHr = createDeviceFn(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		__uuidof(ID3D12Device),
		reinterpret_cast<void**>(&device));
	if (FAILED(createDeviceHr) || device == nullptr) {
		if (device != nullptr) {
			device->Release();
		}
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	const HRESULT heapHr = device->CreateDescriptorHeap(
		&heapDesc,
		__uuidof(ID3D12DescriptorHeap),
		reinterpret_cast<void**>(&descriptorHeap));

	ID3D12Fence* fence = nullptr;
	const HRESULT fenceHr = device->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		__uuidof(ID3D12Fence),
		reinterpret_cast<void**>(&fence));

	const bool ok = SUCCEEDED(heapHr) && SUCCEEDED(fenceHr) &&
		descriptorHeap != nullptr && fence != nullptr;
	if (fence != nullptr) {
		fence->Release();
	}
	if (descriptorHeap != nullptr) {
		descriptorHeap->Release();
	}
	device->Release();
	freeLibraryFn(d3d12Module);
	return ok;
}

// DX12 PoC の最小 dispatch + readback 実行を確認する。
// compute shader で 1 要素を書き込み、readback で値を検証する。
inline bool try_execute_dx12_dispatch_roundtrip_for_poc(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	HMODULE d3d12Module = loadLibraryFn(L"d3d12.dll");
	HMODULE compilerModule = loadLibraryFn(L"d3dcompiler_47.dll");
	if (d3d12Module == nullptr || compilerModule == nullptr) {
		if (compilerModule != nullptr) {
			freeLibraryFn(compilerModule);
		}
		if (d3d12Module != nullptr) {
			freeLibraryFn(d3d12Module);
		}
		return false;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	using Dx12SerializeRootSignatureFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
	using D3DCompileFn = HRESULT(WINAPI*)(
		LPCVOID,
		SIZE_T,
		LPCSTR,
		const D3D_SHADER_MACRO*,
		ID3DInclude*,
		LPCSTR,
		LPCSTR,
		UINT,
		UINT,
		ID3DBlob**,
		ID3DBlob**);

	const Dx12CreateDeviceFn createDeviceFn =
		reinterpret_cast<Dx12CreateDeviceFn>(getProcAddressFn(d3d12Module, "D3D12CreateDevice"));
	const Dx12SerializeRootSignatureFn serializeRootSignatureFn =
		reinterpret_cast<Dx12SerializeRootSignatureFn>(getProcAddressFn(d3d12Module, "D3D12SerializeRootSignature"));
	const D3DCompileFn compileFn =
		reinterpret_cast<D3DCompileFn>(getProcAddressFn(compilerModule, "D3DCompile"));
	if (createDeviceFn == nullptr || serializeRootSignatureFn == nullptr || compileFn == nullptr) {
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12Device* device = nullptr;
	if (FAILED(createDeviceFn(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&device))) ||
		device == nullptr) {
		if (device != nullptr) {
			device->Release();
		}
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ID3D12CommandQueue* queue = nullptr;
	ID3D12CommandAllocator* allocator = nullptr;
	ID3D12GraphicsCommandList* commandList = nullptr;
	if (FAILED(device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&queue))) ||
		FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&allocator))) ||
		FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, allocator, nullptr, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&commandList))) ||
		queue == nullptr || allocator == nullptr || commandList == nullptr) {
		if (commandList != nullptr) {
			commandList->Release();
		}
		if (allocator != nullptr) {
			allocator->Release();
		}
		if (queue != nullptr) {
			queue->Release();
		}
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	if (FAILED(device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&descriptorHeap))) ||
		descriptorHeap == nullptr) {
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	const UINT64 byteSize = sizeof(std::uint32_t);
	D3D12_RESOURCE_DESC defaultDesc = {};
	defaultDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	defaultDesc.Width = byteSize;
	defaultDesc.Height = 1;
	defaultDesc.DepthOrArraySize = 1;
	defaultDesc.MipLevels = 1;
	defaultDesc.Format = DXGI_FORMAT_UNKNOWN;
	defaultDesc.SampleDesc.Count = 1;
	defaultDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	defaultDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_RESOURCE_DESC readbackDesc = defaultDesc;
	readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES defaultHeap = {};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeap.CreationNodeMask = 1;
	defaultHeap.VisibleNodeMask = 1;

	D3D12_HEAP_PROPERTIES readbackHeap = {};
	readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
	readbackHeap.CreationNodeMask = 1;
	readbackHeap.VisibleNodeMask = 1;

	ID3D12Resource* outputBuffer = nullptr;
	ID3D12Resource* readbackBuffer = nullptr;
	if (FAILED(device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&defaultDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			__uuidof(ID3D12Resource),
			reinterpret_cast<void**>(&outputBuffer))) ||
		FAILED(device->CreateCommittedResource(
			&readbackHeap,
			D3D12_HEAP_FLAG_NONE,
			&readbackDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			__uuidof(ID3D12Resource),
			reinterpret_cast<void**>(&readbackBuffer))) ||
		outputBuffer == nullptr || readbackBuffer == nullptr) {
		if (readbackBuffer != nullptr) {
			readbackBuffer->Release();
		}
		if (outputBuffer != nullptr) {
			outputBuffer->Release();
		}
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = sizeof(std::uint32_t);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	device->CreateUnorderedAccessView(outputBuffer, nullptr, &uavDesc, descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_DESCRIPTOR_RANGE range = {};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	range.NumDescriptors = 1;
	range.BaseShaderRegister = 0;
	range.RegisterSpace = 0;
	range.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.DescriptorTable.NumDescriptorRanges = 1;
	rootParam.DescriptorTable.pDescriptorRanges = &range;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = 1;
	rootDesc.pParameters = &rootParam;
	rootDesc.NumStaticSamplers = 0;
	rootDesc.pStaticSamplers = nullptr;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* rootSigBlob = nullptr;
	ID3DBlob* rootSigErr = nullptr;
	ID3D12RootSignature* rootSignature = nullptr;
	if (FAILED(serializeRootSignatureFn(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSigBlob, &rootSigErr)) ||
		rootSigBlob == nullptr ||
		FAILED(device->CreateRootSignature(
			0,
			rootSigBlob->GetBufferPointer(),
			rootSigBlob->GetBufferSize(),
			__uuidof(ID3D12RootSignature),
			reinterpret_cast<void**>(&rootSignature))) ||
		rootSignature == nullptr) {
		if (rootSigErr != nullptr) {
			rootSigErr->Release();
		}
		if (rootSigBlob != nullptr) {
			rootSigBlob->Release();
		}
		if (rootSignature != nullptr) {
			rootSignature->Release();
		}
		readbackBuffer->Release();
		outputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}
	if (rootSigErr != nullptr) {
		rootSigErr->Release();
	}
	rootSigBlob->Release();

	static const char* shaderSource =
		"RWStructuredBuffer<uint> outputData : register(u0);\n"
		"[numthreads(1,1,1)]\n"
		"void main(uint3 tid : SV_DispatchThreadID)\n"
		"{\n"
		"	uint keep = tid.x;\n"
		"	if (keep == 0xffffffffu) {\n"
		"		return;\n"
		"	}\n"
		"	outputData[0] = 123u;\n"
		"}\n";

	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* shaderErr = nullptr;
	if (FAILED(compileFn(
			shaderSource,
			std::strlen(shaderSource),
			"Dx12PocRoundtrip.hlsl",
			nullptr,
			nullptr,
			"main",
			"cs_5_0",
			0,
			0,
			&shaderBlob,
			&shaderErr)) || shaderBlob == nullptr) {
		if (shaderErr != nullptr) {
			shaderErr->Release();
		}
		if (shaderBlob != nullptr) {
			shaderBlob->Release();
		}
		rootSignature->Release();
		readbackBuffer->Release();
		outputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}
	if (shaderErr != nullptr) {
		shaderErr->Release();
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
	psoDesc.CS.BytecodeLength = shaderBlob->GetBufferSize();
	ID3D12PipelineState* pipelineState = nullptr;
	if (FAILED(device->CreateComputePipelineState(
			&psoDesc,
			__uuidof(ID3D12PipelineState),
			reinterpret_cast<void**>(&pipelineState))) || pipelineState == nullptr) {
		shaderBlob->Release();
		rootSignature->Release();
		readbackBuffer->Release();
		outputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_RESOURCE_BARRIER toUav = {};
	toUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toUav.Transition.pResource = outputBuffer;
	toUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	toUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &toUav);

	ID3D12DescriptorHeap* heaps[] = { descriptorHeap };
	commandList->SetDescriptorHeaps(1, heaps);
	commandList->SetComputeRootSignature(rootSignature);
	commandList->SetPipelineState(pipelineState);
	commandList->SetComputeRootDescriptorTable(0, descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	commandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER toCopy = {};
	toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopy.Transition.pResource = outputBuffer;
	toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &toCopy);
	commandList->CopyBufferRegion(readbackBuffer, 0, outputBuffer, 0, sizeof(std::uint32_t));

	if (FAILED(commandList->Close())) {
		pipelineState->Release();
		shaderBlob->Release();
		rootSignature->Release();
		readbackBuffer->Release();
		outputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12CommandList* lists[] = { commandList };
	queue->ExecuteCommandLists(1, lists);

	ID3D12Fence* fence = nullptr;
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&fence))) ||
		fence == nullptr ||
		FAILED(queue->Signal(fence, 1))) {
		if (fence != nullptr) {
			fence->Release();
		}
		pipelineState->Release();
		shaderBlob->Release();
		rootSignature->Release();
		readbackBuffer->Release();
		outputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	bool waited = true;
	HANDLE eventHandle = nullptr;
	if (fence->GetCompletedValue() < 1) {
		eventHandle = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (eventHandle == nullptr) {
			waited = false;
		} else if (FAILED(fence->SetEventOnCompletion(1, eventHandle))) {
			waited = false;
		} else if (::WaitForSingleObject(eventHandle, 3000) != WAIT_OBJECT_0) {
			waited = false;
		}
	}
	if (eventHandle != nullptr) {
		::CloseHandle(eventHandle);
	}

	bool ok = false;
	if (waited) {
		D3D12_RANGE readRange = {};
		readRange.Begin = 0;
		readRange.End = sizeof(std::uint32_t);
		void* mapped = nullptr;
		if (SUCCEEDED(readbackBuffer->Map(0, &readRange, &mapped)) && mapped != nullptr) {
			const std::uint32_t value = *reinterpret_cast<const std::uint32_t*>(mapped);
			ok = (value == 123u);
			const D3D12_RANGE writeRange = {};
			readbackBuffer->Unmap(0, &writeRange);
		}
	}

	fence->Release();
	pipelineState->Release();
	shaderBlob->Release();
	rootSignature->Release();
	readbackBuffer->Release();
	outputBuffer->Release();
	descriptorHeap->Release();
	commandList->Release();
	allocator->Release();
	queue->Release();
	device->Release();
	freeLibraryFn(compilerModule);
	freeLibraryFn(d3d12Module);
	return ok;
}

// DX12 PoC の前段として SRV/UAV 入出力を使う最小 dispatch + readback を確認する。
// 入力1要素をGPUで読み、+1して出力し、readback で値を検証する。
inline bool try_execute_dx12_dispatch_with_io_roundtrip_for_poc(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	HMODULE d3d12Module = loadLibraryFn(L"d3d12.dll");
	HMODULE compilerModule = loadLibraryFn(L"d3dcompiler_47.dll");
	if (d3d12Module == nullptr || compilerModule == nullptr) {
		if (compilerModule != nullptr) {
			freeLibraryFn(compilerModule);
		}
		if (d3d12Module != nullptr) {
			freeLibraryFn(d3d12Module);
		}
		return false;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	using Dx12SerializeRootSignatureFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
	using D3DCompileFn = HRESULT(WINAPI*)(
		LPCVOID,
		SIZE_T,
		LPCSTR,
		const D3D_SHADER_MACRO*,
		ID3DInclude*,
		LPCSTR,
		LPCSTR,
		UINT,
		UINT,
		ID3DBlob**,
		ID3DBlob**);

	const Dx12CreateDeviceFn createDeviceFn =
		reinterpret_cast<Dx12CreateDeviceFn>(getProcAddressFn(d3d12Module, "D3D12CreateDevice"));
	const Dx12SerializeRootSignatureFn serializeRootSignatureFn =
		reinterpret_cast<Dx12SerializeRootSignatureFn>(getProcAddressFn(d3d12Module, "D3D12SerializeRootSignature"));
	const D3DCompileFn compileFn =
		reinterpret_cast<D3DCompileFn>(getProcAddressFn(compilerModule, "D3DCompile"));
	if (createDeviceFn == nullptr || serializeRootSignatureFn == nullptr || compileFn == nullptr) {
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12Device* device = nullptr;
	if (FAILED(createDeviceFn(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&device))) ||
		device == nullptr) {
		if (device != nullptr) {
			device->Release();
		}
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ID3D12CommandQueue* queue = nullptr;
	ID3D12CommandAllocator* allocator = nullptr;
	ID3D12GraphicsCommandList* commandList = nullptr;
	if (FAILED(device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&queue))) ||
		FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&allocator))) ||
		FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, allocator, nullptr, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&commandList))) ||
		queue == nullptr || allocator == nullptr || commandList == nullptr) {
		if (commandList != nullptr) {
			commandList->Release();
		}
		if (allocator != nullptr) {
			allocator->Release();
		}
		if (queue != nullptr) {
			queue->Release();
		}
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	if (FAILED(device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&descriptorHeap))) ||
		descriptorHeap == nullptr) {
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	const UINT64 byteSize = sizeof(std::uint32_t);
	const std::uint32_t inputValue = 41u;
	D3D12_RESOURCE_DESC inputDesc = {};
	inputDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	inputDesc.Width = byteSize;
	inputDesc.Height = 1;
	inputDesc.DepthOrArraySize = 1;
	inputDesc.MipLevels = 1;
	inputDesc.Format = DXGI_FORMAT_UNKNOWN;
	inputDesc.SampleDesc.Count = 1;
	inputDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	inputDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_RESOURCE_DESC outputDesc = inputDesc;
	outputDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES defaultHeap = {};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeap.CreationNodeMask = 1;
	defaultHeap.VisibleNodeMask = 1;

	D3D12_HEAP_PROPERTIES uploadHeap = {};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeap.CreationNodeMask = 1;
	uploadHeap.VisibleNodeMask = 1;

	D3D12_HEAP_PROPERTIES readbackHeap = {};
	readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
	readbackHeap.CreationNodeMask = 1;
	readbackHeap.VisibleNodeMask = 1;

	ID3D12Resource* inputBuffer = nullptr;
	ID3D12Resource* outputBuffer = nullptr;
	ID3D12Resource* uploadBuffer = nullptr;
	ID3D12Resource* readbackBuffer = nullptr;
	bool resourceOk =
		SUCCEEDED(device->CreateCommittedResource(
			&defaultHeap, D3D12_HEAP_FLAG_NONE, &inputDesc, D3D12_RESOURCE_STATE_COMMON, nullptr,
			__uuidof(ID3D12Resource), reinterpret_cast<void**>(&inputBuffer))) &&
		SUCCEEDED(device->CreateCommittedResource(
			&defaultHeap, D3D12_HEAP_FLAG_NONE, &outputDesc, D3D12_RESOURCE_STATE_COMMON, nullptr,
			__uuidof(ID3D12Resource), reinterpret_cast<void**>(&outputBuffer))) &&
		SUCCEEDED(device->CreateCommittedResource(
			&uploadHeap, D3D12_HEAP_FLAG_NONE, &inputDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			__uuidof(ID3D12Resource), reinterpret_cast<void**>(&uploadBuffer))) &&
		SUCCEEDED(device->CreateCommittedResource(
			&readbackHeap, D3D12_HEAP_FLAG_NONE, &inputDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			__uuidof(ID3D12Resource), reinterpret_cast<void**>(&readbackBuffer))) &&
		inputBuffer != nullptr && outputBuffer != nullptr && uploadBuffer != nullptr && readbackBuffer != nullptr;
	if (!resourceOk) {
		if (readbackBuffer != nullptr) {
			readbackBuffer->Release();
		}
		if (uploadBuffer != nullptr) {
			uploadBuffer->Release();
		}
		if (outputBuffer != nullptr) {
			outputBuffer->Release();
		}
		if (inputBuffer != nullptr) {
			inputBuffer->Release();
		}
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	void* uploadPtr = nullptr;
	D3D12_RANGE uploadReadRange = {};
	if (FAILED(uploadBuffer->Map(0, &uploadReadRange, &uploadPtr)) || uploadPtr == nullptr) {
		readbackBuffer->Release();
		uploadBuffer->Release();
		outputBuffer->Release();
		inputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}
	*reinterpret_cast<std::uint32_t*>(uploadPtr) = inputValue;
	D3D12_RANGE uploadWrittenRange = {};
	uploadWrittenRange.Begin = 0;
	uploadWrittenRange.End = sizeof(std::uint32_t);
	uploadBuffer->Unmap(0, &uploadWrittenRange);

	const UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuBase = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv = cpuBase;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuUav = cpuBase;
	cpuUav.ptr += descriptorSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = 1;
	srvDesc.Buffer.StructureByteStride = sizeof(std::uint32_t);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	device->CreateShaderResourceView(inputBuffer, &srvDesc, cpuSrv);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = sizeof(std::uint32_t);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	device->CreateUnorderedAccessView(outputBuffer, nullptr, &uavDesc, cpuUav);

	D3D12_DESCRIPTOR_RANGE ranges[2] = {};
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER rootParams[2] = {};
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[0].DescriptorTable.pDescriptorRanges = &ranges[0];
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[1].DescriptorTable.pDescriptorRanges = &ranges[1];
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = 2;
	rootDesc.pParameters = rootParams;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* rootSigBlob = nullptr;
	ID3DBlob* rootSigErr = nullptr;
	ID3D12RootSignature* rootSignature = nullptr;
	bool rootOk =
		SUCCEEDED(serializeRootSignatureFn(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSigBlob, &rootSigErr)) &&
		rootSigBlob != nullptr &&
		SUCCEEDED(device->CreateRootSignature(
			0,
			rootSigBlob->GetBufferPointer(),
			rootSigBlob->GetBufferSize(),
			__uuidof(ID3D12RootSignature),
			reinterpret_cast<void**>(&rootSignature))) &&
		rootSignature != nullptr;
	if (rootSigErr != nullptr) {
		rootSigErr->Release();
	}
	if (rootSigBlob != nullptr) {
		rootSigBlob->Release();
	}
	if (!rootOk) {
		if (rootSignature != nullptr) {
			rootSignature->Release();
		}
		readbackBuffer->Release();
		uploadBuffer->Release();
		outputBuffer->Release();
		inputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	static const char* shaderSource =
		"StructuredBuffer<uint> inputData : register(t0);\n"
		"RWStructuredBuffer<uint> outputData : register(u0);\n"
		"[numthreads(1,1,1)]\n"
		"void main(uint3 tid : SV_DispatchThreadID)\n"
		"{\n"
		"	uint keep = tid.x;\n"
		"	if (keep == 0xffffffffu) {\n"
		"		return;\n"
		"	}\n"
		"	outputData[0] = inputData[0] + 1u;\n"
		"}\n";

	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* shaderErr = nullptr;
	if (FAILED(compileFn(
			shaderSource,
			std::strlen(shaderSource),
			"Dx12PocIoRoundtrip.hlsl",
			nullptr,
			nullptr,
			"main",
			"cs_5_0",
			0,
			0,
			&shaderBlob,
			&shaderErr)) || shaderBlob == nullptr) {
		if (shaderErr != nullptr) {
			shaderErr->Release();
		}
		if (shaderBlob != nullptr) {
			shaderBlob->Release();
		}
		rootSignature->Release();
		readbackBuffer->Release();
		uploadBuffer->Release();
		outputBuffer->Release();
		inputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}
	if (shaderErr != nullptr) {
		shaderErr->Release();
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
	psoDesc.CS.BytecodeLength = shaderBlob->GetBufferSize();
	ID3D12PipelineState* pipelineState = nullptr;
	if (FAILED(device->CreateComputePipelineState(
			&psoDesc,
			__uuidof(ID3D12PipelineState),
			reinterpret_cast<void**>(&pipelineState))) || pipelineState == nullptr) {
		shaderBlob->Release();
		rootSignature->Release();
		readbackBuffer->Release();
		uploadBuffer->Release();
		outputBuffer->Release();
		inputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_RESOURCE_BARRIER barriers[3] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = inputBuffer;
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barriers[0]);
	commandList->CopyBufferRegion(inputBuffer, 0, uploadBuffer, 0, sizeof(std::uint32_t));
	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Transition.pResource = inputBuffer;
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[2].Transition.pResource = outputBuffer;
	barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(2, &barriers[1]);

	ID3D12DescriptorHeap* heaps[] = { descriptorHeap };
	commandList->SetDescriptorHeaps(1, heaps);
	commandList->SetComputeRootSignature(rootSignature);
	commandList->SetPipelineState(pipelineState);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuBase = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv = gpuBase;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuUav = gpuBase;
	gpuUav.ptr += descriptorSize;
	commandList->SetComputeRootDescriptorTable(0, gpuSrv);
	commandList->SetComputeRootDescriptorTable(1, gpuUav);
	commandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER toCopy = {};
	toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopy.Transition.pResource = outputBuffer;
	toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &toCopy);
	commandList->CopyBufferRegion(readbackBuffer, 0, outputBuffer, 0, sizeof(std::uint32_t));

	if (FAILED(commandList->Close())) {
		pipelineState->Release();
		shaderBlob->Release();
		rootSignature->Release();
		readbackBuffer->Release();
		uploadBuffer->Release();
		outputBuffer->Release();
		inputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12CommandList* lists[] = { commandList };
	queue->ExecuteCommandLists(1, lists);

	ID3D12Fence* fence = nullptr;
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&fence))) ||
		fence == nullptr ||
		FAILED(queue->Signal(fence, 1))) {
		if (fence != nullptr) {
			fence->Release();
		}
		pipelineState->Release();
		shaderBlob->Release();
		rootSignature->Release();
		readbackBuffer->Release();
		uploadBuffer->Release();
		outputBuffer->Release();
		inputBuffer->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	bool waited = true;
	HANDLE eventHandle = nullptr;
	if (fence->GetCompletedValue() < 1) {
		eventHandle = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (eventHandle == nullptr) {
			waited = false;
		} else if (FAILED(fence->SetEventOnCompletion(1, eventHandle))) {
			waited = false;
		} else if (::WaitForSingleObject(eventHandle, 3000) != WAIT_OBJECT_0) {
			waited = false;
		}
	}
	if (eventHandle != nullptr) {
		::CloseHandle(eventHandle);
	}

	bool ok = false;
	if (waited) {
		D3D12_RANGE readRange = {};
		readRange.Begin = 0;
		readRange.End = sizeof(std::uint32_t);
		void* mapped = nullptr;
		if (SUCCEEDED(readbackBuffer->Map(0, &readRange, &mapped)) && mapped != nullptr) {
			const std::uint32_t value = *reinterpret_cast<const std::uint32_t*>(mapped);
			ok = (value == inputValue + 1u);
			const D3D12_RANGE writeRange = {};
			readbackBuffer->Unmap(0, &writeRange);
		}
	}

	fence->Release();
	pipelineState->Release();
	shaderBlob->Release();
	rootSignature->Release();
	readbackBuffer->Release();
	uploadBuffer->Release();
	outputBuffer->Release();
	inputBuffer->Release();
	descriptorHeap->Release();
	commandList->Release();
	allocator->Release();
	queue->Release();
	device->Release();
	freeLibraryFn(compilerModule);
	freeLibraryFn(d3d12Module);
	return ok;
}

// DX12 PoC の前段としてフレーム全体の SRV/UAV compute dispatch 可否を確認する。
// 3x3 近傍平均の最小 compute を実行し、readback で期待値一致を検証する。
inline bool try_execute_dx12_fullframe_compute_for_poc(
	const std::uint32_t* inputPixels,
	std::uint32_t* outputPixels,
	int width,
	int height,
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	if (inputPixels == nullptr || outputPixels == nullptr || width <= 0 || height <= 0) {
		return false;
	}
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return false;
	}

	HMODULE d3d12Module = loadLibraryFn(L"d3d12.dll");
	HMODULE compilerModule = loadLibraryFn(L"d3dcompiler_47.dll");
	if (d3d12Module == nullptr || compilerModule == nullptr) {
		if (compilerModule != nullptr) {
			freeLibraryFn(compilerModule);
		}
		if (d3d12Module != nullptr) {
			freeLibraryFn(d3d12Module);
		}
		return false;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	using Dx12SerializeRootSignatureFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
	using D3DCompileFn = HRESULT(WINAPI*)(
		LPCVOID,
		SIZE_T,
		LPCSTR,
		const D3D_SHADER_MACRO*,
		ID3DInclude*,
		LPCSTR,
		LPCSTR,
		UINT,
		UINT,
		ID3DBlob**,
		ID3DBlob**);

	const Dx12CreateDeviceFn createDeviceFn =
		reinterpret_cast<Dx12CreateDeviceFn>(getProcAddressFn(d3d12Module, "D3D12CreateDevice"));
	const Dx12SerializeRootSignatureFn serializeRootSignatureFn =
		reinterpret_cast<Dx12SerializeRootSignatureFn>(getProcAddressFn(d3d12Module, "D3D12SerializeRootSignature"));
	const D3DCompileFn compileFn =
		reinterpret_cast<D3DCompileFn>(getProcAddressFn(compilerModule, "D3DCompile"));
	if (createDeviceFn == nullptr || serializeRootSignatureFn == nullptr || compileFn == nullptr) {
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	const UINT pixelCount = static_cast<UINT>(static_cast<size_t>(width) * static_cast<size_t>(height));
	const UINT64 byteSize = static_cast<UINT64>(pixelCount) * sizeof(std::uint32_t);
	if (pixelCount == 0 || byteSize == 0) {
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12Device* device = nullptr;
	if (FAILED(createDeviceFn(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&device))) ||
		device == nullptr) {
		if (device != nullptr) {
			device->Release();
		}
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ID3D12CommandQueue* queue = nullptr;
	ID3D12CommandAllocator* allocator = nullptr;
	ID3D12GraphicsCommandList* commandList = nullptr;
	if (FAILED(device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&queue))) ||
		FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&allocator))) ||
		FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, allocator, nullptr, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&commandList))) ||
		queue == nullptr || allocator == nullptr || commandList == nullptr) {
		if (commandList != nullptr) {
			commandList->Release();
		}
		if (allocator != nullptr) {
			allocator->Release();
		}
		if (queue != nullptr) {
			queue->Release();
		}
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 3;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	if (FAILED(device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&descriptorHeap))) ||
		descriptorHeap == nullptr) {
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_RESOURCE_DESC ioBufferDesc = {};
	ioBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ioBufferDesc.Width = byteSize;
	ioBufferDesc.Height = 1;
	ioBufferDesc.DepthOrArraySize = 1;
	ioBufferDesc.MipLevels = 1;
	ioBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	ioBufferDesc.SampleDesc.Count = 1;
	ioBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_RESOURCE_DESC inputDefaultDesc = ioBufferDesc;
	inputDefaultDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_DESC outputDefaultDesc = ioBufferDesc;
	outputDefaultDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES defaultHeap = {};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeap.CreationNodeMask = 1;
	defaultHeap.VisibleNodeMask = 1;

	D3D12_HEAP_PROPERTIES uploadHeap = {};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeap.CreationNodeMask = 1;
	uploadHeap.VisibleNodeMask = 1;

	D3D12_HEAP_PROPERTIES readbackHeap = {};
	readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
	readbackHeap.CreationNodeMask = 1;
	readbackHeap.VisibleNodeMask = 1;

	ID3D12Resource* inputDefault = nullptr;
	ID3D12Resource* outputDefault = nullptr;
	ID3D12Resource* inputUpload = nullptr;
	ID3D12Resource* outputReadback = nullptr;
	if (FAILED(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &inputDefaultDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&inputDefault))) ||
		FAILED(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &outputDefaultDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&outputDefault))) ||
		FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &inputDefaultDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&inputUpload))) ||
		FAILED(device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &inputDefaultDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&outputReadback))) ||
		inputDefault == nullptr || outputDefault == nullptr || inputUpload == nullptr || outputReadback == nullptr) {
		if (outputReadback != nullptr) {
			outputReadback->Release();
		}
		if (inputUpload != nullptr) {
			inputUpload->Release();
		}
		if (outputDefault != nullptr) {
			outputDefault->Release();
		}
		if (inputDefault != nullptr) {
			inputDefault->Release();
		}
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	const UINT64 constantBufferSize = 256;
	D3D12_RESOURCE_DESC constantDesc = {};
	constantDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	constantDesc.Width = constantBufferSize;
	constantDesc.Height = 1;
	constantDesc.DepthOrArraySize = 1;
	constantDesc.MipLevels = 1;
	constantDesc.Format = DXGI_FORMAT_UNKNOWN;
	constantDesc.SampleDesc.Count = 1;
	constantDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	constantDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* constantUpload = nullptr;
	if (FAILED(device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&constantDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			__uuidof(ID3D12Resource),
			reinterpret_cast<void**>(&constantUpload))) || constantUpload == nullptr) {
		outputReadback->Release();
		inputUpload->Release();
		outputDefault->Release();
		inputDefault->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	void* inputUploadPtr = nullptr;
	D3D12_RANGE noRead = {};
	if (FAILED(inputUpload->Map(0, &noRead, &inputUploadPtr)) || inputUploadPtr == nullptr) {
		constantUpload->Release();
		outputReadback->Release();
		inputUpload->Release();
		outputDefault->Release();
		inputDefault->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}
	std::memcpy(inputUploadPtr, inputPixels, static_cast<size_t>(byteSize));
	D3D12_RANGE inputWritten = {};
	inputWritten.Begin = 0;
	inputWritten.End = static_cast<SIZE_T>(byteSize);
	inputUpload->Unmap(0, &inputWritten);

	void* constantPtr = nullptr;
	if (FAILED(constantUpload->Map(0, &noRead, &constantPtr)) || constantPtr == nullptr) {
		constantUpload->Release();
		outputReadback->Release();
		inputUpload->Release();
		outputDefault->Release();
		inputDefault->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}
	reinterpret_cast<UINT*>(constantPtr)[0] = static_cast<UINT>(width);
	reinterpret_cast<UINT*>(constantPtr)[1] = static_cast<UINT>(height);
	reinterpret_cast<UINT*>(constantPtr)[2] = pixelCount;
	D3D12_RANGE constantWritten = {};
	constantWritten.Begin = 0;
	constantWritten.End = sizeof(UINT) * 3u;
	constantUpload->Unmap(0, &constantWritten);

	const UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuBase = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv = cpuBase;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuUav = cpuBase;
	cpuUav.ptr += descriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuCbv = cpuBase;
	cpuCbv.ptr += descriptorSize * 2u;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = pixelCount;
	srvDesc.Buffer.StructureByteStride = sizeof(std::uint32_t);
	device->CreateShaderResourceView(inputDefault, &srvDesc, cpuSrv);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = pixelCount;
	uavDesc.Buffer.StructureByteStride = sizeof(std::uint32_t);
	device->CreateUnorderedAccessView(outputDefault, nullptr, &uavDesc, cpuUav);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantUpload->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(constantBufferSize);
	device->CreateConstantBufferView(&cbvDesc, cpuCbv);

	D3D12_DESCRIPTOR_RANGE ranges[3] = {};
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = 0;
	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges[2].NumDescriptors = 1;
	ranges[2].BaseShaderRegister = 0;
	ranges[2].OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER rootParams[3] = {};
	for (int i = 0; i < 3; ++i) {
		rootParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[i].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[i].DescriptorTable.pDescriptorRanges = &ranges[i];
		rootParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = 3;
	rootDesc.pParameters = rootParams;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* rootSigBlob = nullptr;
	ID3DBlob* rootSigErr = nullptr;
	ID3D12RootSignature* rootSignature = nullptr;
	bool rootOk =
		SUCCEEDED(serializeRootSignatureFn(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSigBlob, &rootSigErr)) &&
		rootSigBlob != nullptr &&
		SUCCEEDED(device->CreateRootSignature(
			0,
			rootSigBlob->GetBufferPointer(),
			rootSigBlob->GetBufferSize(),
			__uuidof(ID3D12RootSignature),
			reinterpret_cast<void**>(&rootSignature))) &&
		rootSignature != nullptr;
	if (rootSigErr != nullptr) {
		rootSigErr->Release();
	}
	if (rootSigBlob != nullptr) {
		rootSigBlob->Release();
	}
	if (!rootOk) {
		if (rootSignature != nullptr) {
			rootSignature->Release();
		}
		constantUpload->Release();
		outputReadback->Release();
		inputUpload->Release();
		outputDefault->Release();
		inputDefault->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	static const char* shaderSource =
		"StructuredBuffer<uint> inputData : register(t0);\n"
		"RWStructuredBuffer<uint> outputData : register(u0);\n"
		"cbuffer Constants : register(b0) { uint Width; uint Height; uint PixelCount; uint Reserved; };\n"
		"uint clamp_coord(int value, uint limit)\n"
		"{\n"
		"	return (uint)clamp(value, 0, (int)limit);\n"
		"}\n"
		"[numthreads(64,1,1)]\n"
		"void main(uint3 tid : SV_DispatchThreadID)\n"
		"{\n"
		"	uint index = tid.x;\n"
		"	if(index >= PixelCount) return;\n"
		"	uint x = index % Width;\n"
		"	uint y = index / Width;\n"
		"	uint sumR = 0;\n"
		"	uint sumG = 0;\n"
		"	uint sumB = 0;\n"
		"	for(int dy = -1; dy <= 1; ++dy) {\n"
		"		uint sy = clamp_coord((int)y + dy, Height - 1);\n"
		"		for(int dx = -1; dx <= 1; ++dx) {\n"
		"			uint sx = clamp_coord((int)x + dx, Width - 1);\n"
		"			uint sample = inputData[sy * Width + sx];\n"
		"			sumR += sample & 255u;\n"
		"			sumG += (sample >> 8) & 255u;\n"
		"			sumB += (sample >> 16) & 255u;\n"
		"		}\n"
		"	}\n"
		"	uint center = inputData[index];\n"
		"	uint a = (center >> 24) & 255u;\n"
		"	uint r = (sumR / 9u) & 255u;\n"
		"	uint g = (sumG / 9u) & 255u;\n"
		"	uint b = (sumB / 9u) & 255u;\n"
		"	outputData[index] = (a << 24) | (b << 16) | (g << 8) | r;\n"
		"}\n";

	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* shaderErr = nullptr;
	if (FAILED(compileFn(
			shaderSource,
			std::strlen(shaderSource),
			"Dx12PocFullFrame.hlsl",
			nullptr,
			nullptr,
			"main",
			"cs_5_0",
			0,
			0,
			&shaderBlob,
			&shaderErr)) || shaderBlob == nullptr) {
		if (shaderErr != nullptr) {
			shaderErr->Release();
		}
		if (shaderBlob != nullptr) {
			shaderBlob->Release();
		}
		rootSignature->Release();
		constantUpload->Release();
		outputReadback->Release();
		inputUpload->Release();
		outputDefault->Release();
		inputDefault->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}
	if (shaderErr != nullptr) {
		shaderErr->Release();
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
	psoDesc.CS.BytecodeLength = shaderBlob->GetBufferSize();
	ID3D12PipelineState* pipelineState = nullptr;
	if (FAILED(device->CreateComputePipelineState(
			&psoDesc,
			__uuidof(ID3D12PipelineState),
			reinterpret_cast<void**>(&pipelineState))) || pipelineState == nullptr) {
		shaderBlob->Release();
		rootSignature->Release();
		constantUpload->Release();
		outputReadback->Release();
		inputUpload->Release();
		outputDefault->Release();
		inputDefault->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	D3D12_RESOURCE_BARRIER toCopy = {};
	toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopy.Transition.pResource = inputDefault;
	toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &toCopy);
	commandList->CopyBufferRegion(inputDefault, 0, inputUpload, 0, byteSize);

	D3D12_RESOURCE_BARRIER toShader[2] = {};
	toShader[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toShader[0].Transition.pResource = inputDefault;
	toShader[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	toShader[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	toShader[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	toShader[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toShader[1].Transition.pResource = outputDefault;
	toShader[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	toShader[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	toShader[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(2, toShader);

	ID3D12DescriptorHeap* heaps[] = { descriptorHeap };
	commandList->SetDescriptorHeaps(1, heaps);
	commandList->SetComputeRootSignature(rootSignature);
	commandList->SetPipelineState(pipelineState);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuBase = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv = gpuBase;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuUav = gpuBase;
	gpuUav.ptr += descriptorSize;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuCbv = gpuBase;
	gpuCbv.ptr += descriptorSize * 2u;
	commandList->SetComputeRootDescriptorTable(0, gpuSrv);
	commandList->SetComputeRootDescriptorTable(1, gpuUav);
	commandList->SetComputeRootDescriptorTable(2, gpuCbv);
	const UINT dispatchX = (pixelCount + 63u) / 64u;
	commandList->Dispatch(dispatchX, 1, 1);

	D3D12_RESOURCE_BARRIER toReadback = {};
	toReadback.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toReadback.Transition.pResource = outputDefault;
	toReadback.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	toReadback.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	toReadback.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &toReadback);
	commandList->CopyBufferRegion(outputReadback, 0, outputDefault, 0, byteSize);

	if (FAILED(commandList->Close())) {
		pipelineState->Release();
		shaderBlob->Release();
		rootSignature->Release();
		constantUpload->Release();
		outputReadback->Release();
		inputUpload->Release();
		outputDefault->Release();
		inputDefault->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	ID3D12CommandList* lists[] = { commandList };
	queue->ExecuteCommandLists(1, lists);

	ID3D12Fence* fence = nullptr;
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&fence))) ||
		fence == nullptr ||
		FAILED(queue->Signal(fence, 1))) {
		if (fence != nullptr) {
			fence->Release();
		}
		pipelineState->Release();
		shaderBlob->Release();
		rootSignature->Release();
		constantUpload->Release();
		outputReadback->Release();
		inputUpload->Release();
		outputDefault->Release();
		inputDefault->Release();
		descriptorHeap->Release();
		commandList->Release();
		allocator->Release();
		queue->Release();
		device->Release();
		freeLibraryFn(compilerModule);
		freeLibraryFn(d3d12Module);
		return false;
	}

	bool waited = true;
	HANDLE eventHandle = nullptr;
	if (fence->GetCompletedValue() < 1) {
		eventHandle = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (eventHandle == nullptr) {
			waited = false;
		} else if (FAILED(fence->SetEventOnCompletion(1, eventHandle))) {
			waited = false;
		} else if (::WaitForSingleObject(eventHandle, 5000) != WAIT_OBJECT_0) {
			waited = false;
		}
	}
	if (eventHandle != nullptr) {
		::CloseHandle(eventHandle);
	}

	bool ok = false;
	if (waited) {
		D3D12_RANGE readRange = {};
		readRange.Begin = 0;
		readRange.End = static_cast<SIZE_T>(byteSize);
		void* mapped = nullptr;
		if (SUCCEEDED(outputReadback->Map(0, &readRange, &mapped)) && mapped != nullptr) {
			std::memcpy(outputPixels, mapped, static_cast<size_t>(byteSize));
			ok = true;
			const auto idx = [width](int x, int y) -> size_t {
				return static_cast<size_t>(y * width + x);
			};
			const auto unpack = [](std::uint32_t p, int shift) -> int {
				return static_cast<int>((p >> shift) & 0xffu);
			};
			for (int y = 0; ok && y < height; ++y) {
				for (int x = 0; x < width; ++x) {
					int sumR = 0;
					int sumG = 0;
					int sumB = 0;
					for (int dy = -1; dy <= 1; ++dy) {
						const int sy = std::clamp(y + dy, 0, height - 1);
						for (int dx = -1; dx <= 1; ++dx) {
							const int sx = std::clamp(x + dx, 0, width - 1);
							const std::uint32_t sp = inputPixels[idx(sx, sy)];
							sumR += unpack(sp, 0);
							sumG += unpack(sp, 8);
							sumB += unpack(sp, 16);
						}
					}
					const std::uint32_t center = inputPixels[idx(x, y)];
					const std::uint32_t a = (center >> 24) & 0xffu;
					const std::uint32_t r = static_cast<std::uint32_t>(sumR / 9) & 0xffu;
					const std::uint32_t g = static_cast<std::uint32_t>(sumG / 9) & 0xffu;
					const std::uint32_t b = static_cast<std::uint32_t>(sumB / 9) & 0xffu;
					const std::uint32_t expected = (a << 24) | (b << 16) | (g << 8) | r;
					ok = (outputPixels[idx(x, y)] == expected);
					if (!ok) {
						break;
					}
				}
			}
			const D3D12_RANGE writeRange = {};
			outputReadback->Unmap(0, &writeRange);
		}
	}

	fence->Release();
	pipelineState->Release();
	shaderBlob->Release();
	rootSignature->Release();
	constantUpload->Release();
	outputReadback->Release();
	inputUpload->Release();
	outputDefault->Release();
	inputDefault->Release();
	descriptorHeap->Release();
	commandList->Release();
	allocator->Release();
	queue->Release();
	device->Release();
	freeLibraryFn(compilerModule);
	freeLibraryFn(d3d12Module);
	return ok;
}

// DX12 PoC の最小コンピュート相当処理。
// 3x3 の近傍平均を取り、GPU compute を優先し失敗時のみ CPU へフォールバックする。
inline bool process_dx12_poc_compute_path(
	const std::uint32_t* inputPixels,
	std::uint32_t* outputPixels,
	int width,
	int height,
	bool userRequestedDx12Poc,
	const Dx12PocProbeResult& probe)
{
	if (!can_enable_dx12_poc(userRequestedDx12Poc, probe)) {
		return false;
	}
	if (inputPixels == nullptr || outputPixels == nullptr || width <= 0 || height <= 0) {
		return false;
	}

	// 初期化段の preflight は最初の 1 回だけ実行し、毎フレームのオーバーヘッドを避ける。
	static bool dx12PreflightDone = false;
	if (!dx12PreflightDone) {
		(void)try_create_dx12_device_for_poc();
		(void)try_compile_dx12_poc_shader_for_poc();
		(void)try_create_dx12_command_objects_for_poc();
		(void)try_create_dx12_compute_pipeline_for_poc();
		(void)try_create_dx12_buffer_resources_for_poc();
		(void)try_create_dx12_descriptor_and_sync_for_poc();
		(void)try_execute_dx12_dispatch_roundtrip_for_poc();
		(void)try_execute_dx12_dispatch_with_io_roundtrip_for_poc();
		dx12PreflightDone = true;
	}
	if (try_execute_dx12_fullframe_compute_for_poc(inputPixels, outputPixels, width, height)) {
		return true;
	}

	const auto idx = [width](int x, int y) -> size_t {
		return static_cast<size_t>(y * width + x);
	};
	const auto unpack = [](std::uint32_t p, int shift) -> int {
		return static_cast<int>((p >> shift) & 0xffu);
	};

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int sumR = 0;
			int sumG = 0;
			int sumB = 0;
			int count = 0;
			for (int dy = -1; dy <= 1; ++dy) {
				const int sy = std::clamp(y + dy, 0, height - 1);
				for (int dx = -1; dx <= 1; ++dx) {
					const int sx = std::clamp(x + dx, 0, width - 1);
					const std::uint32_t sp = inputPixels[idx(sx, sy)];
					sumR += unpack(sp, 0);
					sumG += unpack(sp, 8);
					sumB += unpack(sp, 16);
					++count;
				}
			}
			const std::uint32_t center = inputPixels[idx(x, y)];
			const std::uint32_t a = (center >> 24) & 0xffu;
			const std::uint32_t r = static_cast<std::uint32_t>(sumR / count) & 0xffu;
			const std::uint32_t g = static_cast<std::uint32_t>(sumG / count) & 0xffu;
			const std::uint32_t b = static_cast<std::uint32_t>(sumB / count) & 0xffu;
			outputPixels[idx(x, y)] = (a << 24) | (b << 16) | (g << 8) | r;
		}
	}
	return true;
}

#endif
