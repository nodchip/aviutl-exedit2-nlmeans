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
#include "ShaderCompileUtil.h"
#include <cmath>
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
	UINT searchRadius;
	UINT frameCount;
	UINT currentFrameIndex;
	UINT reserved0;
	UINT reserved1;
	UINT reserved2;
	float h2;
	float reserved3;
	float reserved4;
	float reserved5;
};

static const char* NLM_SHADER_SOURCE =
	"cbuffer Constants : register(b0)\n"
	"{\n"
	"	uint Width;\n"
	"	uint Height;\n"
	"	uint SearchRadius;\n"
	"	uint FrameCount;\n"
	"	uint CurrentFrameIndex;\n"
	"	uint Reserved0;\n"
	"	uint Reserved1;\n"
	"	uint Reserved2;\n"
	"	float H2;\n"
	"	float Reserved3;\n"
	"	float Reserved4;\n"
	"	float Reserved5;\n"
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
	"int clampi(int v, int low, int high)\n"
	"{\n"
	"	return min(high, max(low, v));\n"
	"}\n"
	"int getChannel(PixelData p, int channel)\n"
	"{\n"
	"	if (channel == 0) return p.y;\n"
	"	if (channel == 1) return p.cb;\n"
	"	return p.cr;\n"
	"}\n"
	"int readChannel(int x, int y, int t, int channel)\n"
	"{\n"
	"	const int cx = clampi(x, 0, (int)Width - 1);\n"
	"	const int cy = clampi(y, 0, (int)Height - 1);\n"
	"	const int ct = clampi(t, 0, (int)FrameCount - 1);\n"
	"	const uint frameOffset = (uint)ct * Width * Height;\n"
	"	return getChannel(InputPixels[frameOffset + cy * Width + cx], channel);\n"
	"}\n"
	"[numthreads(16, 16, 1)]\n"
	"void main(uint3 tid : SV_DispatchThreadID)\n"
	"{\n"
	"	if (tid.x >= Width || tid.y >= Height){\n"
	"		return;\n"
	"	}\n"
	"	const int x = (int)tid.x;\n"
	"	const int y = (int)tid.y;\n"
	"	const uint index = tid.y * Width + tid.x;\n"
	"	PixelData outPixel;\n"
	"	outPixel.reserved = 0;\n"
	"	for (int channel = 0; channel < 3; ++channel){\n"
	"		float sum = 0.0f;\n"
	"		float value = 0.0f;\n"
	"		for (int dy = -((int)SearchRadius); dy <= (int)SearchRadius; ++dy){\n"
	"			for (int dx = -((int)SearchRadius); dx <= (int)SearchRadius; ++dx){\n"
	"				for (int dt = 0; dt < (int)FrameCount; ++dt){\n"
	"					float sum2 = 0.0f;\n"
	"					for (int ny = -1; ny <= 1; ++ny){\n"
	"						for (int nx = -1; nx <= 1; ++nx){\n"
	"							const float a = (float)readChannel(x + nx, y + ny, (int)CurrentFrameIndex, channel);\n"
	"							const float b = (float)readChannel(x + dx + nx, y + dy + ny, dt, channel);\n"
	"							const float diff = a - b;\n"
	"							sum2 += diff * diff;\n"
	"						}\n"
	"					}\n"
	"					const float w = exp(-sum2 * H2);\n"
	"					sum += w;\n"
	"					value += (float)readChannel(x + dx, y + dy, dt, channel) * w;\n"
	"				}\n"
	"			}\n"
	"		}\n"
	"		const float filtered = (sum > 0.0f) ? (value / sum) : (float)readChannel(x, y, (int)CurrentFrameIndex, channel);\n"
	"		const int v = (int)filtered;\n"
	"		if (channel == 0) outPixel.y = v;\n"
	"		if (channel == 1) outPixel.cb = v;\n"
	"		if (channel == 2) outPixel.cr = v;\n"
	"	}\n"
	"	OutputPixels[index] = outPixel;\n"
	"}\n";

}

GpuBackendDx11::GpuBackendDx11() : available(false), preferredAdapterIndex(-1), bufferWidth(0), bufferHeight(0), bufferFrames(0)
{
}

GpuBackendDx11::~GpuBackendDx11()
{
}

void GpuBackendDx11::setPreferredAdapterIndex(int adapterIndex)
{
	preferredAdapterIndex = adapterIndex;
}

