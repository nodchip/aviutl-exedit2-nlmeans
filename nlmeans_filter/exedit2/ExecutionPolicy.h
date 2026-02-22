// ExEdit2 の実行ポリシー決定ロジック。
#ifndef EXEDIT2_EXECUTION_POLICY_H
#define EXEDIT2_EXECUTION_POLICY_H

#include "BackendSelection.h"
#include "GpuAdapterSelection.h"

// 実行モードと GPU ordinal の組を表す。
struct ExecutionPolicy {
	ExecutionMode mode;
	int gpuAdapterOrdinal;
};

// UI 設定と実行環境から最終実行ポリシーを決定する。
inline ExecutionPolicy resolve_execution_policy(
	int requestedMode,
	int selectedGpuAdapterValue,
	size_t hardwareGpuCount,
	bool isAvx2Available)
{
	const bool hasGpuAdapter = hardwareGpuCount > 0;
	const bool isGpuSelectionAvailable = is_gpu_adapter_selection_available(selectedGpuAdapterValue, hardwareGpuCount);
	ExecutionPolicy policy;
	policy.mode = resolve_execution_mode(
		requestedMode,
		hasGpuAdapter,
		isGpuSelectionAvailable,
		isAvx2Available);
	policy.gpuAdapterOrdinal = resolve_gpu_adapter_ordinal(selectedGpuAdapterValue, hardwareGpuCount);
	return policy;
}

#endif
