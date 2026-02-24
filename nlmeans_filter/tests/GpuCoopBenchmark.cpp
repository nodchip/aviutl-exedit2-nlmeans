// GPU 協調 PoC の処理時間比較を Markdown で出力するベンチマーク。
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include "../exedit2/Exedit2GpuRunner.h"
#include "../exedit2/GpuCoopExecution.h"
#include "../exedit2/MultiGpuCompose.h"
#include "../exedit2/MultiGpuTiling.h"

namespace {

struct BenchmarkProfile
{
	const char* key;
	int width;
	int height;
	int iterations;
};

struct BenchmarkResult
{
	const char* key;
	int width;
	int height;
	int iterations;
	double singleMs;
	double coopSeqMs;
	double coopAsyncMs;
	double coopAsyncFallbackMs;
};

std::vector<PIXEL_RGBA> make_frame(int width, int height, int seed)
{
	std::vector<PIXEL_RGBA> frame(static_cast<size_t>(width) * static_cast<size_t>(height));
	for (size_t i = 0; i < frame.size(); ++i) {
		const int base = static_cast<int>((i * 41 + static_cast<size_t>(seed) * 13) & 255);
		frame[i] = PIXEL_RGBA {
			static_cast<unsigned char>(base),
			static_cast<unsigned char>((base * 3 + 19) & 255),
			static_cast<unsigned char>((base * 5 + 7) & 255),
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
	return elapsed.count() / static_cast<double>(iterations);
}

double safe_ratio(double numerator, double denominator)
{
	if (numerator <= 0.0 || denominator <= 0.0) {
		return -1.0;
	}
	return numerator / denominator;
}

bool run_profile(const BenchmarkProfile& profile, BenchmarkResult* outResult)
{
	if (outResult == nullptr) {
		return false;
	}

	const int search_radius = 2;
	const int time_radius = 0;
	const double sigma = 55.0;
	const int spatial_step = 1;
	const double temporal_decay = 0.0;

	const int width = profile.width;
	const int height = profile.height;
	const int iterations = profile.iterations;

	const std::vector<PIXEL_RGBA> frame = make_frame(width, height, 1);
	std::vector<PIXEL_RGBA> full_out(static_cast<size_t>(width) * static_cast<size_t>(height));
	std::vector<PIXEL_RGBA> composed(static_cast<size_t>(width) * static_cast<size_t>(height));

	Exedit2GpuRunner full_runner;
	if (!full_runner.initialize(-1)) {
		return false;
	}

	const double full_ms = benchmark_ms([&]() {
		return full_runner.process(
			frame.data(),
			full_out.data(),
			width,
			height,
			search_radius,
			time_radius,
			sigma,
			spatial_step,
			temporal_decay);
	}, iterations);
	if (full_ms <= 0.0) {
		return false;
	}

	const std::vector<GpuRowTile> tiles = plan_gpu_row_tiles(height, 2, 8);
	if (tiles.size() != 2) {
		return false;
	}

	std::vector<std::unique_ptr<Exedit2GpuRunner>> tile_runners(2);
	std::vector<std::vector<PIXEL_RGBA>> tile_outputs(2);
	for (size_t i = 0; i < 2; ++i) {
		tile_runners[i] = std::unique_ptr<Exedit2GpuRunner>(new Exedit2GpuRunner());
		if (!tile_runners[i]->initialize(-1)) {
			return false;
		}
		tile_outputs[i].resize(static_cast<size_t>(width) * static_cast<size_t>(height));
	}

	auto run_coop = [&](bool enableAsync, bool forceSingleFallback) {
		Exedit2GpuRunner single_fallback_runner;
		if (forceSingleFallback && !single_fallback_runner.initialize(-1)) {
			return -1.0;
		}
		return benchmark_ms([&]() {
			bool usedSingleGpu = false;
			return execute_gpu_coop_with_recovery(
				tiles,
				true,
				2,
				[&](size_t tileIndex, const GpuRowTile& tile) {
					if (forceSingleFallback && tileIndex == 1) {
						return false;
					}
					return tile_runners[tile.adapterIndex]->process(
						frame.data(),
						tile_outputs[tile.adapterIndex].data(),
						width,
						height,
						search_radius,
						time_radius,
						sigma,
						spatial_step,
						temporal_decay,
						tile.yBegin,
						tile.yEnd);
				},
				[&](size_t, const GpuRowTile&) {
					return false;
				},
				[&]() {
					return single_fallback_runner.process(
						frame.data(),
						full_out.data(),
						width,
						height,
						search_radius,
						time_radius,
						sigma,
						spatial_step,
						temporal_decay);
				},
				[&]() {
					return compose_row_tiled_output(width, height, tiles, tile_outputs, &composed);
				},
				&usedSingleGpu,
				enableAsync);
		}, iterations);
	};

	const double tiled_seq_ms = run_coop(false, false);
	const double tiled_async_ms = run_coop(true, false);
	const double tiled_async_fallback_ms = run_coop(true, true);
	if (tiled_seq_ms <= 0.0 || tiled_async_ms <= 0.0 || tiled_async_fallback_ms <= 0.0) {
		return false;
	}

	outResult->key = profile.key;
	outResult->width = width;
	outResult->height = height;
	outResult->iterations = iterations;
	outResult->singleMs = full_ms;
	outResult->coopSeqMs = tiled_seq_ms;
	outResult->coopAsyncMs = tiled_async_ms;
	outResult->coopAsyncFallbackMs = tiled_async_fallback_ms;
	return true;
}

}

int main()
{
	const BenchmarkProfile profiles[] = {
		{ "smoke", 128, 72, 12 },
		{ "hd", 1280, 720, 6 },
		{ "fhd", 1920, 1080, 4 }
	};

	std::vector<BenchmarkResult> results;
	results.reserve(sizeof(profiles) / sizeof(profiles[0]));
	for (const BenchmarkProfile& profile : profiles) {
		BenchmarkResult result {};
		if (!run_profile(profile, &result)) {
			std::cerr << "Benchmark failed for profile=" << profile.key << std::endl;
			return 1;
		}
		results.push_back(result);
	}

	std::cout << "# GPU Coop PoC Benchmark\n\n";
	std::cout << "- Search Radius: 2\n";
	std::cout << "- Tile Count: 2\n";
	std::cout << "- Notes: adoption gate should use real-size profile (`fhd`) instead of smoke size.\n\n";
	std::cout << "| Profile | Frame | Iterations | Single GPU Full Frame | Coop PoC (2 tiles, sequential dispatch) | Coop PoC (2 tiles, async dispatch) | Coop PoC (2 tiles, async + single fallback) | Relative (CoopSeq/Single) | Relative (CoopAsync/Single) | Relative (CoopAsyncFallback/Single) |\n";
	std::cout << "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|\n";
	std::cout << std::fixed << std::setprecision(3);
	for (const BenchmarkResult& result : results) {
		const double seqRatio = safe_ratio(result.coopSeqMs, result.singleMs);
		const double asyncRatio = safe_ratio(result.coopAsyncMs, result.singleMs);
		const double fallbackRatio = safe_ratio(result.coopAsyncFallbackMs, result.singleMs);
		std::cout
			<< "| " << result.key
			<< " | " << result.width << "x" << result.height
			<< " | " << result.iterations
			<< " | " << result.singleMs
			<< " | " << result.coopSeqMs
			<< " | " << result.coopAsyncMs
			<< " | " << result.coopAsyncFallbackMs
			<< " | " << seqRatio << "x"
			<< " | " << asyncRatio << "x"
			<< " | " << fallbackRatio << "x |\n";
	}

	return 0;
}
