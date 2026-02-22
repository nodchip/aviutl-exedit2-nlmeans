// GPU 協調 PoC の処理時間比較を Markdown で出力するベンチマーク。
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>
#include "../exedit2/Exedit2GpuRunner.h"
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

	const double tiled_ms = benchmark_ms([&]() {
		for (size_t i = 0; i < 2; ++i) {
			if (!tile_runners[i]->process(
				frame.data(),
				tile_outputs[i].data(),
				width,
				height,
				search_radius,
				time_radius,
				sigma,
				spatial_step,
				temporal_decay,
				tiles[i].yBegin,
				tiles[i].yEnd)) {
				return false;
			}
		}
		return compose_row_tiled_output(width, height, tiles, tile_outputs, &composed);
	}, iterations);

	std::cout << "# GPU Coop PoC Benchmark\n\n";
	std::cout << "- Frame: " << width << "x" << height << "\n";
	std::cout << "- Search Radius: " << search_radius << "\n";
	std::cout << "- Iterations: " << iterations << "\n\n";
	std::cout << "| Mode | Mean Time (ms/frame) |\n";
	std::cout << "|---|---:|\n";
	std::cout << std::fixed << std::setprecision(3);
	std::cout << "| Single GPU Full Frame | " << full_ms << " |\n";
	std::cout << "| Coop PoC (2 tiles, sequential dispatch) | " << tiled_ms << " |\n";
	if (full_ms > 0.0 && tiled_ms > 0.0) {
		std::cout << "\n- Relative (Coop/Single): " << (tiled_ms / full_ms) << "x\n";
	}
	return 0;
}
