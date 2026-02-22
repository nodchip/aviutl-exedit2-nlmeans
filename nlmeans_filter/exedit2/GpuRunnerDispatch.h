// GPU ランナー呼び出しディスパッチ。
#ifndef EXEDIT2_GPU_RUNNER_DISPATCH_H
#define EXEDIT2_GPU_RUNNER_DISPATCH_H

#include <functional>
#include "GpuFallbackExecution.h"

// GPU 初期化/実行/フォールバックを受け持つコールバック群。
struct GpuRunnerDispatchOps {
	std::function<bool(int)> initialize;
	std::function<bool()> process;
	std::function<bool(ExecutionMode)> fallback;
};

// GPU ランナー実行をディスパッチし、失敗時は指定 CPU モードへフォールバックする。
inline bool dispatch_gpu_runner(
	const GpuRunnerDispatchOps& ops,
	int adapterOrdinal,
	ExecutionMode fallbackMode)
{
	if (!ops.initialize || !ops.process || !ops.fallback) {
		return false;
	}

	return execute_gpu_with_fallback(
		[&ops, adapterOrdinal]() {
			return ops.initialize(adapterOrdinal);
		},
		ops.process,
		ops.fallback,
		fallbackMode);
}

#endif
