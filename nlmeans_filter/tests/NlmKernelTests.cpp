// Naive と AVX2 の基礎演算一致性テスト。
#include "NlmKernel.h"

int main()
{
	const int a[9] = { 10, 20, 30, 40, 50, 60, 70, 80, 90 };
	const int b[9] = { 11, 19, 33, 39, 52, 61, 69, 81, 88 };

	const long long scalar = patch_ssd_scalar_3x3(a, b);
	if (scalar != 23) {
		return 1;
	}

	if (is_avx2_supported_runtime()) {
		const long long avx2 = patch_ssd_avx2_3x3(a, b);
		if (avx2 != scalar) {
			return 2;
		}
	}

	return 0;
}
