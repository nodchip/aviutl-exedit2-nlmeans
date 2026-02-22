// ExEdit2 のバックエンド選択ロジックを検証する。
#include "../exedit2/BackendSelection.h"

int main()
{
	if (resolve_execution_mode_for_test(kModeGpuDx11, true, true, true) != kModeGpuDx11) {
		return 1;
	}
	if (resolve_execution_mode_for_test(kModeGpuDx11, false, true, true) != kModeCpuAvx2) {
		return 2;
	}
	if (resolve_execution_mode_for_test(kModeGpuDx11, false, false, true) != kModeCpuAvx2) {
		return 3;
	}
	if (resolve_execution_mode_for_test(kModeCpuAvx2, false, true, false) != kModeCpuNaive) {
		return 4;
	}
	if (resolve_execution_mode_for_test(kModeCpuAvx2, false, false, false) != kModeCpuNaive) {
		return 5;
	}
	return 0;
}
