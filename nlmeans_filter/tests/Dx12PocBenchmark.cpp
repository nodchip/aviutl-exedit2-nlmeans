// DX12 PoC 最小単一フレーム経路のベンチマークを Markdown で出力する。
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
#include "../exedit2/Dx12PocProcessor.h"

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

}

int main()
{
	const int width = 1920;
	const int height = 1080;
	const int iterations = 60;
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

	std::vector<std::uint32_t> src(pixelCount);
	std::vector<std::uint32_t> dst(pixelCount, 0u);
	for (size_t i = 0; i < pixelCount; ++i) {
		src[i] = static_cast<std::uint32_t>((i * 1103515245u + 12345u) & 0xffffffffu);
	}

	Dx12PocProbeResult probe = {};
	probe.enabled = true;
	const double ms = benchmark_ms([&]() {
		return process_dx12_poc_single_frame(src.data(), dst.data(), width, height, true, probe);
	}, iterations);

	std::cout << "# DX12 PoC Benchmark\n\n";
	std::cout << "- Frame: " << width << "x" << height << "\n";
	std::cout << "- Iterations: " << iterations << "\n\n";
	std::cout << "| Mode | Mean Time (ms/frame) |\n";
	std::cout << "|---|---:|\n";
	std::cout << std::fixed << std::setprecision(3);
	std::cout << "| DX12 PoC Single Frame (copy path) | " << ms << " |\n";
	return 0;
}
