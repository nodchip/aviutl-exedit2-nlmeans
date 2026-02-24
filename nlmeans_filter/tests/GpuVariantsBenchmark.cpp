// GPU 亜種パラメータ（空間間引き/時間減衰）の処理時間比較を Markdown で出力するベンチマーク。
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>
#include "../exedit2/Exedit2GpuRunner.h"

namespace {

struct VariantCase
{
	const char* name;
	int spatialStep;
	double temporalDecay;
};

std::vector<PIXEL_RGBA> make_frame(int width, int height, int seed)
{
	std::vector<PIXEL_RGBA> frame(static_cast<size_t>(width) * static_cast<size_t>(height));
	for (size_t i = 0; i < frame.size(); ++i) {
		const int base = static_cast<int>((i * 53 + static_cast<size_t>(seed) * 17) & 255);
		frame[i] = PIXEL_RGBA {
			static_cast<unsigned char>(base),
			static_cast<unsigned char>((base * 3 + 11) & 255),
			static_cast<unsigned char>((base * 7 + 19) & 255),
			255
		};
	}
	return frame;
}

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
	return elapsed.count() / static_cast<double>(std::max(1, iterations));
}

} // namespace

int main()
{
	const int width = 128;
	const int height = 72;
	const int searchRadius = 2;
	const int timeRadius = 1;
	const double sigma = 55.0;
	const int iterations = 12;

	const std::vector<VariantCase> variants = {
		{ "Baseline", 1, 0.0 },
		{ "Fast(step=2)", 2, 0.0 },
		{ "Fast(step=3)", 3, 0.0 },
		{ "Temporal(decay=1.0)", 1, 1.0 },
		{ "Fast(step=2)+Temporal(decay=1.0)", 2, 1.0 }
	};

	const std::vector<PIXEL_RGBA> frame = make_frame(width, height, 1);
	std::vector<PIXEL_RGBA> output(static_cast<size_t>(width) * static_cast<size_t>(height));
	std::vector<double> timings(variants.size(), -1.0);

	for (size_t i = 0; i < variants.size(); ++i) {
		Exedit2GpuRunner runner;
		if (!runner.initialize(-1)) {
			std::cerr << "GPU initialization failed at variant index " << i << std::endl;
			return 1;
		}

		const VariantCase& variant = variants[i];
		// 初回実行に含まれるシェーダー生成やバッファ確保のコストを除外する。
		if (!runner.process(
			frame.data(),
			output.data(),
			width,
			height,
			searchRadius,
			timeRadius,
			sigma,
			variant.spatialStep,
			variant.temporalDecay)) {
			std::cerr << "Warmup failed at variant index " << i << std::endl;
			return 1;
		}

		timings[i] = benchmark_ms([&]() {
			return runner.process(
				frame.data(),
				output.data(),
				width,
				height,
				searchRadius,
				timeRadius,
				sigma,
				variant.spatialStep,
				variant.temporalDecay);
		}, iterations);
		if (timings[i] <= 0.0) {
			std::cerr << "Benchmark failed at variant index " << i << std::endl;
			return 1;
		}
	}

	const double baseline = timings.front();

	std::cout << "# GPU Variants Benchmark\n\n";
	std::cout << "- Frame: " << width << "x" << height << "\n";
	std::cout << "- Search Radius: " << searchRadius << "\n";
	std::cout << "- Time Radius: " << timeRadius << "\n";
	std::cout << "- Iterations: " << iterations << "\n\n";
	std::cout << "- Warmup: 1 run per variant\n\n";
	std::cout << "| Variant | Spatial Step | Temporal Decay | Mean Time (ms/frame) | Relative to Baseline |\n";
	std::cout << "|---|---:|---:|---:|---:|\n";
	std::cout << std::fixed << std::setprecision(3);
	for (size_t i = 0; i < variants.size(); ++i) {
		const double relative = baseline > 0.0 ? (timings[i] / baseline) : 0.0;
		std::cout
			<< "| " << variants[i].name
			<< " | " << variants[i].spatialStep
			<< " | " << variants[i].temporalDecay
			<< " | " << timings[i]
			<< " | " << relative << "x |\n";
	}

	return 0;
}
