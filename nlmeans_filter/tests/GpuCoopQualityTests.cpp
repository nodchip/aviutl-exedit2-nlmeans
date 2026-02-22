// 複数 GPU 協調経路（タイル分割合成）の品質一致を検証する。
#include <gtest/gtest.h>

#if __has_include("../aviutl2_sdk/filter2.h")

#include <algorithm>
#include <vector>
#include "../exedit2/Exedit2GpuRunner.h"
#include "../exedit2/MultiGpuCompose.h"
#include "../exedit2/MultiGpuTiling.h"

namespace {

std::vector<PIXEL_RGBA> make_frame(int width, int height, int seed)
{
	const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
	std::vector<PIXEL_RGBA> frame(pixel_count);
	for (size_t i = 0; i < pixel_count; ++i) {
		PIXEL_RGBA p = {};
		const int base = static_cast<int>((i * 37 + static_cast<size_t>(seed) * 23) & 255);
		p.r = static_cast<unsigned char>(base);
		p.g = static_cast<unsigned char>((base * 5 + 11) & 255);
		p.b = static_cast<unsigned char>((base * 7 + 3) & 255);
		p.a = 255;
		frame[i] = p;
	}
	return frame;
}

int max_abs_diff(const std::vector<PIXEL_RGBA>& a, const std::vector<PIXEL_RGBA>& b)
{
	int max_abs = 0;
	for (size_t i = 0; i < a.size(); ++i) {
		max_abs = std::max(max_abs, std::abs(static_cast<int>(a[i].r) - static_cast<int>(b[i].r)));
		max_abs = std::max(max_abs, std::abs(static_cast<int>(a[i].g) - static_cast<int>(b[i].g)));
		max_abs = std::max(max_abs, std::abs(static_cast<int>(a[i].b) - static_cast<int>(b[i].b)));
	}
	return max_abs;
}

}

TEST(GpuCoopQualityTests, SpatialSingleFrameMatchesFullFrameOutput)
{
	const int width = 48;
	const int height = 32;
	const int search_radius = 2;
	const int time_radius = 0;
	const double sigma = 55.0;
	const int spatial_step = 2;
	const double temporal_decay = 0.0;
	const std::vector<PIXEL_RGBA> frame = make_frame(width, height, 1);

	Exedit2GpuRunner full_runner;
	if (!full_runner.initialize(-1)) {
		GTEST_SKIP() << "GPU runner initialization failed.";
	}
	std::vector<PIXEL_RGBA> full_out(static_cast<size_t>(width) * static_cast<size_t>(height));
	ASSERT_TRUE(full_runner.process(
		frame.data(),
		full_out.data(),
		width,
		height,
		search_radius,
		time_radius,
		sigma,
		spatial_step,
		temporal_decay));

	const std::vector<GpuRowTile> tiles = plan_gpu_row_tiles(height, 2, 8);
	ASSERT_EQ(tiles.size(), 2u);

	std::vector<std::vector<PIXEL_RGBA>> tile_outputs(2);
	for (size_t i = 0; i < tiles.size(); ++i) {
		Exedit2GpuRunner tile_runner;
		ASSERT_TRUE(tile_runner.initialize(-1));
		tile_outputs[i].resize(static_cast<size_t>(width) * static_cast<size_t>(height));
		ASSERT_TRUE(tile_runner.process(
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
			tiles[i].yEnd));
	}

	std::vector<PIXEL_RGBA> composed;
	ASSERT_TRUE(compose_row_tiled_output(width, height, tiles, tile_outputs, &composed));
	EXPECT_EQ(max_abs_diff(full_out, composed), 0);
}

TEST(GpuCoopQualityTests, TemporalTwoFrameMatchesFullFrameOutput)
{
	const int width = 48;
	const int height = 32;
	const int search_radius = 2;
	const int time_radius = 1;
	const double sigma = 60.0;
	const int spatial_step = 1;
	const double temporal_decay = 1.0;
	const std::vector<PIXEL_RGBA> frame0 = make_frame(width, height, 2);
	const std::vector<PIXEL_RGBA> frame1 = make_frame(width, height, 3);

	Exedit2GpuRunner full_runner;
	if (!full_runner.initialize(-1)) {
		GTEST_SKIP() << "GPU runner initialization failed.";
	}
	std::vector<PIXEL_RGBA> full_out0(static_cast<size_t>(width) * static_cast<size_t>(height));
	std::vector<PIXEL_RGBA> full_out1(static_cast<size_t>(width) * static_cast<size_t>(height));
	ASSERT_TRUE(full_runner.process(
		frame0.data(),
		full_out0.data(),
		width,
		height,
		search_radius,
		time_radius,
		sigma,
		spatial_step,
		temporal_decay));
	ASSERT_TRUE(full_runner.process(
		frame1.data(),
		full_out1.data(),
		width,
		height,
		search_radius,
		time_radius,
		sigma,
		spatial_step,
		temporal_decay));

	const std::vector<GpuRowTile> tiles = plan_gpu_row_tiles(height, 2, 8);
	ASSERT_EQ(tiles.size(), 2u);

	std::vector<std::vector<PIXEL_RGBA>> tile_outputs(2);
	std::vector<std::unique_ptr<Exedit2GpuRunner>> tile_runners(2);
	for (size_t i = 0; i < tiles.size(); ++i) {
		tile_runners[i] = std::unique_ptr<Exedit2GpuRunner>(new Exedit2GpuRunner());
		ASSERT_TRUE(tile_runners[i]->initialize(-1));
		tile_outputs[i].resize(static_cast<size_t>(width) * static_cast<size_t>(height));
	}

	for (size_t i = 0; i < tiles.size(); ++i) {
		ASSERT_TRUE(tile_runners[i]->process(
			frame0.data(),
			tile_outputs[i].data(),
			width,
			height,
			search_radius,
			time_radius,
			sigma,
			spatial_step,
			temporal_decay,
			tiles[i].yBegin,
			tiles[i].yEnd));
	}
	for (size_t i = 0; i < tiles.size(); ++i) {
		ASSERT_TRUE(tile_runners[i]->process(
			frame1.data(),
			tile_outputs[i].data(),
			width,
			height,
			search_radius,
			time_radius,
			sigma,
			spatial_step,
			temporal_decay,
			tiles[i].yBegin,
			tiles[i].yEnd));
	}

	std::vector<PIXEL_RGBA> composed;
	ASSERT_TRUE(compose_row_tiled_output(width, height, tiles, tile_outputs, &composed));
	EXPECT_EQ(max_abs_diff(full_out1, composed), 0);
}

#else

TEST(GpuCoopQualityTests, SkippedWhenSdkIsUnavailable)
{
	GTEST_SKIP() << "aviutl2_sdk/filter2.h not found.";
}

#endif
