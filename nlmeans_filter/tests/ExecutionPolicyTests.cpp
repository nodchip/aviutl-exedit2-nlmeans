// ExEdit2 の実行ポリシー決定ロジックを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/ExecutionPolicy.h"

TEST(ExecutionPolicyTests, GpuAutoUsesNoPinnedAdapter)
{
	const ExecutionPolicy p = resolve_execution_policy(kModeGpuDx11, 0, 2, true);
	EXPECT_EQ(p.mode, ExecutionMode::GpuDx11);
	EXPECT_EQ(p.gpuAdapterOrdinal, -1);
}

TEST(ExecutionPolicyTests, GpuSpecificAdapterUsesZeroBasedOrdinal)
{
	const ExecutionPolicy p = resolve_execution_policy(kModeGpuDx11, 1, 2, true);
	EXPECT_EQ(p.mode, ExecutionMode::GpuDx11);
	EXPECT_EQ(p.gpuAdapterOrdinal, 0);
}

TEST(ExecutionPolicyTests, OutOfRangeAdapterFallsBackToCpuAvx2)
{
	const ExecutionPolicy p = resolve_execution_policy(kModeGpuDx11, 3, 2, true);
	EXPECT_EQ(p.mode, ExecutionMode::CpuAvx2);
	EXPECT_EQ(p.gpuAdapterOrdinal, -1);
}

TEST(ExecutionPolicyTests, NoGpuFallsBackToCpuAvx2)
{
	const ExecutionPolicy p = resolve_execution_policy(kModeGpuDx11, 0, 0, true);
	EXPECT_EQ(p.mode, ExecutionMode::CpuAvx2);
	EXPECT_EQ(p.gpuAdapterOrdinal, -1);
}

TEST(ExecutionPolicyTests, NoAvx2FallsBackToCpuNaive)
{
	const ExecutionPolicy p = resolve_execution_policy(kModeCpuAvx2, 0, 0, false);
	EXPECT_EQ(p.mode, ExecutionMode::CpuNaive);
	EXPECT_EQ(p.gpuAdapterOrdinal, -1);
}

TEST(ExecutionPolicyTests, UnknownModeFallsBackToCpuAvx2)
{
	const ExecutionPolicy p = resolve_execution_policy(kModeGpuDx11 + 1, 1, 2, true);
	EXPECT_EQ(p.mode, ExecutionMode::CpuAvx2);
	EXPECT_EQ(p.gpuAdapterOrdinal, 0);
}
