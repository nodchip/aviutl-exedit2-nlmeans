// 複数 GPU 協調失敗時の再試行ポリシーを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/GpuCoopRecoveryPolicy.h"

TEST(GpuCoopRecoveryPolicyTests, RetryOnlyWhenCoopTileFailed)
{
	EXPECT_FALSE(should_retry_failed_tile_on_single_gpu(false, true, 1, 1));
	EXPECT_FALSE(should_retry_failed_tile_on_single_gpu(true, false, 1, 1));
	EXPECT_TRUE(should_retry_failed_tile_on_single_gpu(true, true, 1, 1));
}

TEST(GpuCoopRecoveryPolicyTests, RetryCountIsLimitedByThreshold)
{
	EXPECT_TRUE(should_retry_failed_tile_on_single_gpu(true, true, 1, 2));
	EXPECT_TRUE(should_retry_failed_tile_on_single_gpu(true, true, 2, 2));
	EXPECT_FALSE(should_retry_failed_tile_on_single_gpu(true, true, 3, 2));
	EXPECT_FALSE(should_retry_failed_tile_on_single_gpu(true, true, 0, 2));
}

TEST(GpuCoopRecoveryPolicyTests, ResolveRecoveryActionCoversFailureInjectionScenarios)
{
	EXPECT_EQ(
		resolve_gpu_coop_recovery_action(false, 2, 2, false),
		GpuCoopRecoveryAction::None);
	EXPECT_EQ(
		resolve_gpu_coop_recovery_action(true, 0, 2, false),
		GpuCoopRecoveryAction::None);
	EXPECT_EQ(
		resolve_gpu_coop_recovery_action(true, 1, 2, false),
		GpuCoopRecoveryAction::RetryFailedTiles);
	EXPECT_EQ(
		resolve_gpu_coop_recovery_action(true, 3, 2, false),
		GpuCoopRecoveryAction::RetrySingleGpu);
	EXPECT_EQ(
		resolve_gpu_coop_recovery_action(true, 1, 2, true),
		GpuCoopRecoveryAction::RetrySingleGpu);
}
