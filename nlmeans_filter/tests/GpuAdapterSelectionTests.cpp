// ExEdit2 の GPU アダプタ選択ロジックを検証する。
#include "../exedit2/GpuAdapterSelection.h"

int main()
{
	if (resolve_gpu_adapter_ordinal(0, 2) != -1) {
		return 1;
	}
	if (resolve_gpu_adapter_ordinal(1, 2) != 0) {
		return 2;
	}
	if (resolve_gpu_adapter_ordinal(2, 2) != 1) {
		return 3;
	}
	if (resolve_gpu_adapter_ordinal(3, 2) != -1) {
		return 4;
	}
	if (resolve_gpu_adapter_ordinal(1, 0) != -1) {
		return 5;
	}
	if (!is_gpu_adapter_selection_available(0, 2)) {
		return 6;
	}
	if (!is_gpu_adapter_selection_available(2, 2)) {
		return 7;
	}
	if (is_gpu_adapter_selection_available(3, 2)) {
		return 8;
	}
	if (is_gpu_adapter_selection_available(0, 0)) {
		return 9;
	}
	return 0;
}
