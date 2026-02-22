// UI 選択値からディスパッチまでの統合を GoogleTest で検証する。
#include <gtest/gtest.h>
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

VideoProcessingHandlers make_handlers()
{
	VideoProcessingHandlers handlers{};
	handlers.context = nullptr;
	handlers.cpuNaive = cpu_naive_stub;
	handlers.cpuAvx2 = cpu_avx2_stub;
	handlers.gpuDx11 = gpu_stub;
	return handlers;
}

}

TEST(UiToDispatcherIntegrationTests, ValidGpuSelectionDispatchesGpu)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = 1;

	reset_state();
	EXPECT_TRUE(dispatch_from_ui_selection(ui, 2, true, make_handlers()));
	EXPECT_EQ(g_last_call, 3);
	EXPECT_EQ(g_last_adapter, 0);
	EXPECT_EQ(g_last_fallback, ExecutionMode::CpuAvx2);
}

TEST(UiToDispatcherIntegrationTests, InvalidGpuSelectionFallsBackToCpuAvx2)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = 9;

	reset_state();
	EXPECT_TRUE(dispatch_from_ui_selection(ui, 2, true, make_handlers()));
	EXPECT_EQ(g_last_call, 2);
}

TEST(UiToDispatcherIntegrationTests, CpuAvx2WithoutSupportFallsBackToCpuNaive)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeCpuAvx2;
	ui.gpuAdapterValue = 0;

	reset_state();
	EXPECT_TRUE(dispatch_from_ui_selection(ui, 0, false, make_handlers()));
	EXPECT_EQ(g_last_call, 1);
}
