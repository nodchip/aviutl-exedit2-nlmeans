// DX11 と DX12 PoC 出力差分の品質比較レポートを Markdown で出力する。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include "../exedit2/Dx12PocProcessor.h"
#include "../exedit2/Exedit2GpuRunner.h"

namespace {

struct QualityStats
{
	int maxAbsDiff;
	double meanAbsDiff;
};

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

std::vector<PIXEL_RGBA> from_u32(const std::vector<std::uint32_t>& packed)
{
	std::vector<PIXEL_RGBA> pixels(packed.size());
	for (size_t i = 0; i < packed.size(); ++i) {
		const std::uint32_t v = packed[i];
		pixels[i] = PIXEL_RGBA {
			static_cast<unsigned char>(v & 0xffu),
			static_cast<unsigned char>((v >> 8) & 0xffu),
			static_cast<unsigned char>((v >> 16) & 0xffu),
			static_cast<unsigned char>((v >> 24) & 0xffu)
		};
	}
	return pixels;
}

QualityStats evaluate_quality(const std::vector<PIXEL_RGBA>& lhs, const std::vector<PIXEL_RGBA>& rhs)
{
	const size_t pixelCount = lhs.size();
	long long totalAbs = 0;
	int maxAbs = 0;
	for (size_t i = 0; i < pixelCount; ++i) {
		const int dr = std::abs(static_cast<int>(lhs[i].r) - static_cast<int>(rhs[i].r));
		const int dg = std::abs(static_cast<int>(lhs[i].g) - static_cast<int>(rhs[i].g));
		const int db = std::abs(static_cast<int>(lhs[i].b) - static_cast<int>(rhs[i].b));
		maxAbs = std::max(maxAbs, std::max(dr, std::max(dg, db)));
		totalAbs += static_cast<long long>(dr + dg + db);
	}
	const double denom = static_cast<double>(pixelCount) * 3.0;
	return { maxAbs, denom > 0.0 ? (static_cast<double>(totalAbs) / denom) : 0.0 };
}

}

int main()
{
	const int width = 64;
	const int height = 48;
	const int searchRadius = 2;
	const int timeRadius = 0;
	const double sigma = 55.0;

	const std::vector<PIXEL_RGBA> input = make_frame(width, height, 7);
	std::vector<PIXEL_RGBA> dx11Out(input.size());

	Exedit2GpuRunner dx11Runner;
	if (!dx11Runner.initialize(-1)) {
		std::cout << "# DX11 vs DX12 PoC Quality Comparison\n\n";
		std::cout << "- SKIPPED: DX11 runner initialization failed.\n";
		return 0;
	}
	if (!dx11Runner.process(
		input.data(),
		dx11Out.data(),
		width,
		height,
		searchRadius,
		timeRadius,
		sigma,
		1,
		0.0)) {
		std::cout << "# DX11 vs DX12 PoC Quality Comparison\n\n";
		std::cout << "- SKIPPED: DX11 processing failed.\n";
		return 0;
	}

	Dx12PocProbeResult probe = {};
	probe.enabled = true;
	const std::vector<std::uint32_t> inputPacked = to_u32(input);
	std::vector<std::uint32_t> dx12CopyPacked(inputPacked.size(), 0u);
	std::vector<std::uint32_t> dx12ComputePacked(inputPacked.size(), 0u);
	if (!process_dx12_poc_single_frame(
		inputPacked.data(),
		dx12CopyPacked.data(),
		width,
		height,
		true,
		probe)) {
		std::cout << "# DX11 vs DX12 PoC Quality Comparison\n\n";
		std::cout << "- SKIPPED: DX12 PoC copy path failed.\n";
		return 0;
	}
	if (!process_dx12_poc_compute_path(
		inputPacked.data(),
		dx12ComputePacked.data(),
		width,
		height,
		true,
		probe)) {
		std::cout << "# DX11 vs DX12 PoC Quality Comparison\n\n";
		std::cout << "- SKIPPED: DX12 PoC compute path failed.\n";
		return 0;
	}
	const std::vector<PIXEL_RGBA> dx12CopyOut = from_u32(dx12CopyPacked);
	const std::vector<PIXEL_RGBA> dx12ComputeOut = from_u32(dx12ComputePacked);

	const QualityStats dx11VsDx12Copy = evaluate_quality(dx11Out, dx12CopyOut);
	const QualityStats dx11VsDx12Compute = evaluate_quality(dx11Out, dx12ComputeOut);
	const QualityStats dx12CopyVsInput = evaluate_quality(dx12CopyOut, input);
	const QualityStats dx12ComputeVsInput = evaluate_quality(dx12ComputeOut, input);

	std::cout << "# DX11 vs DX12 PoC Quality Comparison\n\n";
	std::cout << "- Frame: " << width << "x" << height << "\n";
	std::cout << "- DX11 params: search=" << searchRadius << ", time=" << timeRadius << ", sigma=" << sigma << "\n\n";
	std::cout << "| Pair | Max Abs Diff | Mean Abs Diff |\n";
	std::cout << "|---|---:|---:|\n";
	std::cout << "| DX11 vs DX12 PoC (copy path) | " << dx11VsDx12Copy.maxAbsDiff << " | " << dx11VsDx12Copy.meanAbsDiff << " |\n";
	std::cout << "| DX11 vs DX12 PoC (compute path) | " << dx11VsDx12Compute.maxAbsDiff << " | " << dx11VsDx12Compute.meanAbsDiff << " |\n";
	std::cout << "| DX12 PoC copy path vs Input | " << dx12CopyVsInput.maxAbsDiff << " | " << dx12CopyVsInput.meanAbsDiff << " |\n";
	std::cout << "| DX12 PoC compute path vs Input | " << dx12ComputeVsInput.maxAbsDiff << " | " << dx12ComputeVsInput.meanAbsDiff << " |\n";
	return 0;
}
