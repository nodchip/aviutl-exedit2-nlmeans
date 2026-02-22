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
#include <vector>
#include <d3dcompiler.h>

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

struct ShaderConstants
{
	UINT width;
	UINT height;
	UINT reserved0;
	UINT reserved1;
};

static const char* PASS_THROUGH_SHADER_SOURCE =
	"cbuffer Constants : register(b0)\n"
	"{\n"
	"	uint Width;\n"
	"	uint Height;\n"
	"	uint Reserved0;\n"
	"	uint Reserved1;\n"
	"};\n"
	"struct PixelData\n"
	"{\n"
	"	int y;\n"
	"	int cb;\n"
	"	int cr;\n"
	"	int reserved;\n"
	"};\n"
	"StructuredBuffer<PixelData> InputPixels : register(t0);\n"
	"RWStructuredBuffer<PixelData> OutputPixels : register(u0);\n"
	"[numthreads(16, 16, 1)]\n"
	"void main(uint3 tid : SV_DispatchThreadID)\n"
	"{\n"
	"	if (tid.x >= Width || tid.y >= Height){\n"
	"		return;\n"
	"	}\n"
	"	const uint index = tid.y * Width + tid.x;\n"
	"	OutputPixels[index] = InputPixels[index];\n"
	"}\n";

}

GpuBackendDx11::GpuBackendDx11() : available(false), bufferWidth(0), bufferHeight(0)
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
	passThroughShader = NULL;
	constantBuffer = NULL;
	inputBuffer = NULL;
	inputSrv = NULL;
	outputBuffer = NULL;
	outputUav = NULL;
	readbackBuffer = NULL;
	bufferWidth = 0;
	bufferHeight = 0;

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

	if (!available){
		return FALSE;
	}

	const int width = fpip.w;
	const int height = fpip.h;
	if (width <= 0 || height <= 0){
		return FALSE;
	}

	if (!ensurePipeline()){
		return FALSE;
	}

	if (!ensureBuffers(width, height)){
		return FALSE;
	}

	D3D11_MAPPED_SUBRESOURCE mappedInput;
	if (FAILED(context->Map(inputBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedInput))){
		return FALSE;
	}

	GpuBackendDx11::GpuPixel* inputPixels = reinterpret_cast<GpuBackendDx11::GpuPixel*>(mappedInput.pData);
	for (int y = 0; y < height; ++y){
		for (int x = 0; x < width; ++x){
			const PIXEL_YC& src = fpip.ycp_edit[y * fpip.max_w + x];
			GpuBackendDx11::GpuPixel& dst = inputPixels[y * width + x];
			dst.y = src.y;
			dst.cb = src.cb;
			dst.cr = src.cr;
			dst.reserved = 0;
		}
	}
	context->Unmap(inputBuffer, 0);

	D3D11_MAPPED_SUBRESOURCE mappedConstants;
	if (FAILED(context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedConstants))){
		return FALSE;
	}
	ShaderConstants* constants = reinterpret_cast<ShaderConstants*>(mappedConstants.pData);
	constants->width = static_cast<UINT>(width);
	constants->height = static_cast<UINT>(height);
	constants->reserved0 = 0;
	constants->reserved1 = 0;
	context->Unmap(constantBuffer, 0);

	ID3D11ShaderResourceView* srvs[1] = { inputSrv };
	ID3D11UnorderedAccessView* uavs[1] = { outputUav };
	ID3D11Buffer* cbs[1] = { constantBuffer };

	context->CSSetShader(passThroughShader, NULL, 0);
	context->CSSetShaderResources(0, 1, srvs);
	context->CSSetUnorderedAccessViews(0, 1, uavs, NULL);
	context->CSSetConstantBuffers(0, 1, cbs);

	const UINT gx = static_cast<UINT>((width + 15) / 16);
	const UINT gy = static_cast<UINT>((height + 15) / 16);
	context->Dispatch(gx, gy, 1);

	ID3D11UnorderedAccessView* nullUav[1] = { NULL };
	ID3D11ShaderResourceView* nullSrv[1] = { NULL };
	context->CSSetUnorderedAccessViews(0, 1, nullUav, NULL);
	context->CSSetShaderResources(0, 1, nullSrv);
	context->CSSetShader(NULL, NULL, 0);

	context->CopyResource(readbackBuffer, outputBuffer);

	D3D11_MAPPED_SUBRESOURCE mappedOutput;
	if (FAILED(context->Map(readbackBuffer, 0, D3D11_MAP_READ, 0, &mappedOutput))){
		return FALSE;
	}

	const GpuBackendDx11::GpuPixel* outputPixels = reinterpret_cast<const GpuBackendDx11::GpuPixel*>(mappedOutput.pData);
	for (int y = 0; y < height; ++y){
		for (int x = 0; x < width; ++x){
			const GpuBackendDx11::GpuPixel& src = outputPixels[y * width + x];
			PIXEL_YC& dst = fpip.ycp_edit[y * fpip.max_w + x];
			dst.y = static_cast<short>(src.y);
			dst.cb = static_cast<short>(src.cb);
			dst.cr = static_cast<short>(src.cr);
		}
	}

	context->Unmap(readbackBuffer, 0);
	return TRUE;
}

