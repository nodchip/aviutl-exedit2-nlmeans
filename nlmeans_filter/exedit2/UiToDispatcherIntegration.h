// UI 選択値からディスパッチ実行までを接続する補助。
#ifndef EXEDIT2_UI_TO_DISPATCHER_INTEGRATION_H
#define EXEDIT2_UI_TO_DISPATCHER_INTEGRATION_H

#include "UiSelectionRoute.h"
#include "VideoProcessingDispatcher.h"

// UI 選択値から実行経路を解決してディスパッチする。
inline bool dispatch_from_ui_selection(
	const UiSelectionSnapshot& ui,
	size_t hardwareGpuCount,
	bool isAvx2Available,
	const VideoProcessingHandlers& handlers,
	ProcessingRoute* outRoute = nullptr)
{
	const ProcessingRoute route = resolve_route_from_ui_selection(ui, hardwareGpuCount, isAvx2Available);
	if (outRoute != nullptr) {
		*outRoute = route;
	}
	return dispatch_video_processing(route, handlers);
}

#endif
