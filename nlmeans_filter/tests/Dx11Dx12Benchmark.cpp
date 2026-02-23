// DX11 と DX12 PoC の処理時間比較ベンチマークを Markdown で出力する。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
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

}

int main()
{
	const int width = 1280;
	const int height = 720;
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
	std::cout << "| D3D12CreateDevice | " << dx12DeviceMs << " |\n";
	std::cout << "| D3DCompile (cs_5_0) | " << dx12ShaderCompileMs << " |\n";
	std::cout << "| CreateComputePipelineState | " << dx12PipelineMs << " |\n";
	std::cout << "| Dispatch IO Roundtrip | " << dx12DispatchRoundtripMs << " |\n";
	std::cout << "| Fullframe 3x3 Compute | " << dx12FullframeComputeMs << " |\n";
	return 0;
}
