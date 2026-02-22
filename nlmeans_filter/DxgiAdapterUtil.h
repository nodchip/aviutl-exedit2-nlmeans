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

#ifndef DXGI_ADAPTER_UTIL_H
#define DXGI_ADAPTER_UTIL_H

#include <vector>
#include <dxgi1_2.h>
#include <atlbase.h>

// ソフトウェアを除いたハードウェアアダプタ一覧を列挙する。
inline void enumerate_hardware_adapters(
	IDXGIFactory1* factory,
	std::vector<CComPtr<IDXGIAdapter1>>& adapters,
	std::vector<DXGI_ADAPTER_DESC1>* adapter_descs = nullptr)
{
	adapters.clear();
	if (adapter_descs != nullptr) {
		adapter_descs->clear();
	}
	if (factory == nullptr) {
		return;
	}

	for (UINT index = 0;; ++index) {
		CComPtr<IDXGIAdapter1> adapter;
		if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) {
			break;
		}

		DXGI_ADAPTER_DESC1 desc = {};
		if (FAILED(adapter->GetDesc1(&desc))) {
			continue;
		}
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		adapters.push_back(adapter);
		if (adapter_descs != nullptr) {
			adapter_descs->push_back(desc);
		}
	}
}

// 優先番号に基づいてアダプタを選択する。見つからない場合は先頭を返す。
inline CComPtr<IDXGIAdapter1> select_hardware_adapter(
	const std::vector<CComPtr<IDXGIAdapter1>>& adapters,
	int preferred_index,
	int* resolved_index = nullptr)
{
	if (resolved_index != nullptr) {
		*resolved_index = -1;
	}
	if (adapters.empty()) {
		return CComPtr<IDXGIAdapter1>();
	}
	if (preferred_index >= 0 && preferred_index < static_cast<int>(adapters.size())) {
		if (resolved_index != nullptr) {
			*resolved_index = preferred_index;
		}
		return adapters[static_cast<size_t>(preferred_index)];
	}
	return adapters[0];
}

#endif
