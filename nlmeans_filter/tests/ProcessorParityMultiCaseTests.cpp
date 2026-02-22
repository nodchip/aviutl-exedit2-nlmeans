// Processor 実経路の Naive / AVX2 一致を複数条件で確認する。
#include "../stdafx.h"
#include "ProcessorHarness.h"

int main()
{
	if (!is_avx2_processor_available()) {
		return 0;
	}

	ProcessorRunConfig case1 = { 8, 8, 5, 2, 1, 1, 50, 101 };
	ProcessorRunConfig case2 = { 12, 10, 7, 3, 2, 1, 45, 202 };
	ProcessorRunConfig case3 = { 16, 12, 9, 4, 2, 2, 55, 303 };

	if (!run_processor_parity_case(case1)) {
		return 1;
	}
	if (!run_processor_parity_case(case2)) {
		return 2;
	}
	if (!run_processor_parity_case(case3)) {
		return 3;
	}
	return 0;
}
