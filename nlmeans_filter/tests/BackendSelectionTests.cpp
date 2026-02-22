// ExEdit2 のバックエンド選択ロジックを検証する。
#include "../exedit2/BackendSelection.h"

int main()
{
	if (resolve_execution_mode_for_test(2, true, true, true) != 2) {
		return 1;
	}
	if (resolve_execution_mode_for_test(2, false, true, true) != 1) {
		return 2;
	}
	if (resolve_execution_mode_for_test(2, false, false, true) != 1) {
		return 3;
	}
	if (resolve_execution_mode_for_test(1, false, true, false) != 0) {
		return 4;
	}
	if (resolve_execution_mode_for_test(1, false, false, false) != 0) {
		return 5;
	}
	return 0;
}
