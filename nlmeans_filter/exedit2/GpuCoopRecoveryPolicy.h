// ExEdit2 の複数 GPU 協調失敗時リカバリポリシー。
#ifndef EXEDIT2_GPU_COOP_RECOVERY_POLICY_H
#define EXEDIT2_GPU_COOP_RECOVERY_POLICY_H

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

#endif
