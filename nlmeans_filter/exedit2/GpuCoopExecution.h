// 複数 GPU 協調実行とリカバリ制御の共通化。
#ifndef EXEDIT2_GPU_COOP_EXECUTION_H
#define EXEDIT2_GPU_COOP_EXECUTION_H

#include <cstddef>
#include <functional>
#include <vector>
#include "GpuCoopRecoveryPolicy.h"
#include "GpuCoopTaskDispatch.h"
#include "MultiGpuTiling.h"

// 協調タイル実行とリカバリ分岐を共通化する。
inline bool execute_gpu_coop_with_recovery(
	const std::vector<GpuRowTile>& tiles,
	bool enableMultiGpu,
	int retryThreshold,
	const std::function<bool(size_t, const GpuRowTile&)>& runTile,
	const std::function<bool(size_t, const GpuRowTile&)>& retryFailedTile,
	const std::function<bool()>& retrySingleGpu,
	const std::function<bool()>& composeCoopOutput,
	bool* usedSingleGpuResult,
	bool enableAsyncDispatch = true)
{
	if (!runTile || !retrySingleGpu || !composeCoopOutput || tiles.empty()) {
		return false;
	}

	if (usedSingleGpuResult != nullptr) {
		*usedSingleGpuResult = false;
	}

	std::vector<bool> tileSuccess = execute_gpu_coop_tasks(
		tiles.size(),
		[&](size_t tileIndex) {
			return runTile(tileIndex, tiles[tileIndex]);
		},
		enableAsyncDispatch);

	int failedTileCount = 0;
	for (size_t i = 0; i < tileSuccess.size(); ++i) {
		if (!tileSuccess[i]) {
			++failedTileCount;
		}
	}

	const GpuCoopRecoveryAction action = resolve_gpu_coop_recovery_action(
		enableMultiGpu,
		failedTileCount,
		retryThreshold,
		false);
	if (action == GpuCoopRecoveryAction::RetryFailedTiles) {
		if (!retryFailedTile) {
			return false;
		}
		for (size_t i = 0; i < tiles.size(); ++i) {
			if (tileSuccess[i]) {
				continue;
			}
			tileSuccess[i] = retryFailedTile(i, tiles[i]);
		}
		failedTileCount = 0;
		for (size_t i = 0; i < tileSuccess.size(); ++i) {
			if (!tileSuccess[i]) {
				++failedTileCount;
			}
		}
	}

	bool coopFailed = failedTileCount > 0;
	if (coopFailed && resolve_gpu_coop_recovery_action(enableMultiGpu, failedTileCount, retryThreshold, true) ==
		GpuCoopRecoveryAction::RetrySingleGpu) {
		if (!retrySingleGpu()) {
			return false;
		}
		if (usedSingleGpuResult != nullptr) {
			*usedSingleGpuResult = true;
		}
		coopFailed = false;
	}
	if (!coopFailed && (usedSingleGpuResult == nullptr || !*usedSingleGpuResult) && !composeCoopOutput()) {
		coopFailed = true;
	}
	return !coopFailed;
}

#endif
