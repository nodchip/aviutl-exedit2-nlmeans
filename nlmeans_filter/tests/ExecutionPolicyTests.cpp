// ExEdit2 の実行ポリシー決定ロジックを検証する。
#include "../exedit2/ExecutionPolicy.h"

int main()
{
	ExecutionPolicy p = resolve_execution_policy(2, 0, 2, true);
	if (p.mode != ExecutionMode::GpuDx11 || p.gpuAdapterOrdinal != -1) {
		return 1;
	}

	p = resolve_execution_policy(2, 1, 2, true);
	if (p.mode != ExecutionMode::GpuDx11 || p.gpuAdapterOrdinal != 0) {
		return 2;
	}

	p = resolve_execution_policy(2, 3, 2, true);
	if (p.mode != ExecutionMode::CpuAvx2 || p.gpuAdapterOrdinal != -1) {
		return 3;
	}

	p = resolve_execution_policy(2, 0, 0, true);
	if (p.mode != ExecutionMode::CpuAvx2 || p.gpuAdapterOrdinal != -1) {
		return 4;
	}

	p = resolve_execution_policy(1, 0, 0, false);
	if (p.mode != ExecutionMode::CpuNaive || p.gpuAdapterOrdinal != -1) {
		return 5;
	}

	return 0;
}
