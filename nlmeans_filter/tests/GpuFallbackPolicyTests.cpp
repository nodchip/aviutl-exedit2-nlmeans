// GPU 失敗時の CPU フォールバック判定を検証する。
#include "../exedit2/GpuFallbackPolicy.h"

int main()
{
	if (resolve_gpu_failure_fallback_mode(false) != ExecutionMode::CpuNaive) {
		return 1;
	}
	if (resolve_gpu_failure_fallback_mode(true) != ExecutionMode::CpuAvx2) {
		return 2;
	}
	return 0;
}
