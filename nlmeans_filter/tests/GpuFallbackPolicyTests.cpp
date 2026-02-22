// GPU 失敗時の CPU フォールバック判定を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/GpuFallbackPolicy.h"

TEST(GpuFallbackPolicyTests, NoAvx2FallsBackToCpuNaive)
{
	EXPECT_EQ(resolve_gpu_failure_fallback_mode(false), ExecutionMode::CpuNaive);
}

TEST(GpuFallbackPolicyTests, Avx2AvailableFallsBackToCpuAvx2)
{
	EXPECT_EQ(resolve_gpu_failure_fallback_mode(true), ExecutionMode::CpuAvx2);
}
