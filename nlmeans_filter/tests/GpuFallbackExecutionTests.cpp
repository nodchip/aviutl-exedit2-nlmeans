// GPU 実行失敗時フォールバック分岐を検証する。
#include "../exedit2/GpuFallbackExecution.h"

int main()
{
	int fallbackCalls = 0;

	bool ok = execute_gpu_with_fallback(
		[]() { return true; },
		[]() { return true; },
		[&fallbackCalls](ExecutionMode mode) {
			if (mode != ExecutionMode::CpuAvx2) {
				return false;
			}
			++fallbackCalls;
			return true;
		},
		ExecutionMode::CpuAvx2);
	if (!ok || fallbackCalls != 0) {
		return 1;
	}

	ok = execute_gpu_with_fallback(
		[]() { return false; },
		[]() { return true; },
		[&fallbackCalls](ExecutionMode mode) {
			if (mode != ExecutionMode::CpuNaive) {
				return false;
			}
			++fallbackCalls;
			return true;
		},
		ExecutionMode::CpuNaive);
	if (!ok || fallbackCalls != 1) {
		return 2;
	}

	ok = execute_gpu_with_fallback(
		[]() { return true; },
		[]() { return false; },
		[&fallbackCalls](ExecutionMode mode) {
			if (mode != ExecutionMode::CpuAvx2) {
				return false;
			}
			++fallbackCalls;
			return true;
		},
		ExecutionMode::CpuAvx2);
	if (!ok || fallbackCalls != 2) {
		return 3;
	}

	ok = execute_gpu_with_fallback(
		nullptr,
		[]() { return true; },
		[](ExecutionMode) { return true; },
		ExecutionMode::CpuNaive);
	if (ok) {
		return 4;
	}

	return 0;
}
