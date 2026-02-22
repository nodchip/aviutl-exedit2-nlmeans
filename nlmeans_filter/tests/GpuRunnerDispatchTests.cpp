// GPU ランナーディスパッチ層を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/GpuRunnerDispatch.h"

TEST(GpuRunnerDispatchTests, SuccessPathInitializesAndProcesses)
{
	int initializedAdapter = -999;
	int processCalls = 0;
	int fallbackCalls = 0;

	GpuRunnerDispatchOps ops{};
	ops.initialize = [&initializedAdapter](int adapterOrdinal) {
		initializedAdapter = adapterOrdinal;
		return true;
	};
	ops.process = [&processCalls]() {
		++processCalls;
		return true;
	};
	ops.fallback = [&fallbackCalls](ExecutionMode mode) {
		EXPECT_EQ(mode, ExecutionMode::CpuAvx2);
		++fallbackCalls;
		return true;
	};

	EXPECT_TRUE(dispatch_gpu_runner(ops, 7, ExecutionMode::CpuAvx2));
	EXPECT_EQ(initializedAdapter, 7);
	EXPECT_EQ(processCalls, 1);
	EXPECT_EQ(fallbackCalls, 0);
}

TEST(GpuRunnerDispatchTests, InitializeFailureFallsBack)
{
	int initializedAdapter = -999;
	int fallbackCalls = 0;

	GpuRunnerDispatchOps ops{};
	ops.initialize = [&initializedAdapter](int adapterOrdinal) {
		initializedAdapter = adapterOrdinal;
		return false;
	};
	ops.process = []() { return true; };
	ops.fallback = [&fallbackCalls](ExecutionMode mode) {
		EXPECT_EQ(mode, ExecutionMode::CpuAvx2);
		++fallbackCalls;
		return true;
	};

	EXPECT_TRUE(dispatch_gpu_runner(ops, 9, ExecutionMode::CpuAvx2));
	EXPECT_EQ(initializedAdapter, 9);
	EXPECT_EQ(fallbackCalls, 1);
}

TEST(GpuRunnerDispatchTests, ProcessFailureFallsBack)
{
	int fallbackCalls = 0;

	GpuRunnerDispatchOps ops{};
	ops.initialize = [](int) { return true; };
	ops.process = []() { return false; };
	ops.fallback = [&fallbackCalls](ExecutionMode mode) {
		EXPECT_EQ(mode, ExecutionMode::CpuAvx2);
		++fallbackCalls;
		return true;
	};

	EXPECT_TRUE(dispatch_gpu_runner(ops, 10, ExecutionMode::CpuAvx2));
	EXPECT_EQ(fallbackCalls, 1);
}

TEST(GpuRunnerDispatchTests, MissingFallbackReturnsFalse)
{
	GpuRunnerDispatchOps ops{};
	ops.initialize = [](int) { return true; };
	ops.process = []() { return false; };
	ops.fallback = nullptr;

	EXPECT_FALSE(dispatch_gpu_runner(ops, 10, ExecutionMode::CpuAvx2));
}
