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

#ifndef GPU_BACKEND_DX11_H
#define GPU_BACKEND_DX11_H

#include <vector>
#include <string>
#include <cstdint>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <atlbase.h>
#include "filter.hpp"

// DirectX 11 系 GPU バックエンドの初期化状態を管理するクラス。
class GpuBackendDx11
{
public:
	GpuBackendDx11();
	~GpuBackendDx11();

	// 利用可能な GPU を列挙し、既定デバイスを初期化する。
	bool initialize();

	// バックエンドが利用可能かを返す。
	bool isAvailable() const;

	// GPU 処理本体。現時点では未実装のため FALSE を返す。
	BOOL process(FILTER& fp, FILTER_PROC_INFO& fpip);

	// 列挙した GPU 名を返す。
	const std::vector<std::string>& getAdapterNames() const;

private:
	struct alignas(16) GpuPixel
	{
		int32_t y;
		int32_t cb;
		int32_t cr;
		int32_t reserved;
	};

	bool ensurePipeline();
	bool ensureBuffers(int width, int height);

	bool available;
	std::vector<std::string> adapterNames;
	CComPtr<IDXGIFactory1> dxgiFactory;
	CComPtr<ID3D11Device> device;
	CComPtr<ID3D11DeviceContext> context;
	CComPtr<ID3D11ComputeShader> passThroughShader;
	CComPtr<ID3D11Buffer> constantBuffer;
	CComPtr<ID3D11Buffer> inputBuffer;
	CComPtr<ID3D11ShaderResourceView> inputSrv;
	CComPtr<ID3D11Buffer> outputBuffer;
	CComPtr<ID3D11UnorderedAccessView> outputUav;
	CComPtr<ID3D11Buffer> readbackBuffer;
	int bufferWidth;
	int bufferHeight;
};

#endif
