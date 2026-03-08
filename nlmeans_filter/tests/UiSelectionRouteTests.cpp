// UI 設定値と実行経路の結合ロジックを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/UiSelectionRoute.h"

TEST(UiSelectionRouteTests, GpuAutoSelectionResolvesToGpu)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = 0;

	const ProcessingRoute route = resolve_route_from_ui_selection(ui, 2, true);
	EXPECT_EQ(route.mode, ExecutionMode::GpuDx11);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}

TEST(UiSelectionRouteTests, CpuAvx2ModeFallsBackToCpuNaiveWithoutAvx2)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeCpuAvx2;
	ui.gpuAdapterValue = 0;

	const ProcessingRoute route = resolve_route_from_ui_selection(ui, 0, false);
	EXPECT_EQ(route.mode, ExecutionMode::CpuNaive);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuNaive);
}

TEST(UiSelectionRouteTests, InvalidGpuAdapterFallsBackToCpuAvx2)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = 3;

	const ProcessingRoute route = resolve_route_from_ui_selection(ui, 2, true);
	EXPECT_EQ(route.mode, ExecutionMode::CpuAvx2);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}

TEST(UiSelectionRouteTests, NegativeGpuAdapterIsTreatedAsAuto)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeGpuDx11;
	ui.gpuAdapterValue = -1;

	const ProcessingRoute route = resolve_route_from_ui_selection(ui, 2, true);
	EXPECT_EQ(route.mode, ExecutionMode::GpuDx11);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}

TEST(UiSelectionRouteTests, NegativeModeDefaultsToCpuNaive)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = -1;
	ui.gpuAdapterValue = 0;

	const ProcessingRoute route = resolve_route_from_ui_selection(ui, 2, true);
	EXPECT_EQ(route.mode, ExecutionMode::CpuNaive);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}

TEST(UiSelectionRouteTests, UnknownModeFallsBackToCpuAvx2)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = 999;
	ui.gpuAdapterValue = 1;

	const ProcessingRoute route = resolve_route_from_ui_selection(ui, 2, true);
	EXPECT_EQ(route.mode, ExecutionMode::CpuAvx2);
	EXPECT_EQ(route.gpuAdapterOrdinal, 0);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}

TEST(UiSelectionRouteTests, CpuFastModeResolvesToCpuFast)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeCpuFast;
	ui.gpuAdapterValue = 0;

	const ProcessingRoute route = resolve_route_from_ui_selection(ui, 0, true);
	EXPECT_EQ(route.mode, ExecutionMode::CpuFast);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}

TEST(UiSelectionRouteTests, CpuTemporalModeResolvesToCpuTemporal)
{
	UiSelectionSnapshot ui{};
	ui.modeValue = kModeCpuTemporal;
	ui.gpuAdapterValue = 0;

	const ProcessingRoute route = resolve_route_from_ui_selection(ui, 0, true);
	EXPECT_EQ(route.mode, ExecutionMode::CpuTemporal);
	EXPECT_EQ(route.gpuAdapterOrdinal, -1);
	EXPECT_EQ(route.gpuFallbackMode, ExecutionMode::CpuAvx2);
}
