// 複数 GPU 協調実行フロー（失敗注入含む）を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/GpuCoopExecution.h"

namespace {

std::vector<GpuRowTile> make_two_tiles()
{
	std::vector<GpuRowTile> tiles;
	tiles.push_back({ 0, 0, 8 });
	tiles.push_back({ 1, 8, 16 });
	return tiles;
}

}

TEST(GpuCoopExecutionTests, FailedTileIsRecoveredByTileRetry)
{
	const std::vector<GpuRowTile> tiles = make_two_tiles();
	int runCalls = 0;
	int retryTileCalls = 0;
	int retrySingleCalls = 0;
	int composeCalls = 0;

	bool usedSingleGpu = false;
	const bool ok = execute_gpu_coop_with_recovery(
		tiles,
		true,
		2,
		[&runCalls](size_t tileIndex, const GpuRowTile&) {
			++runCalls;
			return tileIndex == 0;
		},
		[&retryTileCalls](size_t tileIndex, const GpuRowTile&) {
			++retryTileCalls;
			return tileIndex == 1;
		},
		[&retrySingleCalls]() {
			++retrySingleCalls;
			return true;
		},
		[&composeCalls]() {
			++composeCalls;
			return true;
		},
		&usedSingleGpu,
		false);

	EXPECT_TRUE(ok);
	EXPECT_FALSE(usedSingleGpu);
	EXPECT_EQ(runCalls, 2);
	EXPECT_EQ(retryTileCalls, 1);
	EXPECT_EQ(retrySingleCalls, 0);
	EXPECT_EQ(composeCalls, 1);
}

TEST(GpuCoopExecutionTests, MultipleFailuresFallbackToSingleGpu)
{
	const std::vector<GpuRowTile> tiles = make_two_tiles();
	int retrySingleCalls = 0;
	int composeCalls = 0;

	bool usedSingleGpu = false;
	const bool ok = execute_gpu_coop_with_recovery(
		tiles,
		true,
		1,
		[](size_t, const GpuRowTile&) { return false; },
		[](size_t, const GpuRowTile&) { return false; },
		[&retrySingleCalls]() {
			++retrySingleCalls;
			return true;
		},
		[&composeCalls]() {
			++composeCalls;
			return true;
		},
		&usedSingleGpu,
		true);

	EXPECT_TRUE(ok);
	EXPECT_TRUE(usedSingleGpu);
	EXPECT_EQ(retrySingleCalls, 1);
	EXPECT_EQ(composeCalls, 0);
}

TEST(GpuCoopExecutionTests, ComposeFailureReturnsFalse)
{
	const std::vector<GpuRowTile> tiles = make_two_tiles();
	bool usedSingleGpu = false;
	const bool ok = execute_gpu_coop_with_recovery(
		tiles,
		true,
		2,
		[](size_t, const GpuRowTile&) { return true; },
		[](size_t, const GpuRowTile&) { return true; },
		[]() { return true; },
		[]() { return false; },
		&usedSingleGpu,
		true);

	EXPECT_FALSE(ok);
	EXPECT_FALSE(usedSingleGpu);
}

TEST(GpuCoopExecutionTests, RetrySingleGpuFailureReturnsFalse)
{
	const std::vector<GpuRowTile> tiles = make_two_tiles();
	bool usedSingleGpu = false;
	const bool ok = execute_gpu_coop_with_recovery(
		tiles,
		true,
		1,
		[](size_t, const GpuRowTile&) { return false; },
		[](size_t, const GpuRowTile&) { return false; },
		[]() { return false; },
		[]() { return true; },
		&usedSingleGpu,
		true);

	EXPECT_FALSE(ok);
	EXPECT_FALSE(usedSingleGpu);
}
