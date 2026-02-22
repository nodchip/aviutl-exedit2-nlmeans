// UI 選択値からディスパッチまでの統合を検証する。
#include "../exedit2/UiToDispatcherIntegration.h"

namespace {

int g_last_call = 0;
int g_last_adapter = -999;
ExecutionMode g_last_fallback = ExecutionMode::CpuNaive;

bool cpu_naive_stub(void*)
{
	g_last_call = 1;
	return true;
}

bool cpu_avx2_stub(void*)
{
	g_last_call = 2;
	return true;
}

bool gpu_stub(void*, int adapterOrdinal, ExecutionMode fallbackMode)
{
	g_last_call = 3;
	g_last_adapter = adapterOrdinal;
	g_last_fallback = fallbackMode;
	return true;
}

void reset_state()
{
	g_last_call = 0;
	g_last_adapter = -999;
	g_last_fallback = ExecutionMode::CpuNaive;
}

}

int main()
{
	VideoProcessingHandlers handlers{};
	handlers.context = nullptr;
	handlers.cpuNaive = cpu_naive_stub;
	handlers.cpuAvx2 = cpu_avx2_stub;
	handlers.gpuDx11 = gpu_stub;

	UiSelectionSnapshot ui{};
	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = 1;
	reset_state();
	if (!dispatch_from_ui_selection(ui, 2, true, handlers)) {
		return 1;
	}
	if (g_last_call != 3 || g_last_adapter != 0 || g_last_fallback != ExecutionMode::CpuAvx2) {
		return 2;
	}

	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = 9;
	reset_state();
	if (!dispatch_from_ui_selection(ui, 2, true, handlers)) {
		return 3;
	}
	if (g_last_call != 2) {
		return 4;
	}

	ui.modeValue = kModeCpuAvx2;
	ui.gpuAdapterValue = 0;
	reset_state();
	if (!dispatch_from_ui_selection(ui, 0, false, handlers)) {
		return 5;
	}
	if (g_last_call != 1) {
		return 6;
	}

	return 0;
}
