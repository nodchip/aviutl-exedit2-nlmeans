// CPU Fast モード設定解決を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/FastModeConfig.h"

TEST(FastModeConfigTests, UsesValueInRangeAsIs)
{
	EXPECT_EQ(resolve_fast_spatial_step(1), 1);
	EXPECT_EQ(resolve_fast_spatial_step(2), 2);
	EXPECT_EQ(resolve_fast_spatial_step(4), 4);
}

TEST(FastModeConfigTests, ClampsOutOfRangeValues)
{
	EXPECT_EQ(resolve_fast_spatial_step(-1), 1);
	EXPECT_EQ(resolve_fast_spatial_step(0), 1);
	EXPECT_EQ(resolve_fast_spatial_step(5), 4);
	EXPECT_EQ(resolve_fast_spatial_step(99), 4);
}
