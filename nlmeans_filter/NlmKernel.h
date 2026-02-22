// Copyright 2026 nodchip
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NLM_KERNEL_H
#define NLM_KERNEL_H

#include <immintrin.h>
#include <intrin.h>

// 実行環境が AVX2 を利用可能かを返す。
inline bool is_avx2_supported_runtime()
{
	int cpuInfo[4] = { 0, 0, 0, 0 };
	__cpuid(cpuInfo, 0);
	if (cpuInfo[0] < 7) {
		return false;
	}
	__cpuid(cpuInfo, 1);
	const bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
	const bool avx = (cpuInfo[2] & (1 << 28)) != 0;
	if (!osxsave || !avx) {
		return false;
	}
	const unsigned __int64 xcr0 = _xgetbv(0);
	if ((xcr0 & 0x6) != 0x6) {
		return false;
	}
	__cpuidex(cpuInfo, 7, 0);
	return (cpuInfo[1] & (1 << 5)) != 0;
}

// 3x3 パッチの二乗差和をスカラーで求める。
inline long long patch_ssd_scalar_3x3(const int* a, const int* b)
{
	long long sum = 0;
	for (int i = 0; i < 9; ++i) {
		const int d = a[i] - b[i];
		sum += static_cast<long long>(d) * static_cast<long long>(d);
	}
	return sum;
}

// 3x3 パッチの二乗差和を AVX2 で求める。
inline long long patch_ssd_avx2_3x3(const int* a, const int* b)
{
	__m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a));
	__m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b));
	__m256i vd = _mm256_sub_epi32(va, vb);
	__m256i vs = _mm256_mullo_epi32(vd, vd);

	alignas(32) int tmp[8];
	_mm256_store_si256(reinterpret_cast<__m256i*>(tmp), vs);

	long long sum = 0;
	for (int i = 0; i < 8; ++i) {
		sum += tmp[i];
	}

	const int d9 = a[8] - b[8];
	sum += static_cast<long long>(d9) * static_cast<long long>(d9);
	return sum;
}

#endif
