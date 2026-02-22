// func_proc_video のディスパッチ分岐を検証する。
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

int main()
{
	VideoProcessingHandlers handlers{};
	handlers.context = nullptr;
	handlers.cpuNaive = cpu_naive_stub;
	handlers.cpuAvx2 = cpu_avx2_stub;
	handlers.gpuDx11 = gpu_stub;

	ProcessingRoute route{};
	route.mode = ExecutionMode::CpuNaive;
	route.gpuAdapterOrdinal = -1;
	route.gpuFallbackMode = ExecutionMode::CpuNaive;
	reset_state();
	if (!dispatch_video_processing(route, handlers) || g_last_call != 1) {
		return 1;
	}

	route.mode = ExecutionMode::CpuAvx2;
	reset_state();
	if (!dispatch_video_processing(route, handlers) || g_last_call != 2) {
		return 2;
	}

	route.mode = ExecutionMode::GpuDx11;
	route.gpuAdapterOrdinal = 4;
	route.gpuFallbackMode = ExecutionMode::CpuAvx2;
	reset_state();
	if (!dispatch_video_processing(route, handlers) || g_last_call != 3 || g_last_adapter != 4 || g_last_fallback != ExecutionMode::CpuAvx2) {
		return 3;
	}

	route.mode = static_cast<ExecutionMode>(999);
	reset_state();
	if (!dispatch_video_processing(route, handlers) || g_last_call != 1) {
		return 4;
	}

	reset_state();
	g_return_value = false;
	route.mode = ExecutionMode::CpuNaive;
	if (dispatch_video_processing(route, handlers)) {
		return 5;
	}

	return 0;
}
