// Copyright 2026 nodchip
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ExEdit2 向けフィルタプラグインの入口。
// 現時点では SDK 統合の下地のみ実装し、実処理は今後移植する。

#include <windows.h>
#include <cstdint>

#if __has_include("../aviutl2_sdk/filter2.h")
#include "../aviutl2_sdk/filter2.h"

namespace {

// TODO: 既存の ProcessorCpu / ProcessorAvx2 / GpuBackendDx11 を
// ExEdit2 の FILTER_PROC_VIDEO へ接続する。
bool func_proc_video(FILTER_PROC_VIDEO* video)
{
	(void)video;
	return true;
}

FILTER_ITEM_TRACK item_search_radius = FILTER_ITEM_TRACK(L"空間範囲", 3.0, 1.0, 16.0, 1.0);
FILTER_ITEM_TRACK item_time_radius = FILTER_ITEM_TRACK(L"時間範囲", 0.0, 0.0, 7.0, 1.0);
FILTER_ITEM_TRACK item_sigma = FILTER_ITEM_TRACK(L"分散", 50.0, 0.0, 100.0, 1.0);
FILTER_ITEM_SELECT::ITEM item_mode_list[] = {
	{ L"CPU (Naive)", 0 },
	{ L"CPU (AVX2)", 1 },
	{ L"GPU (DirectX 11)", 2 },
	{ nullptr }
};
FILTER_ITEM_SELECT item_mode = FILTER_ITEM_SELECT(L"計算モード", 2, item_mode_list);
FILTER_ITEM_SELECT::ITEM item_gpu_adapter_list[] = {
	{ L"Auto", 0 },
	{ nullptr }
};
FILTER_ITEM_SELECT item_gpu_adapter = FILTER_ITEM_SELECT(L"GPUアダプタ", 0, item_gpu_adapter_list);
void* items[] = {
	&item_search_radius,
	&item_time_radius,
	&item_sigma,
	&item_mode,
	&item_gpu_adapter,
	nullptr
};

FILTER_PLUGIN_TABLE filter_plugin_table = {
	FILTER_PLUGIN_TABLE::FLAG_VIDEO,
	L"NL-Means Filter (ExEdit2)",
	L"NL-Means",
	L"NL-Means Filter for AviUtl ExEdit2 (WIP: CPU/GPU backend migration)",
	items,
	func_proc_video,
	nullptr
};

}

EXTERN_C __declspec(dllexport) FILTER_PLUGIN_TABLE* GetFilterPluginTable(void)
{
	return &filter_plugin_table;
}
#endif
