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

#include "stdafx.h"
#include "GpuBackendDx11.h"
#include <string>

namespace {

static std::string wideToUtf8(const wchar_t* source)
{
	if (source == NULL || *source == L'\0'){
		return std::string();
	}

	const int required = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
	if (required <= 1){
		return std::string();
	}

	std::string result(required - 1, '\0');
	(void)WideCharToMultiByte(CP_UTF8, 0, source, -1, &result[0], required, NULL, NULL);
	return result;
}

}

GpuBackendDx11::GpuBackendDx11() : available(false)
{
}

GpuBackendDx11::~GpuBackendDx11()
{
}

bool GpuBackendDx11::initialize()
{
	available = false;
	adapterNames.clear();
	device = NULL;
	context = NULL;
	dxgiFactory = NULL;

	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory)))){
		return false;
	}

	for (UINT i = 0;; ++i){
		CComPtr<IDXGIAdapter1> adapter;
		if (dxgiFactory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND){
			break;
		}

		DXGI_ADAPTER_DESC1 desc;
		if (FAILED(adapter->GetDesc1(&desc))){
			continue;
		}

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE){
			continue;
		}

		adapterNames.push_back(wideToUtf8(desc.Description));
	}

	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	D3D_FEATURE_LEVEL outputLevel = D3D_FEATURE_LEVEL_11_0;
	const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	const HRESULT hr = D3D11CreateDevice(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		flags,
		levels,
		2,
		D3D11_SDK_VERSION,
		&device,
		&outputLevel,
		&context
	);

	if (FAILED(hr)){
		return false;
	}

	available = true;
	return true;
}

bool GpuBackendDx11::isAvailable() const
{
	return available;
}

BOOL GpuBackendDx11::process(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	(void)fp;
	(void)fpip;
	return FALSE;
}

const std::vector<std::string>& GpuBackendDx11::getAdapterNames() const
{
	return adapterNames;
}
