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

#ifndef D3D11_COMPUTE_UTIL_H
#define D3D11_COMPUTE_UTIL_H

#include <d3d11.h>
#include <cstring>

// 動的バッファへデータを書き込む。
inline HRESULT map_write_discard_buffer(
	ID3D11DeviceContext* context,
	ID3D11Buffer* buffer,
	const void* source,
	size_t size_bytes)
{
	if (context == nullptr || buffer == nullptr || source == nullptr) {
		return E_POINTER;
	}
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	const HRESULT hr = context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr)) {
		return hr;
	}
	std::memcpy(mapped.pData, source, size_bytes);
	context->Unmap(buffer, 0);
	return S_OK;
}

// 読み取りバッファからデータを読み出す。
inline HRESULT map_read_buffer(
	ID3D11DeviceContext* context,
	ID3D11Buffer* buffer,
	void* destination,
	size_t size_bytes)
{
	if (context == nullptr || buffer == nullptr || destination == nullptr) {
		return E_POINTER;
	}
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	const HRESULT hr = context->Map(buffer, 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) {
		return hr;
	}
	std::memcpy(destination, mapped.pData, size_bytes);
	context->Unmap(buffer, 0);
	return S_OK;
}

// 1 パスの compute shader を実行し、SRV/UAV を解除する。
inline void dispatch_compute_pass(
	ID3D11DeviceContext* context,
	ID3D11ComputeShader* shader,
	ID3D11ShaderResourceView* srv,
	ID3D11UnorderedAccessView* uav,
	ID3D11Buffer* constant_buffer,
	UINT width,
	UINT height)
{
	if (context == nullptr || shader == nullptr) {
		return;
	}

	ID3D11ShaderResourceView* srvs[1] = { srv };
	ID3D11UnorderedAccessView* uavs[1] = { uav };
	ID3D11Buffer* cbs[1] = { constant_buffer };
	context->CSSetShader(shader, nullptr, 0);
	context->CSSetShaderResources(0, 1, srvs);
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	context->CSSetConstantBuffers(0, 1, cbs);

	const UINT groups_x = (width + 15) / 16;
	const UINT groups_y = (height + 15) / 16;
	context->Dispatch(groups_x, groups_y, 1);

	ID3D11ShaderResourceView* null_srv[1] = { nullptr };
	ID3D11UnorderedAccessView* null_uav[1] = { nullptr };
	context->CSSetShaderResources(0, 1, null_srv);
	context->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);
	context->CSSetShader(nullptr, nullptr, 0);
}

#endif
