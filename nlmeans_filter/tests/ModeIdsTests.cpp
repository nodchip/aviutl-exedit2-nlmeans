// ExEdit2 のモードID定義を検証する。
#include "../exedit2/ModeIds.h"

int main()
{
	if (kModeCpuNaive != 0) {
		return 1;
	}
	if (kModeCpuAvx2 != 1) {
		return 2;
	}
	if (kModeGpuDx11 != 2) {
		return 3;
	}
	return 0;
}
