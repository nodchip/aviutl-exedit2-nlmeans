// GPU 実行失敗時フォールバックの共通制御。
#ifndef EXEDIT2_GPU_FALLBACK_EXECUTION_H
#define EXEDIT2_GPU_FALLBACK_EXECUTION_H

#include <functional>
#include "BackendSelection.h"

// GPU 初期化/実行と CPU フォールバックの分岐を共通化する。
inline bool execute_gpu_with_fallback(
	const std::function<bool()>& tryInitializeGpu,
	const std::function<bool()>& runGpu,
	const std::function<bool(ExecutionMode)>& runCpuFallback,
	ExecutionMode fallbackMode)
{
	if (!tryInitializeGpu || !runGpu || !runCpuFallback) {
		return false;
	}
	if (!tryInitializeGpu()) {
		return runCpuFallback(fallbackMode);
	}
	if (!runGpu()) {
		return runCpuFallback(fallbackMode);
	}
	return true;
}

#endif
