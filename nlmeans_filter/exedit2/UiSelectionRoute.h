// UI 設定値から実行経路を決定する補助。
#ifndef EXEDIT2_UI_SELECTION_ROUTE_H
#define EXEDIT2_UI_SELECTION_ROUTE_H

#include "ProcessingRoutePolicy.h"

// UI 設定値のスナップショット。
struct UiSelectionSnapshot {
	int modeValue;
	int gpuAdapterValue;
};

// UI 設定値と実行環境から実行経路を決定する。
inline ProcessingRoute resolve_route_from_ui_selection(
	const UiSelectionSnapshot& ui,
	size_t hardwareGpuCount,
	bool isAvx2Available)
{
	return resolve_processing_route(
		ui.modeValue,
		ui.gpuAdapterValue,
		hardwareGpuCount,
		isAvx2Available);
}

#endif
