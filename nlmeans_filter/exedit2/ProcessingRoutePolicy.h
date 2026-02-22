// ExEdit2 の実行経路ポリシー判定。
#ifndef EXEDIT2_PROCESSING_ROUTE_POLICY_H
#define EXEDIT2_PROCESSING_ROUTE_POLICY_H

#include "ExecutionPolicy.h"
#include "GpuFallbackPolicy.h"

// 実行経路に必要な情報をまとめた構造体。
struct ProcessingRoute {
	ExecutionMode mode;
	int gpuAdapterOrdinal;
	ExecutionMode gpuFallbackMode;
};

// UI 設定と実行環境から最終実行経路を決定する。
inline ProcessingRoute resolve_processing_route(
	int requestedMode,
	int selectedGpuAdapterValue,
	size_t hardwareGpuCount,
	bool isAvx2Available)
{
	const ExecutionPolicy policy = resolve_execution_policy(
		requestedMode,
		selectedGpuAdapterValue,
		hardwareGpuCount,
		isAvx2Available);
	ProcessingRoute route;
	route.mode = policy.mode;
	route.gpuAdapterOrdinal = policy.gpuAdapterOrdinal;
	route.gpuFallbackMode = resolve_gpu_failure_fallback_mode(isAvx2Available);
	return route;
}

#endif
