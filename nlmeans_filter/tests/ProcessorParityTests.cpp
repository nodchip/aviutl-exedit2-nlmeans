// ProcessorCpu / ProcessorAvx2 の実経路一致テスト。
#include "../stdafx.h"
#include "ProcessorHarness.h"

int main()
{
	if (!is_avx2_processor_available()) {
		return 0;
	}

	ProcessorRunConfig cfg = {};
	cfg.width = 8;
	cfg.height = 8;
	cfg.totalFrames = 5;
	cfg.currentFrame = 2;
	cfg.searchRadius = 1;
	cfg.timeRadius = 1;
	cfg.h = 50;
	cfg.seed = 123;

	ProcessorRunResult cpu = run_processor_cpu(cfg);
	ProcessorRunResult avx2 = run_processor_avx2(cfg);

	if (cpu.output.size() != avx2.output.size()) {
		return 1;
	}
	for (size_t i = 0; i < cpu.output.size(); ++i) {
		if (cpu.output[i].y != avx2.output[i].y ||
			cpu.output[i].cb != avx2.output[i].cb ||
			cpu.output[i].cr != avx2.output[i].cr) {
			return 2;
		}
	}

	return 0;
}
