// 複数 GPU タイル分割ロジックを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/MultiGpuTiling.h"

TEST(MultiGpuTilingTests, ReturnsEmptyForInvalidInputs)
{
	EXPECT_TRUE(plan_gpu_row_tiles(0, 2).empty());
	EXPECT_TRUE(plan_gpu_row_tiles(10, 0).empty());
	EXPECT_TRUE(plan_gpu_row_tiles(-5, 3).empty());
}

TEST(MultiGpuTilingTests, SplitsRowsEvenlyAcrossAdapters)
{
	const std::vector<GpuRowTile> tiles = plan_gpu_row_tiles(10, 3);
	ASSERT_EQ(tiles.size(), 3u);
	EXPECT_EQ(tiles[0].adapterIndex, 0u);
	EXPECT_EQ(tiles[1].adapterIndex, 1u);
	EXPECT_EQ(tiles[2].adapterIndex, 2u);
	EXPECT_EQ(tiles[0].yBegin, 0);
	EXPECT_EQ(tiles[0].yEnd, 4);
	EXPECT_EQ(tiles[1].yBegin, 4);
	EXPECT_EQ(tiles[1].yEnd, 7);
	EXPECT_EQ(tiles[2].yBegin, 7);
	EXPECT_EQ(tiles[2].yEnd, 10);
}

TEST(MultiGpuTilingTests, LimitsTileCountByMinRows)
{
	const std::vector<GpuRowTile> tiles = plan_gpu_row_tiles(8, 8, 3);
	ASSERT_EQ(tiles.size(), 2u);
	EXPECT_EQ(tiles[0].yBegin, 0);
	EXPECT_EQ(tiles[0].yEnd, 4);
	EXPECT_EQ(tiles[1].yBegin, 4);
	EXPECT_EQ(tiles[1].yEnd, 8);
}

TEST(MultiGpuTilingTests, CoversAllRowsWithoutOverlap)
{
	const int height = 17;
	const std::vector<GpuRowTile> tiles = plan_gpu_row_tiles(height, 4, 2);
	ASSERT_FALSE(tiles.empty());
	EXPECT_EQ(tiles.front().yBegin, 0);
	EXPECT_EQ(tiles.back().yEnd, height);
	for (size_t i = 1; i < tiles.size(); ++i) {
		EXPECT_EQ(tiles[i - 1].yEnd, tiles[i].yBegin);
	}
}
