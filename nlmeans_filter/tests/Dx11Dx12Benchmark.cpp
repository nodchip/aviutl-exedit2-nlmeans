// DX11 と DX12 PoC の処理時間比較ベンチマークを Markdown で出力する。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../exedit2/Dx12PocProcessor.h"
#include "../exedit2/Exedit2GpuRunner.h"

namespace {

template <typename Fn>
double benchmark_ms(Fn&& fn, int iterations)
{
	const auto begin = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i) {
		if (!fn()) {
			return -1.0;
		}
	}
	const auto end = std::chrono::steady_clock::now();
	const std::chrono::duration<double, std::milli> elapsed = end - begin;
	return elapsed.count() / static_cast<double>(iterations);
}

std::vector<PIXEL_RGBA> make_frame(int width, int height, int seed)
{
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	std::vector<PIXEL_RGBA> frame(pixelCount);
	for (size_t i = 0; i < pixelCount; ++i) {
		const int base = static_cast<int>((i * 73 + static_cast<size_t>(seed) * 19) & 255);
		frame[i] = PIXEL_RGBA {
			static_cast<unsigned char>(base),
			static_cast<unsigned char>((base * 3 + 11) & 255),
			static_cast<unsigned char>((base * 5 + 17) & 255),
			255
		};
	}
	return frame;
}

std::vector<std::uint32_t> to_u32(const std::vector<PIXEL_RGBA>& pixels)
{
	std::vector<std::uint32_t> packed(pixels.size(), 0u);
	for (size_t i = 0; i < pixels.size(); ++i) {
		packed[i] =
			static_cast<std::uint32_t>(pixels[i].r) |
			(static_cast<std::uint32_t>(pixels[i].g) << 8) |
			(static_cast<std::uint32_t>(pixels[i].b) << 16) |
			(static_cast<std::uint32_t>(pixels[i].a) << 24);
	}
	return packed;
}

struct Dx12Diagnostics
{
	HRESULT loadD3d12Hr;
	HRESULT loadCompilerHr;
	HRESULT procCreateDeviceHr;
	HRESULT procSerializeRootSigHr;
	HRESULT procCompileHr;
	HRESULT createDeviceHr;
	HRESULT compileShaderHr;
	HRESULT serializeRootSignatureHr;
	HRESULT createRootSignatureHr;
	HRESULT createComputePsoHr;
	std::string compileError;
	std::string rootSignatureError;
};

std::string hr_to_string(HRESULT hr)
{
	std::ostringstream oss;
	oss << "0x" << std::uppercase << std::hex << static_cast<std::uint32_t>(hr);
	return oss.str();
}

