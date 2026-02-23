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

}

int main()
{
	const int width = 128;
	const int height = 72;
	const int search_radius = 2;
	const int time_radius = 0;
	const double sigma = 55.0;
	const int spatial_step = 1;
	const double temporal_decay = 0.0;
	const int iterations = 12;

	const std::vector<PIXEL_RGBA> frame = make_frame(width, height, 1);
	std::vector<PIXEL_RGBA> full_out(static_cast<size_t>(width) * static_cast<size_t>(height));
	std::vector<PIXEL_RGBA> composed(static_cast<size_t>(width) * static_cast<size_t>(height));

	Exedit2GpuRunner full_runner;
	if (!full_runner.initialize(-1)) {
		std::cerr << "GPU initialization failed." << std::endl;
		return 1;
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

	const std::vector<GpuRowTile> tiles = plan_gpu_row_tiles(height, 2, 8);
	if (tiles.size() != 2) {
		std::cerr << "Tile planning failed." << std::endl;
		return 1;
	}
	std::vector<std::unique_ptr<Exedit2GpuRunner>> tile_runners(2);
	std::vector<std::vector<PIXEL_RGBA>> tile_outputs(2);
	for (size_t i = 0; i < 2; ++i) {
		tile_runners[i] = std::unique_ptr<Exedit2GpuRunner>(new Exedit2GpuRunner());
		if (!tile_runners[i]->initialize(-1)) {
			std::cerr << "Tile GPU initialization failed." << std::endl;
			return 1;
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

	std::cout << "# GPU Coop PoC Benchmark\n\n";
	std::cout << "- Frame: " << width << "x" << height << "\n";
	std::cout << "- Search Radius: " << search_radius << "\n";
	std::cout << "- Iterations: " << iterations << "\n\n";
	std::cout << "| Mode | Mean Time (ms/frame) |\n";
	std::cout << "|---|---:|\n";
	std::cout << std::fixed << std::setprecision(3);
	std::cout << "| Single GPU Full Frame | " << full_ms << " |\n";
	std::cout << "| Coop PoC (2 tiles, sequential dispatch) | " << tiled_seq_ms << " |\n";
	std::cout << "| Coop PoC (2 tiles, async dispatch) | " << tiled_async_ms << " |\n";
	std::cout << "| Coop PoC (2 tiles, async + single fallback) | " << tiled_async_fallback_ms << " |\n";
	if (full_ms > 0.0 && tiled_seq_ms > 0.0) {
		std::cout << "\n- Relative (CoopSeq/Single): " << (tiled_seq_ms / full_ms) << "x\n";
	}
	if (full_ms > 0.0 && tiled_async_ms > 0.0) {
		std::cout << "- Relative (CoopAsync/Single): " << (tiled_async_ms / full_ms) << "x\n";
	}
	if (full_ms > 0.0 && tiled_async_fallback_ms > 0.0) {
		std::cout << "- Relative (CoopAsyncFallback/Single): " << (tiled_async_fallback_ms / full_ms) << "x\n";
	}
	return 0;
}
