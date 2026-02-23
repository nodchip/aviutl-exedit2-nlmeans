// ExEdit2 の複数 GPU 協調失敗時リカバリポリシー。
#ifndef EXEDIT2_GPU_COOP_RECOVERY_POLICY_H
#define EXEDIT2_GPU_COOP_RECOVERY_POLICY_H

// 協調失敗時に次に取るべきリカバリアクション。
enum class GpuCoopRecoveryAction
{
	None = 0,
	RetryFailedTiles = 1,
	RetrySingleGpu = 2
};

// 協調タイル単位の再試行を行うべきかを判定する。
inline bool should_retry_failed_tile_on_single_gpu(
	bool coopEnabled,
	bool tileFailed,
	int failedTileCount,
	int maxRetryTiles)
{
	if (!coopEnabled || !tileFailed) {
		return false;
	}
	if (failedTileCount <= 0) {
		return false;
	}
	return failedTileCount <= maxRetryTiles;
}

// 協調失敗時の次アクションを決定する。
inline GpuCoopRecoveryAction resolve_gpu_coop_recovery_action(
	bool coopEnabled,
	int failedTileCount,
	int maxRetryTiles,
	bool tileRetryAttempted)
{
	if (!coopEnabled || failedTileCount <= 0) {
		return GpuCoopRecoveryAction::None;
	}
	if (failedTileCount <= maxRetryTiles && !tileRetryAttempted) {
		return GpuCoopRecoveryAction::RetryFailedTiles;
	}
	return GpuCoopRecoveryAction::RetrySingleGpu;
}

#endif
