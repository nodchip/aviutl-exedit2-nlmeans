// 複数 GPU 協調ポリシーを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/GpuCoopPolicy.h"

TEST(GpuCoopPolicyTests, MultiGpuRequiresAutoSelectionAndMultipleAdapters)
{
	EXPECT_FALSE(should_enable_multi_gpu(0, 2, 2));
	EXPECT_FALSE(should_enable_multi_gpu(-1, 1, 2));
	EXPECT_FALSE(should_enable_multi_gpu(-1, 2, 1));
	EXPECT_TRUE(should_enable_multi_gpu(-1, 2, 2));
}

TEST(GpuCoopPolicyTests, ActiveGpuCountIsClamped)
{
	EXPECT_EQ(resolve_active_gpu_count(0, 4), 1u);
	EXPECT_EQ(resolve_active_gpu_count(1, 4), 1u);
	EXPECT_EQ(resolve_active_gpu_count(2, 1), 1u);
	EXPECT_EQ(resolve_active_gpu_count(2, 3), 2u);
	EXPECT_EQ(resolve_active_gpu_count(4, 3), 3u);
}
