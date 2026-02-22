// ExEdit2 の GPU アダプタ選択ロジック。
#ifndef EXEDIT2_GPU_ADAPTER_SELECTION_H
#define EXEDIT2_GPU_ADAPTER_SELECTION_H

#include <cstddef>

// UI 選択値を実アダプタ ordinal へ変換する。Auto/範囲外は -1。
inline int resolve_gpu_adapter_ordinal(int selectedValue, size_t hardwareCount)
{
	if (selectedValue <= 0) {
		return -1;
	}

	const int ordinal = selectedValue - 1;
	if (ordinal < 0 || static_cast<size_t>(ordinal) >= hardwareCount) {
		return -1;
	}
	return ordinal;
}

// UI 選択値が現在の列挙結果で有効かを返す。
inline bool is_gpu_adapter_selection_available(int selectedValue, size_t hardwareCount)
{
	if (hardwareCount == 0) {
		return false;
	}
	if (selectedValue <= 0) {
		return true;
	}
	return static_cast<size_t>(selectedValue - 1) < hardwareCount;
}

#endif
