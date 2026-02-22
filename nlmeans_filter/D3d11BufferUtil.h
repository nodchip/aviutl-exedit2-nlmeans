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

#ifndef D3D11_BUFFER_UTIL_H
#define D3D11_BUFFER_UTIL_H

#include <d3d11.h>

// 構造化バッファを作成する。
inline HRESULT create_structured_buffer(
	ID3D11Device* device,
	UINT byte_size,
	UINT structure_stride,
	D3D11_USAGE usage,
	UINT bind_flags,
	UINT cpu_access_flags,
	ID3D11Buffer** out_buffer)
{
	if (device == nullptr || out_buffer == nullptr) {
		return E_POINTER;
	}

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byte_size;
	desc.Usage = usage;
	desc.BindFlags = bind_flags;
	desc.CPUAccessFlags = cpu_access_flags;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = structure_stride;
	return device->CreateBuffer(&desc, nullptr, out_buffer);
}

// 構造化バッファ向け SRV を作成する。
inline HRESULT create_structured_srv(
	ID3D11Device* device,
	ID3D11Buffer* buffer,
	UINT num_elements,
	ID3D11ShaderResourceView** out_srv)
{
	if (device == nullptr || buffer == nullptr || out_srv == nullptr) {
		return E_POINTER;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = 0;
	desc.Buffer.NumElements = num_elements;
	return device->CreateShaderResourceView(buffer, &desc, out_srv);
}

// 構造化バッファ向け UAV を作成する。
inline HRESULT create_structured_uav(
	ID3D11Device* device,
	ID3D11Buffer* buffer,
	UINT num_elements,
	ID3D11UnorderedAccessView** out_uav)
{
	if (device == nullptr || buffer == nullptr || out_uav == nullptr) {
		return E_POINTER;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = 0;
	desc.Buffer.NumElements = num_elements;
	return device->CreateUnorderedAccessView(buffer, &desc, out_uav);
}

#endif
