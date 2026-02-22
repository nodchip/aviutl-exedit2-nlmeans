// GoogleTest で ExEdit2 のモードID定義を検証する。
#include <gtest/gtest.h>
#include "../exedit2/ModeIds.h"

TEST(ModeIdsGoogleTest, ConstantsAreStable)
{
	EXPECT_EQ(kModeCpuNaive, 0);
	EXPECT_EQ(kModeCpuAvx2, 1);
	EXPECT_EQ(kModeCpuFast, 2);
	EXPECT_EQ(kModeGpuDx11, 3);
}