const std::vector<std::string>& GpuBackendDx11::getAdapterNames() const
{
	return adapterNames;
}

bool GpuBackendDx11::ensurePipeline()
{
	if (passThroughShader != NULL && constantBuffer != NULL){
		return true;
	}

	CComPtr<ID3DBlob> shaderBlob;
	CComPtr<ID3DBlob> errorBlob;
	if (FAILED(D3DCompile(
		PASS_THROUGH_SHADER_SOURCE,
		strlen(PASS_THROUGH_SHADER_SOURCE),
		"GpuPassThrough.hlsl",
		NULL,
		NULL,
		"main",
		"cs_5_0",
		0,
		0,
		&shaderBlob,
		&errorBlob))){
		return false;
	}

	if (FAILED(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, &passThroughShader))){
		return false;
	}

	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.ByteWidth = sizeof(ShaderConstants);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(device->CreateBuffer(&cbDesc, NULL, &constantBuffer))){
		return false;
	}

	return true;
}

bool GpuBackendDx11::ensureBuffers(int width, int height)
{
	if (inputBuffer != NULL && outputBuffer != NULL && readbackBuffer != NULL && width == bufferWidth && height == bufferHeight){
		return true;
	}

	inputBuffer = NULL;
	inputSrv = NULL;
	outputBuffer = NULL;
	outputUav = NULL;
	readbackBuffer = NULL;
	bufferWidth = width;
	bufferHeight = height;

	const UINT elementCount = static_cast<UINT>(width * height);
	const UINT byteSize = elementCount * sizeof(GpuBackendDx11::GpuPixel);

	D3D11_BUFFER_DESC inputDesc;
	ZeroMemory(&inputDesc, sizeof(inputDesc));
	inputDesc.ByteWidth = byteSize;
	inputDesc.Usage = D3D11_USAGE_DYNAMIC;
	inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	inputDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	inputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	inputDesc.StructureByteStride = sizeof(GpuBackendDx11::GpuPixel);
	if (FAILED(device->CreateBuffer(&inputDesc, NULL, &inputBuffer))){
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = elementCount;
	if (FAILED(device->CreateShaderResourceView(inputBuffer, &srvDesc, &inputSrv))){
		return false;
	}

	D3D11_BUFFER_DESC outputDesc;
	ZeroMemory(&outputDesc, sizeof(outputDesc));
	outputDesc.ByteWidth = byteSize;
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDesc.CPUAccessFlags = 0;
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	outputDesc.StructureByteStride = sizeof(GpuBackendDx11::GpuPixel);
	if (FAILED(device->CreateBuffer(&outputDesc, NULL, &outputBuffer))){
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = elementCount;
	if (FAILED(device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUav))){
		return false;
	}

	D3D11_BUFFER_DESC readbackDesc;
	ZeroMemory(&readbackDesc, sizeof(readbackDesc));
	readbackDesc.ByteWidth = byteSize;
	readbackDesc.Usage = D3D11_USAGE_STAGING;
	readbackDesc.BindFlags = 0;
	readbackDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	readbackDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	readbackDesc.StructureByteStride = sizeof(GpuBackendDx11::GpuPixel);
	if (FAILED(device->CreateBuffer(&readbackDesc, NULL, &readbackBuffer))){
		return false;
	}

	return true;
}
