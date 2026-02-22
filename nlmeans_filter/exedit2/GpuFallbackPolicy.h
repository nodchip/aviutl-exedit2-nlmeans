// GPU 実行失敗時の CPU フォールバック判定。
#ifndef EXEDIT2_GPU_FALLBACK_POLICY_H
#define EXEDIT2_GPU_FALLBACK_POLICY_H

#include "BackendSelection.h"

// GPU 実行失敗時に選ぶ CPU モードを返す。
inline ExecutionMode resolve_gpu_failure_fallback_mode(bool isAvx2Available)
{
	return isAvx2Available ? ExecutionMode::CpuAvx2 : ExecutionMode::CpuNaive;
}

#endif
