// UI 設定値と実行経路の結合ロジックを検証する。
#include "../exedit2/UiSelectionRoute.h"

int main()
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = 0;

	ProcessingRoute route = resolve_route_from_ui_selection(ui, 2, true);
	if (route.mode != ExecutionMode::GpuDx11 || route.gpuAdapterOrdinal != -1 || route.gpuFallbackMode != ExecutionMode::CpuAvx2) {
		return 1;
	}

	ui.modeValue = kModeCpuAvx2;
	route = resolve_route_from_ui_selection(ui, 0, false);
	if (route.mode != ExecutionMode::CpuNaive || route.gpuAdapterOrdinal != -1 || route.gpuFallbackMode != ExecutionMode::CpuNaive) {
		return 2;
	}

	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = 3;
	route = resolve_route_from_ui_selection(ui, 2, true);
	if (route.mode != ExecutionMode::CpuAvx2 || route.gpuAdapterOrdinal != -1 || route.gpuFallbackMode != ExecutionMode::CpuAvx2) {
		return 3;
	}

	return 0;
}
