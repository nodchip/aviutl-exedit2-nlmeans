// Naive / AVX2 の基礎カーネル比較ベンチマーク。
#include <chrono>
#include <cstdio>
#include <vector>
#include "NlmFrameKernel.h"

static double measure_naive(
	const short* input,
	int width,
	int height,
	int frames,
	int iterations)
{
	volatile int sink = 0;
	const auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < iterations; ++i) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				sink += nlm_filter_pixel_channel_naive(
					input, width, height, frames, x, y, 0, 1, 1, 1, 1.0 / (25.0 * 25.0));
			}
		}
	}
	const auto end = std::chrono::high_resolution_clock::now();
	(void)sink;
	return std::chrono::duration<double, std::milli>(end - begin).count();
}

static double measure_avx2(
	const short* input,
	int width,
	int height,
	int frames,
	int iterations)
{
	volatile int sink = 0;
	const auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < iterations; ++i) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				sink += nlm_filter_pixel_channel_avx2(
					input, width, height, frames, x, y, 0, 1, 1, 1, 1.0 / (25.0 * 25.0));
			}
		}
	}
	const auto end = std::chrono::high_resolution_clock::now();
	(void)sink;
	return std::chrono::duration<double, std::milli>(end - begin).count();
}

int main()
{
	const int width = 64;
	const int height = 64;
	const int frames = 3;
	const int iterations = 2;

	std::vector<short> input(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(frames) * 3u);
	for (size_t i = 0; i < input.size(); ++i) {
		input[i] = static_cast<short>((i * 31 + 7) % 1024 - 512);
	}

	const double naiveMs = measure_naive(input.data(), width, height, frames, iterations);
	std::printf("Naive: %.3f ms\n", naiveMs);

	if (!is_avx2_supported_runtime()) {
		std::printf("AVX2: unsupported on this machine\n");
		return 0;
	}

	const double avx2Ms = measure_avx2(input.data(), width, height, frames, iterations);
	std::printf("AVX2 : %.3f ms\n", avx2Ms);
	if (avx2Ms > 0.0) {
		std::printf("Speedup: %.2f x\n", naiveMs / avx2Ms);
	}
	return 0;
}
