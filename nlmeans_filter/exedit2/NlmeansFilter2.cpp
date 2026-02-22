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
#include <string>
#include <vector>
#include <intrin.h>
#include <dxgi1_6.h>

#pragma comment(lib, "dxgi.lib")

#if __has_include("../aviutl2_sdk/filter2.h")
#include "../aviutl2_sdk/filter2.h"

namespace {

std::vector<std::wstring> g_gpu_adapter_names;
std::vector<FILTER_ITEM_SELECT::ITEM> g_gpu_adapter_items;
extern FILTER_ITEM_SELECT item_mode;

// 実行バックエンドの選択状態を表す。
enum class ExecutionMode : int {
	CpuNaive = 0,
	CpuAvx2 = 1,
	GpuDx11 = 2
};

// AVX2 命令が利用可能かを判定する。
bool is_avx2_available()
{
	int cpuInfo[4] = {};
	__cpuid(cpuInfo, 0);
	if (cpuInfo[0] < 7) {
		return false;
	}

	__cpuid(cpuInfo, 1);
	const bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
	const bool avx = (cpuInfo[2] & (1 << 28)) != 0;
	if (!osxsave || !avx) {
		return false;
	}

	const unsigned __int64 xcr0 = _xgetbv(0);
	if ((xcr0 & 0x6) != 0x6) {
		return false;
	}

	__cpuidex(cpuInfo, 7, 0);
	return (cpuInfo[1] & (1 << 5)) != 0;
}

// 少なくとも 1 つのハードウェア GPU が列挙できるかを返す。
bool has_hardware_gpu_adapter()
{
	// 先頭は Auto、末尾は終端nullのため 3 要素以上で実アダプタあり。
	return g_gpu_adapter_items.size() >= 3;
}

// 要求モードと環境能力から、実際に実行するモードを決定する。
ExecutionMode resolve_execution_mode(int requested_mode)
{
	if (requested_mode >= static_cast<int>(ExecutionMode::GpuDx11) && has_hardware_gpu_adapter()) {
		return ExecutionMode::GpuDx11;
	}
	if (requested_mode >= static_cast<int>(ExecutionMode::CpuAvx2) && is_avx2_available()) {
		return ExecutionMode::CpuAvx2;
	}
	return ExecutionMode::CpuNaive;
}

// TODO: 既存の ProcessorCpu / ProcessorAvx2 / GpuBackendDx11 を
// ExEdit2 の FILTER_PROC_VIDEO へ接続する。
bool func_proc_video(FILTER_PROC_VIDEO* video)
{
	(void)resolve_execution_mode(item_mode.value);
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

// DXGI から利用可能な GPU アダプタ名を列挙する。
void rebuild_gpu_adapter_list()
{
	g_gpu_adapter_names.clear();
	g_gpu_adapter_items.clear();

	g_gpu_adapter_names.emplace_back(L"Auto");
	g_gpu_adapter_items.push_back({ g_gpu_adapter_names.back().c_str(), 0 });

	IDXGIFactory1* factory = nullptr;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
		g_gpu_adapter_items.push_back({ nullptr, 0 });
		item_gpu_adapter.list = g_gpu_adapter_items.data();
		item_gpu_adapter.value = 0;
		return;
	}

	for (UINT index = 0;; ++index) {
		IDXGIAdapter1* adapter = nullptr;
		if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) {
			break;
		}
		if (adapter == nullptr) {
			continue;
		}

		DXGI_ADAPTER_DESC1 desc = {};
		if (SUCCEEDED(adapter->GetDesc1(&desc))) {
			// ソフトウェアアダプタ以外のみ候補として表示する。
			if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
				g_gpu_adapter_names.emplace_back(desc.Description);
				g_gpu_adapter_items.push_back({
					g_gpu_adapter_names.back().c_str(),
					static_cast<int>(g_gpu_adapter_items.size())
				});
			}
		}

		adapter->Release();
	}

	factory->Release();

	g_gpu_adapter_items.push_back({ nullptr, 0 });
	item_gpu_adapter.list = g_gpu_adapter_items.data();
	item_gpu_adapter.value = 0;
}

}

EXTERN_C __declspec(dllexport) FILTER_PLUGIN_TABLE* GetFilterPluginTable(void)
{
	return &filter_plugin_table;
}

// プラグイン初期化時に GPU アダプタ候補を更新する。
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version)
{
	(void)version;
	rebuild_gpu_adapter_list();
	return true;
}
#endif
