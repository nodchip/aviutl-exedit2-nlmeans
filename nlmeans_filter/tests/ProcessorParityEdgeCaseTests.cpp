// Processor 実経路の Naive / AVX2 一致を境界条件で確認する。
#include "../stdafx.h"
#include "ProcessorHarness.h"

int main()
{
	if (!is_avx2_processor_available()) {
		return 0;
	}

	// 先頭フレーム参照ケース
	ProcessorRunConfig firstFrame = { 10, 9, 7, 0, 2, 2, 52, 404 };
	// 末尾フレーム参照ケース
	ProcessorRunConfig lastFrame = { 10, 9, 7, 6, 2, 2, 52, 405 };
	// 大きめ探索範囲ケース
	ProcessorRunConfig largeSearch = { 18, 14, 9, 4, 3, 2, 48, 406 };

	if (!run_processor_parity_case(firstFrame)) {
		return 1;
	}
	if (!run_processor_parity_case(lastFrame)) {
		return 2;
	}
	if (!run_processor_parity_case(largeSearch)) {
		return 3;
	}

	return 0;
}
