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
		"	(void)tid;\n"
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

// DX12 PoC の最小コンピュート相当処理。
// 3x3 の近傍平均を取り、copy path よりも計算経路に近い出力を作る。
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

	// TODO: D3D12 compute 実行本体は次段で実装する。
	// 先にデバイス初期化/シェーダーコンパイル/コマンド生成/パイプライン生成/バッファ生成可否を実測し、失敗時は既存の CPU 経路へフォールバックする。
	(void)try_create_dx12_device_for_poc();
	(void)try_compile_dx12_poc_shader_for_poc();
	(void)try_create_dx12_command_objects_for_poc();
	(void)try_create_dx12_compute_pipeline_for_poc();
	(void)try_create_dx12_buffer_resources_for_poc();

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
