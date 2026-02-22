// 複数 GPU 出力合成ロジックを GoogleTest で検証する。
#include <gtest/gtest.h>
#include <vector>
#include "../exedit2/MultiGpuCompose.h"

namespace {

struct Pixel4
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
};

std::vector<Pixel4> make_image(int width, int height, unsigned char base)
{
	std::vector<Pixel4> image(static_cast<size_t>(width) * static_cast<size_t>(height));
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const size_t idx = static_cast<size_t>(y * width + x);
			image[idx] = Pixel4 {
				static_cast<unsigned char>(base + x),
				static_cast<unsigned char>(base + y),
				base,
				255
			};
		}
	}
	return image;
}

}

TEST(MultiGpuComposeTests, ComposeCopiesRowsFromAssignedAdapters)
{
	const int width = 4;
	const int height = 4;
	const std::vector<GpuRowTile> tiles = {
		{ 0, 0, 2 },
		{ 1, 2, 4 }
	};
	const std::vector<std::vector<Pixel4>> perAdapter = {
		make_image(width, height, 10),
		make_image(width, height, 50)
	};

	std::vector<Pixel4> out;
	ASSERT_TRUE(compose_row_tiled_output(width, height, tiles, perAdapter, &out));
	ASSERT_EQ(out.size(), static_cast<size_t>(width * height));

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const size_t idx = static_cast<size_t>(y * width + x);
			if (y < 2) {
				EXPECT_EQ(out[idx].b, 10);
			} else {
				EXPECT_EQ(out[idx].b, 50);
			}
		}
	}
}

TEST(MultiGpuComposeTests, ReturnsFalseWhenTileReferencesMissingAdapter)
{
	const int width = 3;
	const int height = 2;
	const std::vector<GpuRowTile> tiles = {
		{ 2, 0, 2 }
	};
	const std::vector<std::vector<Pixel4>> perAdapter = {
		make_image(width, height, 1)
	};
	std::vector<Pixel4> out;
	EXPECT_FALSE(compose_row_tiled_output(width, height, tiles, perAdapter, &out));
}
