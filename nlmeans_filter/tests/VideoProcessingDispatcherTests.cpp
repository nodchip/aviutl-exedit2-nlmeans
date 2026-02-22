// func_proc_video のディスパッチ分岐を GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/VideoProcessingDispatcher.h"

namespace {

int g_last_call = 0;
int g_last_adapter = -999;
ExecutionMode g_last_fallback = ExecutionMode::CpuNaive;
bool g_return_value = true;

bool cpu_naive_stub(void*)
{
	g_last_call = 1;
	return g_return_value;
}

bool cpu_avx2_stub(void*)
{
	g_last_call = 2;
	return g_return_value;
}

bool cpu_fast_stub(void*)
{
	g_last_call = 4;
	return g_return_value;
}

bool gpu_stub(void*, int adapterOrdinal, ExecutionMode fallbackMode)
{
	g_last_call = 3;
	g_last_adapter = adapterOrdinal;
	g_last_fallback = fallbackMode;
	return g_return_value;
}

void reset_state()
{
	g_last_call = 0;
	g_last_adapter = -999;
	g_last_fallback = ExecutionMode::CpuNaive;
	g_return_value = true;
}

}

namespace {

VideoProcessingHandlers make_handlers()
{
	VideoProcessingHandlers handlers{};
	handlers.context = nullptr;
	handlers.cpuNaive = cpu_naive_stub;
	handlers.cpuAvx2 = cpu_avx2_stub;
	handlers.cpuFast = cpu_fast_stub;
	handlers.gpuDx11 = gpu_stub;
	return handlers;
}

}

TEST(VideoProcessingDispatcherTests, DispatchesCpuNaive)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	route.mode = ExecutionMode::CpuNaive;
	route.gpuAdapterOrdinal = -1;
	route.gpuFallbackMode = ExecutionMode::CpuNaive;
	reset_state();
	EXPECT_TRUE(dispatch_video_processing(route, handlers));
	EXPECT_EQ(g_last_call, 1);
}

TEST(VideoProcessingDispatcherTests, DispatchesCpuAvx2)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	route.mode = ExecutionMode::CpuAvx2;
	reset_state();
	EXPECT_TRUE(dispatch_video_processing(route, handlers));
	EXPECT_EQ(g_last_call, 2);
}

TEST(VideoProcessingDispatcherTests, DispatchesGpuDx11WithParameters)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	route.mode = ExecutionMode::GpuDx11;
	route.gpuAdapterOrdinal = 4;
	route.gpuFallbackMode = ExecutionMode::CpuAvx2;
	reset_state();
	EXPECT_TRUE(dispatch_video_processing(route, handlers));
	EXPECT_EQ(g_last_call, 3);
	EXPECT_EQ(g_last_adapter, 4);
	EXPECT_EQ(g_last_fallback, ExecutionMode::CpuAvx2);
}

TEST(VideoProcessingDispatcherTests, DispatchesCpuFast)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	route.mode = ExecutionMode::CpuFast;
	reset_state();
	EXPECT_TRUE(dispatch_video_processing(route, handlers));
	EXPECT_EQ(g_last_call, 4);
}

TEST(VideoProcessingDispatcherTests, UnknownModeFallsBackToCpuNaive)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	route.mode = static_cast<ExecutionMode>(999);
	reset_state();
	EXPECT_TRUE(dispatch_video_processing(route, handlers));
	EXPECT_EQ(g_last_call, 1);
}

TEST(VideoProcessingDispatcherTests, ReturnsFalseWhenHandlerReturnsFalse)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	reset_state();
	g_return_value = false;
	route.mode = ExecutionMode::CpuNaive;
	EXPECT_FALSE(dispatch_video_processing(route, handlers));
}

TEST(VideoProcessingDispatcherTests, ReturnsFalseWithoutCpuNaiveHandler)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	handlers.cpuNaive = nullptr;
	route.mode = ExecutionMode::CpuNaive;
	EXPECT_FALSE(dispatch_video_processing(route, handlers));
}

TEST(VideoProcessingDispatcherTests, ReturnsFalseWithoutGpuHandler)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	handlers.gpuDx11 = nullptr;
	route.mode = ExecutionMode::GpuDx11;
	EXPECT_FALSE(dispatch_video_processing(route, handlers));
}

TEST(VideoProcessingDispatcherTests, ReturnsFalseWithoutCpuFastHandler)
{
	VideoProcessingHandlers handlers = make_handlers();
	ProcessingRoute route{};
	handlers.cpuFast = nullptr;
	route.mode = ExecutionMode::CpuFast;
	EXPECT_FALSE(dispatch_video_processing(route, handlers));
}
