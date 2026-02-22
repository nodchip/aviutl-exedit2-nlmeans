// ExEdit2 の実行経路ポリシー判定を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/ProcessingRoutePolicy.h"

TEST(ProcessingRoutePolicyTests, GpuRouteResolvesToGpuWhenAvailable)
{
	const ProcessingRoute route = resolve_processing_route(kModeGpuDx11, 0, 2, true);
	EXPECT_EQ(route.mode, ExecutionMode::GpuDx11);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}

TEST(ProcessingRoutePolicyTests, InvalidGpuSelectionFallsBackToCpuAvx2)
{
	const ProcessingRoute route = resolve_processing_route(kModeGpuDx11, 3, 2, true);
	EXPECT_EQ(route.mode, ExecutionMode::CpuAvx2);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}

TEST(ProcessingRoutePolicyTests, CpuAvx2WithoutSupportFallsBackToCpuNaive)
{
	const ProcessingRoute route = resolve_processing_route(kModeCpuAvx2, 0, 0, false);
	EXPECT_EQ(route.mode, ExecutionMode::CpuNaive);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuNaive);
}
