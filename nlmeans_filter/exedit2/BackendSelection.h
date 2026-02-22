// ExEdit2 のバックエンド選択ロジック。
#ifndef EXEDIT2_BACKEND_SELECTION_H
#define EXEDIT2_BACKEND_SELECTION_H

#include "ModeIds.h"

// 実行バックエンドの選択状態を表す。
enum class ExecutionMode : int {
	CpuNaive = kModeCpuNaive,
	CpuAvx2 = kModeCpuAvx2,
	CpuFast = kModeCpuFast,
	GpuDx11 = kModeGpuDx11
};

// 要求モードと環境能力から、実際に実行するモードを決定する。
inline ExecutionMode resolve_execution_mode(
	int requestedMode,
	bool hasGpuAdapter,
	bool isSelectedGpuAdapterAvailable,
	bool isAvx2Available)
{
	if (requestedMode >= static_cast<int>(ExecutionMode::GpuDx11) &&
		hasGpuAdapter &&
		isSelectedGpuAdapterAvailable) {
		return ExecutionMode::GpuDx11;
	}
	if (requestedMode == static_cast<int>(ExecutionMode::CpuFast)) {
		return ExecutionMode::CpuFast;
	}
	if (requestedMode >= static_cast<int>(ExecutionMode::CpuAvx2) && isAvx2Available) {
		return ExecutionMode::CpuAvx2;
	}
	return ExecutionMode::CpuNaive;
}

// 単体テスト用の int 返却ラッパー。
inline int resolve_execution_mode_for_test(
	int requestedMode,
	bool hasGpuAdapter,
	bool isSelectedGpuAdapterAvailable,
	bool isAvx2Available)
{
	return static_cast<int>(resolve_execution_mode(
		requestedMode,
		hasGpuAdapter,
		isSelectedGpuAdapterAvailable,
		isAvx2Available));
}

#endif
