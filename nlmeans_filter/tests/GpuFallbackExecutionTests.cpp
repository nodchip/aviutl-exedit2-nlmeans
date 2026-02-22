// GPU 実行失敗時フォールバック分岐を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/GpuFallbackExecution.h"

TEST(GpuFallbackExecutionTests, SuccessPathDoesNotCallFallback)
{
	int fallbackCalls = 0;
	const bool ok = execute_gpu_with_fallback(
		[]() { return true; },
		[]() { return true; },
		[&fallbackCalls](ExecutionMode mode) {
			EXPECT_EQ(mode, ExecutionMode::CpuAvx2);
			++fallbackCalls;
			return true;
		},
		ExecutionMode::CpuAvx2);
	EXPECT_TRUE(ok);
	EXPECT_EQ(fallbackCalls, 0);
}

TEST(GpuFallbackExecutionTests, GpuFailureCallsFallback)
{
	int fallbackCalls = 0;
	const bool ok = execute_gpu_with_fallback(
		[]() { return false; },
		[]() { return true; },
		[&fallbackCalls](ExecutionMode mode) {
			EXPECT_EQ(mode, ExecutionMode::CpuNaive);
			++fallbackCalls;
			return true;
		},
		ExecutionMode::CpuNaive);
	EXPECT_TRUE(ok);
	EXPECT_EQ(fallbackCalls, 1);
}

TEST(GpuFallbackExecutionTests, GpuRunnerFailureCallsFallback)
{
	int fallbackCalls = 0;
	const bool ok = execute_gpu_with_fallback(
		[]() { return true; },
		[]() { return false; },
		[&fallbackCalls](ExecutionMode mode) {
			EXPECT_EQ(mode, ExecutionMode::CpuAvx2);
			++fallbackCalls;
			return true;
		},
		ExecutionMode::CpuAvx2);
	EXPECT_TRUE(ok);
	EXPECT_EQ(fallbackCalls, 1);
}

TEST(GpuFallbackExecutionTests, MissingGpuHandlerReturnsFalse)
{
	const bool ok = execute_gpu_with_fallback(
		nullptr,
		[]() { return true; },
		[](ExecutionMode) { return true; },
		ExecutionMode::CpuNaive);
	EXPECT_FALSE(ok);
}
