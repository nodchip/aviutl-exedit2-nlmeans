// NLM 亜種の処理時間比較を Markdown で出力するベンチマーク。
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "../NlmFrameKernel.h"
#include "../algorithms/FastNlm.h"
#include "../algorithms/TemporalNlm.h"

namespace {

std::vector<short> make_input(int width, int height, int frameCount)
{
	std::vector<short> input(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(frameCount) * 3);
	for (size_t i = 0; i < input.size(); ++i) {
		input[i] = static_cast<short>((i * 29 + 7) % 256);
	}
	return input;
}

template <typename Fn>
double benchmark_ms(Fn&& fn, int iterations)
{
	const auto begin = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i) {
		fn();
	}
	const auto end = std::chrono::steady_clock::now();
	const std::chrono::duration<double, std::milli> elapsed = end - begin;
	return elapsed.count() / static_cast<double>(std::max(1, iterations));
}

}

int main()
{
	const int width = 64;
	const int height = 36;
	const int frameCount = 3;
	const int currentFrame = 2;
	const int searchRadius = 2;
	const int timeRadius = 1;
	const double h2 = 0.0002;
	const int iterations = 8;

	std::vector<short> input = make_input(width, height, frameCount);
	std::vector<short> output(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);

	const double naiveMs = benchmark_ms([&]() {
		nlm_filter_frame_naive(
			input.data(),
			width,
			height,
			frameCount,
			currentFrame,
			searchRadius,
			timeRadius,
			h2,
			output.data());
	}, iterations);

	const double fastMs = benchmark_ms([&]() {
		nlm_filter_frame_fast_naive(
			input.data(),
			width,
			height,
			frameCount,
			currentFrame,
			searchRadius,
			timeRadius,
			h2,
			2,
			output.data());
	}, iterations);

	const double temporalMs = benchmark_ms([&]() {
		nlm_filter_frame_temporal_naive(
			input.data(),
			width,
			height,
			frameCount,
			currentFrame,
			searchRadius,
			timeRadius,
			h2,
			1.0,
			output.data());
	}, iterations);

	std::cout << "# NLM Variants Benchmark\n\n";
	std::cout << "- Frame: " << width << "x" << height << "\n";
	std::cout << "- Search Radius: " << searchRadius << "\n";
	std::cout << "- Time Radius: " << timeRadius << "\n";
	std::cout << "- Iterations: " << iterations << "\n\n";
	std::cout << "| Variant | Mean Time (ms/frame) |\n";
	std::cout << "|---|---:|\n";
	std::cout << std::fixed << std::setprecision(3);
	std::cout << "| Naive | " << naiveMs << " |\n";
	std::cout << "| Fast(step=2) | " << fastMs << " |\n";
	std::cout << "| Temporal(decay=1.0) | " << temporalMs << " |\n";
	return 0;
}
