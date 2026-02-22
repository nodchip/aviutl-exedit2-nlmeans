// ExEdit2 の GPU アダプタ選択ロジックを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/GpuAdapterSelection.h"

TEST(GpuAdapterSelectionTests, ResolveOrdinalFollowsUiValues)
{
	EXPECT_EQ(resolve_gpu_adapter_ordinal(0, 2), -1);
	EXPECT_EQ(resolve_gpu_adapter_ordinal(1, 2), 0);
	EXPECT_EQ(resolve_gpu_adapter_ordinal(2, 2), 1);
	EXPECT_EQ(resolve_gpu_adapter_ordinal(3, 2), -1);
	EXPECT_EQ(resolve_gpu_adapter_ordinal(1, 0), -1);
}

TEST(GpuAdapterSelectionTests, AvailabilityDependsOnAdapterCount)
{
	EXPECT_TRUE(is_gpu_adapter_selection_available(0, 2));
	EXPECT_TRUE(is_gpu_adapter_selection_available(2, 2));
	EXPECT_FALSE(is_gpu_adapter_selection_available(3, 2));
	EXPECT_FALSE(is_gpu_adapter_selection_available(0, 0));
}
