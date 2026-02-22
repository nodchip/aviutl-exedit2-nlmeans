// Naive と AVX2 の基礎演算一致性を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "NlmKernel.h"

TEST(NlmKernelTests, ScalarSsdMatchesExpectedValue)
{
	const int a[9] = { 10, 20, 30, 40, 50, 60, 70, 80, 90 };
	const int b[9] = { 11, 19, 33, 39, 52, 61, 69, 81, 88 };

	const long long scalar = patch_ssd_scalar_3x3(a, b);
	EXPECT_EQ(scalar, 23);
}

TEST(NlmKernelTests, Avx2SsdMatchesScalarWhenAvailable)
{
	const int a[9] = { 10, 20, 30, 40, 50, 60, 70, 80, 90 };
	const int b[9] = { 11, 19, 33, 39, 52, 61, 69, 81, 88 };

	const long long scalar = patch_ssd_scalar_3x3(a, b);
	if (!is_avx2_supported_runtime()) {
		GTEST_SKIP() << "AVX2 not available on this environment.";
	}

	const long long avx2 = patch_ssd_avx2_3x3(a, b);
	EXPECT_EQ(avx2, scalar);
}