Dx12Diagnostics collect_dx12_diagnostics()
{
	Dx12Diagnostics d = {};
	d.loadD3d12Hr = E_FAIL;
	d.loadCompilerHr = E_FAIL;
	d.procCreateDeviceHr = E_FAIL;
	d.procSerializeRootSigHr = E_FAIL;
	d.procCompileHr = E_FAIL;
	d.createDeviceHr = E_FAIL;
	d.compileShaderHr = E_FAIL;
	d.serializeRootSignatureHr = E_FAIL;
	d.createRootSignatureHr = E_FAIL;
	d.createComputePsoHr = E_FAIL;

	HMODULE d3d12Module = ::LoadLibraryW(L"d3d12.dll");
	HMODULE compilerModule = ::LoadLibraryW(L"d3dcompiler_47.dll");
	if (d3d12Module != nullptr) {
		d.loadD3d12Hr = S_OK;
	}
	if (compilerModule != nullptr) {
		d.loadCompilerHr = S_OK;
	}
	if (d3d12Module == nullptr || compilerModule == nullptr) {
		if (compilerModule != nullptr) {
			::FreeLibrary(compilerModule);
		}
		if (d3d12Module != nullptr) {
			::FreeLibrary(d3d12Module);
		}
		return d;
	}

	using Dx12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
	using Dx12SerializeRootSignatureFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
	using D3DCompileFn = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

	const Dx12CreateDeviceFn createDeviceFn =
		reinterpret_cast<Dx12CreateDeviceFn>(::GetProcAddress(d3d12Module, "D3D12CreateDevice"));
	const Dx12SerializeRootSignatureFn serializeRootSignatureFn =
		reinterpret_cast<Dx12SerializeRootSignatureFn>(::GetProcAddress(d3d12Module, "D3D12SerializeRootSignature"));
	const D3DCompileFn compileFn =
		reinterpret_cast<D3DCompileFn>(::GetProcAddress(compilerModule, "D3DCompile"));
	if (createDeviceFn != nullptr) {
		d.procCreateDeviceHr = S_OK;
	}
	if (serializeRootSignatureFn != nullptr) {
		d.procSerializeRootSigHr = S_OK;
	}
	if (compileFn != nullptr) {
		d.procCompileHr = S_OK;
	}
	if (createDeviceFn == nullptr || serializeRootSignatureFn == nullptr || compileFn == nullptr) {
		::FreeLibrary(compilerModule);
		::FreeLibrary(d3d12Module);
		return d;
	}

	ID3D12Device* device = nullptr;
	d.createDeviceHr = createDeviceFn(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		__uuidof(ID3D12Device),
		reinterpret_cast<void**>(&device));
	if (FAILED(d.createDeviceHr) || device == nullptr) {
		if (device != nullptr) {
			device->Release();
		}
		::FreeLibrary(compilerModule);
		::FreeLibrary(d3d12Module);
		return d;
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
	ID3DBlob* shaderErr = nullptr;
	d.compileShaderHr = compileFn(
		shaderSource,
		std::strlen(shaderSource),
		"Dx12Diag.hlsl",
		nullptr,
		nullptr,
		"main",
		"cs_5_0",
		0,
		0,
		&shaderBlob,
		&shaderErr);
	if (shaderErr != nullptr && shaderErr->GetBufferPointer() != nullptr) {
		d.compileError.assign(
			static_cast<const char*>(shaderErr->GetBufferPointer()),
			shaderErr->GetBufferSize());
	}
	if (shaderErr != nullptr) {
		shaderErr->Release();
	}
	if (FAILED(d.compileShaderHr) || shaderBlob == nullptr) {
		if (shaderBlob != nullptr) {
			shaderBlob->Release();
		}
		device->Release();
		::FreeLibrary(compilerModule);
		::FreeLibrary(d3d12Module);
		return d;
	}

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = 0;
	rootDesc.pParameters = nullptr;
	rootDesc.NumStaticSamplers = 0;
	rootDesc.pStaticSamplers = nullptr;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	ID3DBlob* rootSigBlob = nullptr;
	ID3DBlob* rootSigErr = nullptr;
	d.serializeRootSignatureHr = serializeRootSignatureFn(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&rootSigBlob,
		&rootSigErr);
	if (rootSigErr != nullptr && rootSigErr->GetBufferPointer() != nullptr) {
		d.rootSignatureError.assign(
			static_cast<const char*>(rootSigErr->GetBufferPointer()),
			rootSigErr->GetBufferSize());
	}
	if (rootSigErr != nullptr) {
		rootSigErr->Release();
	}
	if (FAILED(d.serializeRootSignatureHr) || rootSigBlob == nullptr) {
		if (rootSigBlob != nullptr) {
			rootSigBlob->Release();
		}
		shaderBlob->Release();
		device->Release();
		::FreeLibrary(compilerModule);
		::FreeLibrary(d3d12Module);
		return d;
	}

	ID3D12RootSignature* rootSignature = nullptr;
	d.createRootSignatureHr = device->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		__uuidof(ID3D12RootSignature),
		reinterpret_cast<void**>(&rootSignature));
	rootSigBlob->Release();
	if (FAILED(d.createRootSignatureHr) || rootSignature == nullptr) {
		if (rootSignature != nullptr) {
			rootSignature->Release();
		}
		shaderBlob->Release();
		device->Release();
		::FreeLibrary(compilerModule);
		::FreeLibrary(d3d12Module);
		return d;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
	psoDesc.CS.BytecodeLength = shaderBlob->GetBufferSize();
	ID3D12PipelineState* pipelineState = nullptr;
	d.createComputePsoHr = device->CreateComputePipelineState(
		&psoDesc,
		__uuidof(ID3D12PipelineState),
		reinterpret_cast<void**>(&pipelineState));
	if (pipelineState != nullptr) {
		pipelineState->Release();
	}
	rootSignature->Release();
	shaderBlob->Release();
	device->Release();
	::FreeLibrary(compilerModule);
	::FreeLibrary(d3d12Module);
	return d;
}

}