bool GpuBackendDx11::initialize(int requestedAdapterIndex)
{
	if (requestedAdapterIndex >= 0){
		preferredAdapterIndex = requestedAdapterIndex;
	}

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
	bufferFrames = 0;

	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory)))){
		return false;
	}

	std::vector<CComPtr<IDXGIAdapter1>> hardwareAdapters;
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
		hardwareAdapters.push_back(adapter);
	}

	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	D3D_FEATURE_LEVEL outputLevel = D3D_FEATURE_LEVEL_11_0;
	const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	CComPtr<IDXGIAdapter1> selectedAdapter;
	if (preferredAdapterIndex >= 0 && preferredAdapterIndex < static_cast<int>(hardwareAdapters.size())){
		selectedAdapter = hardwareAdapters[static_cast<size_t>(preferredAdapterIndex)];
	} else if (!hardwareAdapters.empty()){
		selectedAdapter = hardwareAdapters[0];
	}

	IDXGIAdapter* rawAdapter = selectedAdapter;
	const D3D_DRIVER_TYPE driverType = (rawAdapter != NULL) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
	const HRESULT hr = D3D11CreateDevice(
		rawAdapter,
		driverType,
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
	if (!available){
		return FALSE;
	}

	const int width = fpip.w;
	const int height = fpip.h;
	const int timeSearchRadius = fp.track[1];
	const int frameCount = timeSearchRadius * 2 + 1;
	if (width <= 0 || height <= 0 || frameCount <= 0){
		return FALSE;
	}

	if (!ensurePipeline()){
		return FALSE;
	}

	if (!ensureBuffers(width, height, frameCount)){
		return FALSE;
	}

	const int currentFrame = fp.exfunc->get_frame(fpip.editp);
	const int totalFrames = fp.exfunc->get_frame_n(fpip.editp);
	fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, frameCount, NULL);

	std::vector<PIXEL_YC*> frames(frameCount, NULL);
	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		const int idx = dt + timeSearchRadius;
		const int targetFrame = max(0, min(totalFrames - 1, currentFrame + dt));
		frames[idx] = fp.exfunc->get_ycp_filtering_cache_ex(&fp, fpip.editp, targetFrame, NULL, NULL);
		if (frames[idx] == NULL){
			return FALSE;
		}
	}

	D3D11_MAPPED_SUBRESOURCE mappedInput;
	if (FAILED(context->Map(inputBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedInput))){
		return FALSE;
	}

	GpuBackendDx11::GpuPixel* inputPixels = reinterpret_cast<GpuBackendDx11::GpuPixel*>(mappedInput.pData);
	for (int t = 0; t < frameCount; ++t){
		const PIXEL_YC* frame = frames[t];
		for (int y = 0; y < height; ++y){
			for (int x = 0; x < width; ++x){
				const PIXEL_YC& src = frame[y * fpip.max_w + x];
				const int offset = (t * height + y) * width + x;
				GpuBackendDx11::GpuPixel& dst = inputPixels[offset];
				dst.y = src.y;
				dst.cb = src.cb;
				dst.cr = src.cr;
				dst.reserved = 0;
			}
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
	constants->searchRadius = static_cast<UINT>(fp.track[0]);
	constants->frameCount = static_cast<UINT>(frameCount);
	constants->currentFrameIndex = static_cast<UINT>(timeSearchRadius);
	constants->reserved0 = 0;
	constants->reserved1 = 0;
	constants->reserved2 = 0;
	const double h = std::pow(10.0, static_cast<double>(fp.track[2]) / 22.0);
	constants->h2 = static_cast<float>(1.0 / (h * h));
	constants->reserved3 = 0;
	constants->reserved4 = 0;
	constants->reserved5 = 0;
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
			dst.y = static_cast<short>(max(-32768, min(32767, src.y)));
			dst.cb = static_cast<short>(max(-32768, min(32767, src.cb)));
			dst.cr = static_cast<short>(max(-32768, min(32767, src.cr)));
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
	if (!compile_compute_shader_from_file_or_embedded(
		reinterpret_cast<HMODULE>(&::__ImageBase),
		L"gpu_nlm_cs.hlsl",
		NLM_SHADER_SOURCE,
		"GpuNlmEmbedded.hlsl",
		&shaderBlob,
		&errorBlob)) {
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

bool GpuBackendDx11::ensureBuffers(int width, int height, int frames)
{
	if (inputBuffer != NULL && outputBuffer != NULL && readbackBuffer != NULL
		&& width == bufferWidth && height == bufferHeight && frames == bufferFrames){
		return true;
	}

	inputBuffer = NULL;
	inputSrv = NULL;
	outputBuffer = NULL;
	outputUav = NULL;
	readbackBuffer = NULL;
	bufferWidth = width;
	bufferHeight = height;
	bufferFrames = frames;

	const UINT outputElementCount = static_cast<UINT>(width * height);
	const UINT outputByteSize = outputElementCount * sizeof(GpuBackendDx11::GpuPixel);
	const UINT inputElementCount = static_cast<UINT>(width * height * frames);
	const UINT inputByteSize = inputElementCount * sizeof(GpuBackendDx11::GpuPixel);

	D3D11_BUFFER_DESC inputDesc;
	ZeroMemory(&inputDesc, sizeof(inputDesc));
	inputDesc.ByteWidth = inputByteSize;
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
	srvDesc.Buffer.NumElements = inputElementCount;
	if (FAILED(device->CreateShaderResourceView(inputBuffer, &srvDesc, &inputSrv))){
		return false;
	}

	D3D11_BUFFER_DESC outputDesc;
	ZeroMemory(&outputDesc, sizeof(outputDesc));
	outputDesc.ByteWidth = outputByteSize;
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
	uavDesc.Buffer.NumElements = outputElementCount;
	if (FAILED(device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUav))){
		return false;
	}

	D3D11_BUFFER_DESC readbackDesc;
	ZeroMemory(&readbackDesc, sizeof(readbackDesc));
	readbackDesc.ByteWidth = outputByteSize;
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
