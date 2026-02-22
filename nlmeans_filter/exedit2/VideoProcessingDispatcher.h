// func_proc_video のディスパッチ処理。
#ifndef EXEDIT2_VIDEO_PROCESSING_DISPATCHER_H
#define EXEDIT2_VIDEO_PROCESSING_DISPATCHER_H

#include "ProcessingRoutePolicy.h"

// 処理実体へのハンドラ群。
struct VideoProcessingHandlers {
	void* context;
	bool (*cpuNaive)(void* context);
	bool (*cpuAvx2)(void* context);
	bool (*cpuFast)(void* context);
	bool (*cpuTemporal)(void* context);
	bool (*gpuDx11)(void* context, int adapterOrdinal, ExecutionMode fallbackMode);
};

// 実行経路に応じて処理ハンドラを呼び分ける。
inline bool dispatch_video_processing(const ProcessingRoute& route, const VideoProcessingHandlers& handlers)
{
	switch (route.mode) {
	case ExecutionMode::CpuNaive:
		if (handlers.cpuNaive == nullptr) {
			return false;
		}
		return handlers.cpuNaive(handlers.context);
	case ExecutionMode::CpuAvx2:
		if (handlers.cpuAvx2 == nullptr) {
			return false;
		}
		return handlers.cpuAvx2(handlers.context);
	case ExecutionMode::CpuFast:
		if (handlers.cpuFast == nullptr) {
			return false;
		}
		return handlers.cpuFast(handlers.context);
	case ExecutionMode::CpuTemporal:
		if (handlers.cpuTemporal == nullptr) {
			return false;
		}
		return handlers.cpuTemporal(handlers.context);
	case ExecutionMode::GpuDx11:
		if (handlers.gpuDx11 == nullptr) {
			return false;
		}
		return handlers.gpuDx11(handlers.context, route.gpuAdapterOrdinal, route.gpuFallbackMode);
	default:
		if (handlers.cpuNaive == nullptr) {
			return false;
		}
		return handlers.cpuNaive(handlers.context);
	}
}

#endif
