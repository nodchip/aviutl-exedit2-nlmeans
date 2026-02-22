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

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
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
std::vector<PIXEL_RGBA> g_input_pixels;
std::vector<PIXEL_RGBA> g_output_pixels;
extern FILTER_ITEM_TRACK item_search_radius;
extern FILTER_ITEM_TRACK item_sigma;
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

// ExEdit2 の画像バッファへ CPU Naive な NLM を適用する。
bool apply_nlm_cpu_naive(FILTER_PROC_VIDEO* video)
{
	if (video == nullptr || video->scene == nullptr || video->get_image_data == nullptr || video->set_image_data == nullptr) {
		return false;
	}

	const int width = video->scene->width;
	const int height = video->scene->height;
	if (width <= 0 || height <= 0) {
		return false;
	}

	const int pixel_count = width * height;
	g_input_pixels.resize(static_cast<size_t>(pixel_count));
	g_output_pixels.resize(static_cast<size_t>(pixel_count));

	video->get_image_data(g_input_pixels.data());

	const int search_radius = std::max(1, static_cast<int>(item_search_radius.value));
	const double sigma = std::max(0.001, static_cast<double>(item_sigma.value));
	const double sigma2 = sigma * sigma;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int center_index = y * width + x;
			const PIXEL_RGBA& center = g_input_pixels[static_cast<size_t>(center_index)];

			double sum_r = 0.0;
			double sum_g = 0.0;
			double sum_b = 0.0;
			double sum_w = 0.0;

			for (int dy = -search_radius; dy <= search_radius; ++dy) {
				const int sy = std::clamp(y + dy, 0, height - 1);
				for (int dx = -search_radius; dx <= search_radius; ++dx) {
					const int sx = std::clamp(x + dx, 0, width - 1);
					const PIXEL_RGBA& sample = g_input_pixels[static_cast<size_t>(sy * width + sx)];

					const double dr = static_cast<double>(sample.r) - static_cast<double>(center.r);
					const double dg = static_cast<double>(sample.g) - static_cast<double>(center.g);
					const double db = static_cast<double>(sample.b) - static_cast<double>(center.b);
					const double dist2 = dr * dr + dg * dg + db * db;
					const double w = std::exp(-dist2 / sigma2);

					sum_r += w * static_cast<double>(sample.r);
					sum_g += w * static_cast<double>(sample.g);
					sum_b += w * static_cast<double>(sample.b);
					sum_w += w;
				}
			}

			PIXEL_RGBA out = center;
			if (sum_w > 0.0) {
				out.r = static_cast<unsigned char>(std::clamp(sum_r / sum_w, 0.0, 255.0));
				out.g = static_cast<unsigned char>(std::clamp(sum_g / sum_w, 0.0, 255.0));
				out.b = static_cast<unsigned char>(std::clamp(sum_b / sum_w, 0.0, 255.0));
			}
			g_output_pixels[static_cast<size_t>(center_index)] = out;
		}
	}

	video->set_image_data(g_output_pixels.data(), width, height);
	return true;
}

// TODO: ExEdit2 向けに AVX2/GPU 実処理を接続する。
bool func_proc_video(FILTER_PROC_VIDEO* video)
{
	const ExecutionMode mode = resolve_execution_mode(item_mode.value);
	switch (mode) {
	case ExecutionMode::CpuNaive:
		return apply_nlm_cpu_naive(video);
	case ExecutionMode::CpuAvx2:
		// 現時点では AVX2 未接続のため Naive 実装へフォールバックする。
		return apply_nlm_cpu_naive(video);
	case ExecutionMode::GpuDx11:
		// 現時点では GPU 未接続のため Naive 実装へフォールバックする。
		return apply_nlm_cpu_naive(video);
	default:
		return apply_nlm_cpu_naive(video);
	}
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