int main()
{
	const int width = 1920;
	const int height = 1080;
	const int iterations = 20;
	const int breakdownIterations = 10;
	const int searchRadius = 2;
	const int timeRadius = 0;
	const double sigma = 55.0;

	const std::vector<PIXEL_RGBA> input = make_frame(width, height, 11);
	std::vector<PIXEL_RGBA> dx11Out(input.size());

	Exedit2GpuRunner dx11Runner;
	double dx11Ms = -1.0;
	if (dx11Runner.initialize(-1)) {
		dx11Ms = benchmark_ms([&]() {
			return dx11Runner.process(
				input.data(),
				dx11Out.data(),
				width,
				height,
				searchRadius,
				timeRadius,
				sigma,
				1,
				0.0);
		}, iterations);
	}

	const std::vector<std::uint32_t> inputPacked = to_u32(input);
	std::vector<std::uint32_t> dx12CopyOut(inputPacked.size(), 0u);
	std::vector<std::uint32_t> dx12ComputeOut(inputPacked.size(), 0u);
	Dx12PocProbeResult probe = {};
	probe.enabled = true;
	const double dx12CopyMs = benchmark_ms([&]() {
		return process_dx12_poc_single_frame(
			inputPacked.data(),
			dx12CopyOut.data(),
			width,
			height,
			true,
			probe);
	}, iterations);
	const double dx12ComputeMs = benchmark_ms([&]() {
		return process_dx12_poc_compute_path(
			inputPacked.data(),
			dx12ComputeOut.data(),
			width,
			height,
			true,
			probe);
	}, iterations);
	const double dx12DeviceMs = benchmark_ms([&]() {
		return try_create_dx12_device_for_poc();
	}, breakdownIterations);
	const double dx12ShaderCompileMs = benchmark_ms([&]() {
		return try_compile_dx12_poc_shader_for_poc();
	}, breakdownIterations);
	const double dx12PipelineMs = benchmark_ms([&]() {
		return try_create_dx12_compute_pipeline_for_poc();
	}, breakdownIterations);
	const double dx12DispatchRoundtripMs = benchmark_ms([&]() {
		return try_execute_dx12_dispatch_with_io_roundtrip_for_poc();
	}, breakdownIterations);
	const double dx12FullframeComputeMs = benchmark_ms([&]() {
		return try_execute_dx12_fullframe_compute_for_poc(
			inputPacked.data(),
			dx12ComputeOut.data(),
			width,
			height);
	}, breakdownIterations);
	const Dx12Diagnostics dx12Diag = collect_dx12_diagnostics();

	std::cout << "# DX11 vs DX12 Benchmark\n\n";
	std::cout << "- Frame: " << width << "x" << height << "\n";
	std::cout << "- Iterations: " << iterations << "\n";
	std::cout << "- DX12 breakdown iterations: " << breakdownIterations << "\n";
	std::cout << "- DX11 params: search=" << searchRadius << ", time=" << timeRadius << ", sigma=" << sigma << "\n\n";
	std::cout << "| Mode | Mean Time (ms/frame) |\n";
	std::cout << "|---|---:|\n";
	std::cout << std::fixed << std::setprecision(3);
	if (dx11Ms < 0.0) {
		std::cout << "| DX11 Runner | N/A |\n";
	} else {
		std::cout << "| DX11 Runner | " << dx11Ms << " |\n";
	}
	std::cout << "| DX12 PoC Single Frame (copy path) | " << dx12CopyMs << " |\n";
	std::cout << "| DX12 PoC Single Frame (compute path) | " << dx12ComputeMs << " |\n";
	std::cout << "\n## DX12 Breakdown\n\n";
	std::cout << "| Step | Mean Time (ms/call) |\n";
	std::cout << "|---|---:|\n";
	auto print_breakdown = [](const char* label, double value) {
		if (value < 0.0) {
			std::cout << "| " << label << " | N/A |\n";
		} else {
			std::cout << "| " << label << " | " << value << " |\n";
		}
	};
	print_breakdown("D3D12CreateDevice", dx12DeviceMs);
	print_breakdown("D3DCompile (cs_5_0)", dx12ShaderCompileMs);
	print_breakdown("CreateComputePipelineState", dx12PipelineMs);
	print_breakdown("Dispatch IO Roundtrip", dx12DispatchRoundtripMs);
	print_breakdown("Fullframe 3x3 Compute", dx12FullframeComputeMs);
	std::cout << "\n## DX12 HRESULT Diagnostics\n\n";
	std::cout << "| Step | HRESULT |\n";
	std::cout << "|---|---|\n";
	std::cout << "| LoadLibrary(d3d12.dll) | " << hr_to_string(dx12Diag.loadD3d12Hr) << " |\n";
	std::cout << "| LoadLibrary(d3dcompiler_47.dll) | " << hr_to_string(dx12Diag.loadCompilerHr) << " |\n";
	std::cout << "| GetProcAddress(D3D12CreateDevice) | " << hr_to_string(dx12Diag.procCreateDeviceHr) << " |\n";
	std::cout << "| GetProcAddress(D3D12SerializeRootSignature) | " << hr_to_string(dx12Diag.procSerializeRootSigHr) << " |\n";
	std::cout << "| GetProcAddress(D3DCompile) | " << hr_to_string(dx12Diag.procCompileHr) << " |\n";
	std::cout << "| D3D12CreateDevice | " << hr_to_string(dx12Diag.createDeviceHr) << " |\n";
	std::cout << "| D3DCompile(cs_5_0) | " << hr_to_string(dx12Diag.compileShaderHr) << " |\n";
	std::cout << "| D3D12SerializeRootSignature | " << hr_to_string(dx12Diag.serializeRootSignatureHr) << " |\n";
	std::cout << "| CreateRootSignature | " << hr_to_string(dx12Diag.createRootSignatureHr) << " |\n";
	std::cout << "| CreateComputePipelineState | " << hr_to_string(dx12Diag.createComputePsoHr) << " |\n";
	std::cout << "\n## DX12 Diagnostic Messages\n\n";
	std::cout << "- D3DCompile: ";
	if (dx12Diag.compileError.empty()) {
		std::cout << "(none)\n";
	} else {
		std::cout << dx12Diag.compileError << "\n";
	}
	std::cout << "- D3D12SerializeRootSignature: ";
	if (dx12Diag.rootSignatureError.empty()) {
		std::cout << "(none)\n";
	} else {
		std::cout << dx12Diag.rootSignatureError << "\n";
	}
	return 0;
}
