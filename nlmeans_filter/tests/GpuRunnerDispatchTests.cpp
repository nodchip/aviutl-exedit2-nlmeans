// GPU ランナーディスパッチ層を検証する。
#include "../exedit2/GpuRunnerDispatch.h"

int main()
{
	int initializedAdapter = -999;
	int processCalls = 0;
	int fallbackCalls = 0;

	GpuRunnerDispatchOps ops{};
	ops.initialize = [&initializedAdapter](int adapterOrdinal) {
		initializedAdapter = adapterOrdinal;
		return true;
	};
	ops.process = [&processCalls]() {
		++processCalls;
		return true;
	};
	ops.fallback = [&fallbackCalls](ExecutionMode mode) {
		if (mode != ExecutionMode::CpuAvx2) {
			return false;
		}
		++fallbackCalls;
		return true;
	};

	if (!dispatch_gpu_runner(ops, 7, ExecutionMode::CpuAvx2)) {
		return 1;
	}
	if (initializedAdapter != 7 || processCalls != 1 || fallbackCalls != 0) {
		return 2;
	}

	ops.initialize = [&initializedAdapter](int adapterOrdinal) {
		initializedAdapter = adapterOrdinal;
		return false;
	};
	if (!dispatch_gpu_runner(ops, 9, ExecutionMode::CpuAvx2)) {
		return 3;
	}
	if (initializedAdapter != 9 || fallbackCalls != 1) {
		return 4;
	}

	ops.initialize = [&initializedAdapter](int adapterOrdinal) {
		initializedAdapter = adapterOrdinal;
		return true;
	};
	ops.process = []() { return false; };
	if (!dispatch_gpu_runner(ops, 10, ExecutionMode::CpuAvx2)) {
		return 5;
	}
	if (fallbackCalls != 2) {
		return 6;
	}

	ops.fallback = nullptr;
	if (dispatch_gpu_runner(ops, 10, ExecutionMode::CpuAvx2)) {
		return 7;
	}

	return 0;
}
