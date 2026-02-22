// func_proc_video のディスパッチ処理。
#ifndef EXEDIT2_VIDEO_PROCESSING_DISPATCHER_H
#define EXEDIT2_VIDEO_PROCESSING_DISPATCHER_H

#include "ProcessingRoutePolicy.h"

// 処理実体へのハンドラ群。
struct VideoProcessingHandlers {
	void* context;
	bool (*cpuNaive)(void* context);
	bool (*cpuAvx2)(void* context);
	bool (*gpuDx11)(void* context, int adapterOrdinal, ExecutionMode fallbackMode);
};

// 実行経路に応じて処理ハンドラを呼び分ける。
inline bool dispatch_video_processing(const ProcessingRoute& route, const VideoProcessingHandlers& handlers)
{
	switch (route.mode) {
	case ExecutionMode::CpuNaive:
		return handlers.cpuNaive(handlers.context);
	case ExecutionMode::CpuAvx2:
		return handlers.cpuAvx2(handlers.context);
	case ExecutionMode::GpuDx11:
		return handlers.gpuDx11(handlers.context, route.gpuAdapterOrdinal, route.gpuFallbackMode);
	default:
		return handlers.cpuNaive(handlers.context);
	}
}

#endif
