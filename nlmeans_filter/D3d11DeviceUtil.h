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

#ifndef D3D11_DEVICE_UTIL_H
#define D3D11_DEVICE_UTIL_H

#include <vector>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <atlbase.h>
#include "DxgiAdapterUtil.h"

// 優先アダプタ番号に基づいて D3D11 デバイスを作成する。
inline bool create_d3d11_device_with_preferred_adapter(
	int preferred_adapter_index,
	ID3D11Device** out_device,
	ID3D11DeviceContext** out_context,
	int* out_resolved_adapter_index = nullptr,
	std::vector<DXGI_ADAPTER_DESC1>* out_hardware_adapter_descs = nullptr)
{
	if (out_device == nullptr || out_context == nullptr) {
		return false;
	}
	*out_device = nullptr;
	*out_context = nullptr;
	if (out_resolved_adapter_index != nullptr) {
		*out_resolved_adapter_index = -1;
	}
	if (out_hardware_adapter_descs != nullptr) {
		out_hardware_adapter_descs->clear();
	}

	CComPtr<IDXGIFactory1> factory;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
		return false;
	}

	std::vector<CComPtr<IDXGIAdapter1>> hardware_adapters;
	std::vector<DXGI_ADAPTER_DESC1> hardware_descs;
	enumerate_hardware_adapters(factory, hardware_adapters, &hardware_descs);

	int resolved_index = -1;
	CComPtr<IDXGIAdapter1> selected_adapter =
		select_hardware_adapter(hardware_adapters, preferred_adapter_index, &resolved_index);

	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	D3D_FEATURE_LEVEL output_level = D3D_FEATURE_LEVEL_11_0;
	IDXGIAdapter* raw_adapter = selected_adapter;
	const D3D_DRIVER_TYPE driver_type =
		(raw_adapter != nullptr) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

	if (FAILED(D3D11CreateDevice(
		raw_adapter,
		driver_type,
		nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		levels,
		static_cast<UINT>(std::size(levels)),
		D3D11_SDK_VERSION,
		out_device,
		&output_level,
		out_context))) {
		return false;
	}

	if (out_resolved_adapter_index != nullptr) {
		*out_resolved_adapter_index = resolved_index;
	}
	if (out_hardware_adapter_descs != nullptr) {
		*out_hardware_adapter_descs = hardware_descs;
	}

	return true;
}

#endif
