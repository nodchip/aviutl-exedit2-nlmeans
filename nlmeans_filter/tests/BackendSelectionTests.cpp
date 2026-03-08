// ExEdit2 のバックエンド選択ロジックを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/BackendSelection.h"

TEST(BackendSelectionTests, GpuModeUsesGpuWhenAvailable)
{
	EXPECT_EQ(resolve_execution_mode_for_test(kModeGpuDx11, true, true, true), kModeGpuDx11);
}

TEST(BackendSelectionTests, GpuModeFallsBackToCpuAvx2WhenGpuUnavailable)
{
	EXPECT_EQ(resolve_execution_mode_for_test(kModeGpuDx11, false, true, true), kModeCpuAvx2);
}

TEST(BackendSelectionTests, GpuModeFallsBackToCpuAvx2WithoutCpuAvx2CapabilityFlag)
{
	EXPECT_EQ(resolve_execution_mode_for_test(kModeGpuDx11, false, false, true), kModeCpuAvx2);
}

TEST(BackendSelectionTests, CpuAvx2ModeFallsBackToCpuNaiveWhenAvx2Unavailable)
{
	EXPECT_EQ(resolve_execution_mode_for_test(kModeCpuAvx2, false, true, false), kModeCpuNaive);
	EXPECT_EQ(resolve_execution_mode_for_test(kModeCpuAvx2, false, false, false), kModeCpuNaive);
}

TEST(BackendSelectionTests, CpuFastModeResolvesToCpuFast)
{
	EXPECT_EQ(resolve_execution_mode_for_test(kModeCpuFast, false, false, false), kModeCpuFast);
	EXPECT_EQ(resolve_execution_mode_for_test(kModeCpuFast, true, true, true), kModeCpuFast);
}

TEST(BackendSelectionTests, CpuTemporalModeResolvesToCpuTemporal)
{
	EXPECT_EQ(resolve_execution_mode_for_test(kModeCpuTemporal, false, false, false), kModeCpuTemporal);
	EXPECT_EQ(resolve_execution_mode_for_test(kModeCpuTemporal, true, true, true), kModeCpuTemporal);
}

TEST(BackendSelectionTests, UnknownModeDoesNotResolveToGpu)
{
	EXPECT_EQ(resolve_execution_mode_for_test(kModeGpuDx11 + 1, true, true, true), kModeCpuAvx2);
}
