// ExEdit2 の実行経路ポリシー判定を検証する。
#include "../exedit2/ProcessingRoutePolicy.h"

int main()
{
	ProcessingRoute route = resolve_processing_route(kModeGpuDx11, 0, 2, true);
	if (route.mode != ExecutionMode::GpuDx11 || route.gpuAdapterOrdinal != -1 || route.gpuFallbackMode != ExecutionMode::CpuAvx2) {
		return 1;
	}

	route = resolve_processing_route(kModeGpuDx11, 3, 2, true);
	if (route.mode != ExecutionMode::CpuAvx2 || route.gpuAdapterOrdinal != -1 || route.gpuFallbackMode != ExecutionMode::CpuAvx2) {
		return 2;
	}

	route = resolve_processing_route(kModeCpuAvx2, 0, 0, false);
	if (route.mode != ExecutionMode::CpuNaive || route.gpuAdapterOrdinal != -1 || route.gpuFallbackMode != ExecutionMode::CpuNaive) {
		return 3;
	}

	return 0;
}
