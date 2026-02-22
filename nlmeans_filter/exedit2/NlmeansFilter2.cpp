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
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <immintrin.h>
#include <intrin.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <atlbase.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#if __has_include("../aviutl2_sdk/filter2.h")
#include "../aviutl2_sdk/filter2.h"
#include "../ShaderCompileUtil.h"

namespace {

extern "C" IMAGE_DOS_HEADER __ImageBase;

std::vector<std::wstring> g_gpu_adapter_names;
std::vector<FILTER_ITEM_SELECT::ITEM> g_gpu_adapter_items;
std::vector<PIXEL_RGBA> g_input_pixels;
std::vector<PIXEL_RGBA> g_output_pixels;
int g_runtime_gpu_adapter_ordinal = -1;
extern FILTER_ITEM_TRACK item_search_radius;
extern FILTER_ITEM_TRACK item_time_radius;
extern FILTER_ITEM_TRACK item_sigma;
extern FILTER_ITEM_SELECT item_mode;
extern FILTER_ITEM_SELECT item_gpu_adapter;

// 実行バックエンドの選択状態を表す。
enum class ExecutionMode : int {
	CpuNaive = 0,
	CpuAvx2 = 1,
	GpuDx11 = 2
};

// ExEdit2 用の GPU 実行コンテキスト。
class Exedit2GpuRunner
{
public:
	Exedit2GpuRunner()
		: activeAdapterOrdinal(-2), width(0), height(0), bufferedFrameCount(0)
	{
	}

	// 指定したアダプタ番号で初期化する。-1 は自動選択。
	bool initialize(int adapterOrdinal)
	{
		if (device != nullptr && context != nullptr && activeAdapterOrdinal == adapterOrdinal) {
			return true;
		}

		clearResources();

		CComPtr<IDXGIFactory1> factory;
		if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
			return false;
		}

		std::vector<CComPtr<IDXGIAdapter1>> hardwareAdapters;
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
			hardwareAdapters.push_back(adapter);
		}

		CComPtr<IDXGIAdapter1> selectedAdapter;
		if (!hardwareAdapters.empty()) {
			if (adapterOrdinal >= 0 && adapterOrdinal < static_cast<int>(hardwareAdapters.size())) {
				selectedAdapter = hardwareAdapters[static_cast<size_t>(adapterOrdinal)];
				activeAdapterOrdinal = adapterOrdinal;
			} else {
				selectedAdapter = hardwareAdapters[0];
				activeAdapterOrdinal = -1;
			}
		} else {
			activeAdapterOrdinal = -1;
		}

		static const D3D_FEATURE_LEVEL levels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};
		D3D_FEATURE_LEVEL outputLevel = D3D_FEATURE_LEVEL_11_0;
		IDXGIAdapter* rawAdapter = selectedAdapter;
		const D3D_DRIVER_TYPE driverType = rawAdapter != nullptr ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
		if (FAILED(D3D11CreateDevice(
			rawAdapter,
			driverType,
			nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			levels,
			static_cast<UINT>(std::size(levels)),
			D3D11_SDK_VERSION,
			&device,
			&outputLevel,
			&context))) {
			clearResources();
			return false;
		}

		if (!ensurePipeline()) {
			clearResources();
			return false;
		}

		return true;
	}

	// RGBA フレーム列に空間/時間 NLM を適用する。
	bool process(
		const PIXEL_RGBA* inputPixels,
		PIXEL_RGBA* outputPixels,
		int imageWidth,
		int imageHeight,
		int searchRadius,
		int timeRadius,
		double sigmaValue)
	{
		if (device == nullptr || context == nullptr || inputPixels == nullptr || outputPixels == nullptr) {
			return false;
		}
		if (imageWidth <= 0 || imageHeight <= 0) {
			return false;
		}
		const int clampedTimeRadius = std::max(0, timeRadius);
		const int requiredHistory = clampedTimeRadius + 1;

		const size_t pixelCount = static_cast<size_t>(imageWidth) * static_cast<size_t>(imageHeight);
		if (width != imageWidth || height != imageHeight) {
			historyFrames.clear();
		}

		std::vector<PIXEL_RGBA> currentFrame(pixelCount);
		std::memcpy(currentFrame.data(), inputPixels, pixelCount * sizeof(PIXEL_RGBA));
		historyFrames.push_back(std::move(currentFrame));
		while (historyFrames.size() > static_cast<size_t>(requiredHistory)) {
			historyFrames.pop_front();
		}

		const int frameCount = static_cast<int>(historyFrames.size());
		if (frameCount <= 0) {
			return false;
		}

		if (!ensureBuffers(imageWidth, imageHeight, frameCount)) {
			return false;
		}

		uploadFrames.resize(pixelCount * static_cast<size_t>(frameCount));
		for (int t = 0; t < frameCount; ++t) {
			std::memcpy(
				uploadFrames.data() + static_cast<size_t>(t) * pixelCount,
				historyFrames[static_cast<size_t>(t)].data(),
				pixelCount * sizeof(PIXEL_RGBA));
		}

		D3D11_MAPPED_SUBRESOURCE mappedInput = {};
		if (FAILED(context->Map(inputBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedInput))) {
			return false;
		}
		std::memcpy(mappedInput.pData, uploadFrames.data(), uploadFrames.size() * sizeof(PIXEL_RGBA));
		context->Unmap(inputBuffer, 0);

		D3D11_MAPPED_SUBRESOURCE mappedConstants = {};
		if (FAILED(context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedConstants))) {
			return false;
		}
		ShaderConstants* constants = reinterpret_cast<ShaderConstants*>(mappedConstants.pData);
		constants->width = static_cast<UINT>(imageWidth);
		constants->height = static_cast<UINT>(imageHeight);
		constants->searchRadius = static_cast<UINT>(std::max(1, searchRadius));
		constants->frameCount = static_cast<UINT>(frameCount);
		constants->currentFrameIndex = static_cast<UINT>(frameCount - 1);
		constants->reserved0 = 0.0f;
		const double sigma = std::max(0.001, sigmaValue);
		constants->invSigma2 = static_cast<float>(1.0 / (sigma * sigma));
		constants->reserved1 = 0.0f;
		context->Unmap(constantBuffer, 0);

		ID3D11ShaderResourceView* srvs[1] = { inputSrv };
		ID3D11UnorderedAccessView* uavs[1] = { outputUav };
		ID3D11Buffer* cbs[1] = { constantBuffer };
		context->CSSetShader(computeShader, nullptr, 0);
		context->CSSetShaderResources(0, 1, srvs);
		context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
		context->CSSetConstantBuffers(0, 1, cbs);

		const UINT groupsX = static_cast<UINT>((imageWidth + 15) / 16);
		const UINT groupsY = static_cast<UINT>((imageHeight + 15) / 16);
		context->Dispatch(groupsX, groupsY, 1);

		ID3D11ShaderResourceView* nullSrv[1] = { nullptr };
		ID3D11UnorderedAccessView* nullUav[1] = { nullptr };
		context->CSSetShaderResources(0, 1, nullSrv);
		context->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
		context->CSSetShader(nullptr, nullptr, 0);

		context->CopyResource(readbackBuffer, outputBuffer);

		D3D11_MAPPED_SUBRESOURCE mappedOutput = {};
		if (FAILED(context->Map(readbackBuffer, 0, D3D11_MAP_READ, 0, &mappedOutput))) {
			return false;
		}
		std::memcpy(outputPixels, mappedOutput.pData, pixelCount * sizeof(PIXEL_RGBA));
		context->Unmap(readbackBuffer, 0);
		return true;
	}

private:
	struct ShaderConstants
	{
		UINT width;
		UINT height;
		UINT searchRadius;
		UINT frameCount;
		UINT currentFrameIndex;
		float reserved0;
		float invSigma2;
		float reserved1;
	};

	bool ensurePipeline()
	{
		if (computeShader != nullptr && constantBuffer != nullptr) {
			return true;
		}

		static const char* shaderSource =
			"cbuffer Constants : register(b0)\n"
			"{\n"
			"	uint Width;\n"
			"	uint Height;\n"
			"	uint SearchRadius;\n"
			"	uint FrameCount;\n"
			"	uint CurrentFrameIndex;\n"
			"	float Reserved0;\n"
			"	float InvSigma2;\n"
			"	float Reserved1;\n"
			"};\n"
			"struct PixelData { uint r; uint g; uint b; uint a; };\n"
			"StructuredBuffer<PixelData> InputPixels : register(t0);\n"
			"RWStructuredBuffer<PixelData> OutputPixels : register(u0);\n"
			"int clampi(int v, int low, int high) { return min(high, max(low, v)); }\n"
			"uint frameIndex(uint t, uint x, uint y) { return (t * Height + y) * Width + x; }\n"
			"[numthreads(16,16,1)]\n"
			"void main(uint3 tid : SV_DispatchThreadID)\n"
			"{\n"
			"	if (tid.x >= Width || tid.y >= Height) return;\n"
			"	const int x = (int)tid.x;\n"
			"	const int y = (int)tid.y;\n"
			"	const uint centerIndex = frameIndex(CurrentFrameIndex, tid.x, tid.y);\n"
			"	const PixelData center = InputPixels[centerIndex];\n"
			"	float sumW = 0.0f;\n"
			"	float sumR = 0.0f;\n"
			"	float sumG = 0.0f;\n"
			"	float sumB = 0.0f;\n"
			"	for (uint t = 0; t < FrameCount; ++t){\n"
			"		for (int dy = -((int)SearchRadius); dy <= (int)SearchRadius; ++dy){\n"
			"			const int sy = clampi(y + dy, 0, (int)Height - 1);\n"
			"			for (int dx = -((int)SearchRadius); dx <= (int)SearchRadius; ++dx){\n"
			"				const int sx = clampi(x + dx, 0, (int)Width - 1);\n"
			"				const PixelData sample = InputPixels[frameIndex(t, (uint)sx, (uint)sy)];\n"
			"				const float dr = (float)sample.r - (float)center.r;\n"
			"				const float dg = (float)sample.g - (float)center.g;\n"
			"				const float db = (float)sample.b - (float)center.b;\n"
			"				const float dist2 = dr * dr + dg * dg + db * db;\n"
			"				const float w = exp(-dist2 * InvSigma2);\n"
			"				sumW += w;\n"
			"				sumR += w * (float)sample.r;\n"
			"				sumG += w * (float)sample.g;\n"
			"				sumB += w * (float)sample.b;\n"
			"			}\n"
			"		}\n"
			"	}\n"
			"	PixelData outPixel = center;\n"
			"	if (sumW > 0.0f){\n"
			"		outPixel.r = (uint)clamp(sumR / sumW, 0.0f, 255.0f);\n"
			"		outPixel.g = (uint)clamp(sumG / sumW, 0.0f, 255.0f);\n"
			"		outPixel.b = (uint)clamp(sumB / sumW, 0.0f, 255.0f);\n"
			"	}\n"
			"	OutputPixels[tid.y * Width + tid.x] = outPixel;\n"
			"}\n";

		CComPtr<ID3DBlob> shaderBlob;
		CComPtr<ID3DBlob> errorBlob;
		if (!compile_compute_shader_from_file_or_embedded(
			reinterpret_cast<HMODULE>(&__ImageBase),
			L"nlmeans_exedit2_cs.hlsl",
			shaderSource,
			"Exedit2NlmEmbedded.hlsl",
			&shaderBlob,
			&errorBlob)) {
			return false;
		}

		if (FAILED(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &computeShader))) {
			return false;
		}

		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.ByteWidth = sizeof(ShaderConstants);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &constantBuffer))) {
			return false;
		}
		return true;
	}

	bool ensureBuffers(int imageWidth, int imageHeight, int frameCount)
	{
		if (inputBuffer != nullptr && outputBuffer != nullptr && readbackBuffer != nullptr &&
			imageWidth == width && imageHeight == height && frameCount == bufferedFrameCount) {
			return true;
		}

		inputBuffer = nullptr;
		inputSrv = nullptr;
		outputBuffer = nullptr;
		outputUav = nullptr;
		readbackBuffer = nullptr;
		width = imageWidth;
		height = imageHeight;
		bufferedFrameCount = frameCount;

		const UINT pixelCount = static_cast<UINT>(imageWidth * imageHeight);
		const UINT outputByteSize = pixelCount * sizeof(PIXEL_RGBA);
		const UINT inputByteSize = pixelCount * static_cast<UINT>(frameCount) * sizeof(PIXEL_RGBA);

		D3D11_BUFFER_DESC inputDesc = {};
		inputDesc.ByteWidth = inputByteSize;
		inputDesc.Usage = D3D11_USAGE_DYNAMIC;
		inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		inputDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		inputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		inputDesc.StructureByteStride = sizeof(PIXEL_RGBA);
		if (FAILED(device->CreateBuffer(&inputDesc, nullptr, &inputBuffer))) {
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = pixelCount * static_cast<UINT>(frameCount);
		if (FAILED(device->CreateShaderResourceView(inputBuffer, &srvDesc, &inputSrv))) {
			return false;
		}

		D3D11_BUFFER_DESC outputDesc = {};
		outputDesc.ByteWidth = outputByteSize;
		outputDesc.Usage = D3D11_USAGE_DEFAULT;
		outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		outputDesc.CPUAccessFlags = 0;
		outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		outputDesc.StructureByteStride = sizeof(PIXEL_RGBA);
		if (FAILED(device->CreateBuffer(&outputDesc, nullptr, &outputBuffer))) {
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = pixelCount;
		if (FAILED(device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUav))) {
			return false;
		}

		D3D11_BUFFER_DESC readbackDesc = {};
		readbackDesc.ByteWidth = outputByteSize;
		readbackDesc.Usage = D3D11_USAGE_STAGING;
		readbackDesc.BindFlags = 0;
		readbackDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		readbackDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		readbackDesc.StructureByteStride = sizeof(PIXEL_RGBA);
		if (FAILED(device->CreateBuffer(&readbackDesc, nullptr, &readbackBuffer))) {
			return false;
		}

		return true;
	}

	void clearResources()
	{
		device = nullptr;
		context = nullptr;
		computeShader = nullptr;
		constantBuffer = nullptr;
		inputBuffer = nullptr;
		inputSrv = nullptr;
		outputBuffer = nullptr;
		outputUav = nullptr;
		readbackBuffer = nullptr;
		width = 0;
		height = 0;
		bufferedFrameCount = 0;
		historyFrames.clear();
		uploadFrames.clear();
	}

	int activeAdapterOrdinal;
	int width;
	int height;
	int bufferedFrameCount;
	std::deque<std::vector<PIXEL_RGBA>> historyFrames;
	std::vector<PIXEL_RGBA> uploadFrames;
	CComPtr<ID3D11Device> device;
	CComPtr<ID3D11DeviceContext> context;
	CComPtr<ID3D11ComputeShader> computeShader;
	CComPtr<ID3D11Buffer> constantBuffer;
	CComPtr<ID3D11Buffer> inputBuffer;
	CComPtr<ID3D11ShaderResourceView> inputSrv;
	CComPtr<ID3D11Buffer> outputBuffer;
	CComPtr<ID3D11UnorderedAccessView> outputUav;
	CComPtr<ID3D11Buffer> readbackBuffer;
};

std::unique_ptr<Exedit2GpuRunner> g_gpu_runner;

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

// UI の選択値を実アダプタ番号へ変換する。Auto は -1 を返す。
int get_selected_gpu_adapter_ordinal()
{
	const int selected = item_gpu_adapter.value;
	if (selected <= 0) {
		return -1;
	}

	const size_t hardware_count = g_gpu_adapter_names.size() > 0 ? (g_gpu_adapter_names.size() - 1) : 0;
	const int ordinal = selected - 1;
	if (ordinal < 0 || static_cast<size_t>(ordinal) >= hardware_count) {
		return -1;
	}
	return ordinal;
}

// 選択値が現在の列挙結果で有効かを返す。
bool is_selected_gpu_adapter_available()
{
	if (!has_hardware_gpu_adapter()) {
		return false;
	}
	const int selected = item_gpu_adapter.value;
	if (selected <= 0) {
		return true;
	}
	const size_t hardware_count = g_gpu_adapter_names.size() > 0 ? (g_gpu_adapter_names.size() - 1) : 0;
	return static_cast<size_t>(selected - 1) < hardware_count;
}

// 要求モードと環境能力から、実際に実行するモードを決定する。
ExecutionMode resolve_execution_mode(int requested_mode)
{
	if (requested_mode >= static_cast<int>(ExecutionMode::GpuDx11) &&
		has_hardware_gpu_adapter() &&
		is_selected_gpu_adapter_available()) {
		return ExecutionMode::GpuDx11;
	}
	if (requested_mode >= static_cast<int>(ExecutionMode::CpuAvx2) && is_avx2_available()) {
		return ExecutionMode::CpuAvx2;
	}
	return ExecutionMode::CpuNaive;
}

// ExEdit2 の画像バッファへ CPU Naive な NLM を適用する。
PIXEL_RGBA process_pixel_scalar(
	const std::vector<PIXEL_RGBA>& input,
	int width,
	int height,
	int x,
	int y,
	int search_radius,
	double sigma2)
{
	const int center_index = y * width + x;
	const PIXEL_RGBA& center = input[static_cast<size_t>(center_index)];

	double sum_r = 0.0;
	double sum_g = 0.0;
	double sum_b = 0.0;
	double sum_w = 0.0;

	for (int dy = -search_radius; dy <= search_radius; ++dy) {
		const int sy = std::clamp(y + dy, 0, height - 1);
		for (int dx = -search_radius; dx <= search_radius; ++dx) {
			const int sx = std::clamp(x + dx, 0, width - 1);
			const PIXEL_RGBA& sample = input[static_cast<size_t>(sy * width + sx)];

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
	return out;
}

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
			g_output_pixels[static_cast<size_t>(y * width + x)] =
				process_pixel_scalar(g_input_pixels, width, height, x, y, search_radius, sigma2);
		}
	}

	video->set_image_data(g_output_pixels.data(), width, height);
	return true;
}

// ExEdit2 の画像バッファへ CPU AVX2 な NLM を適用する。
bool apply_nlm_cpu_avx2(FILTER_PROC_VIDEO* video)
{
	if (video == nullptr || video->scene == nullptr || video->get_image_data == nullptr || video->set_image_data == nullptr) {
		return false;
	}

	const int width = video->scene->width;
	const int height = video->scene->height;
	if (width <= 0 || height <= 0) {
		return false;
	}

	const int search_radius = std::max(1, static_cast<int>(item_search_radius.value));
	const float sigma = std::max(0.001f, static_cast<float>(item_sigma.value));
	const float sigma2 = sigma * sigma;
	const int pixel_count = width * height;

	g_input_pixels.resize(static_cast<size_t>(pixel_count));
	g_output_pixels.resize(static_cast<size_t>(pixel_count));
	video->get_image_data(g_input_pixels.data());

	std::vector<float> r(static_cast<size_t>(pixel_count));
	std::vector<float> g(static_cast<size_t>(pixel_count));
	std::vector<float> b(static_cast<size_t>(pixel_count));
	for (int i = 0; i < pixel_count; ++i) {
		const PIXEL_RGBA& p = g_input_pixels[static_cast<size_t>(i)];
		r[static_cast<size_t>(i)] = static_cast<float>(p.r);
		g[static_cast<size_t>(i)] = static_cast<float>(p.g);
		b[static_cast<size_t>(i)] = static_cast<float>(p.b);
	}

	const int x_begin = search_radius;
	const int x_end_avx = width - search_radius - 8;
	const int y_begin = search_radius;
	const int y_end = height - search_radius;

	const __m256 zero = _mm256_set1_ps(0.0f);
	const __m256 max255 = _mm256_set1_ps(255.0f);
	const __m256 inv_sigma2 = _mm256_set1_ps(-1.0f / sigma2);

	// 端は境界クランプが必要なためスカラーで処理する。
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (y >= y_begin && y < y_end && x >= x_begin && x <= x_end_avx) {
				x = x_end_avx;
				continue;
			}
			g_output_pixels[static_cast<size_t>(y * width + x)] =
				process_pixel_scalar(g_input_pixels, width, height, x, y, search_radius, sigma2);
		}
	}

	// 内部領域は 8 ピクセルずつ AVX2 で処理する。
	for (int y = y_begin; y < y_end; ++y) {
		for (int x = x_begin; x <= x_end_avx; x += 8) {
			const int center_base = y * width + x;
			const __m256 center_r = _mm256_loadu_ps(&r[static_cast<size_t>(center_base)]);
			const __m256 center_g = _mm256_loadu_ps(&g[static_cast<size_t>(center_base)]);
			const __m256 center_b = _mm256_loadu_ps(&b[static_cast<size_t>(center_base)]);

			__m256 sum_r = _mm256_setzero_ps();
			__m256 sum_g = _mm256_setzero_ps();
			__m256 sum_b = _mm256_setzero_ps();
			__m256 sum_w = _mm256_setzero_ps();

			for (int dy = -search_radius; dy <= search_radius; ++dy) {
				const int sy = y + dy;
				for (int dx = -search_radius; dx <= search_radius; ++dx) {
					const int sample_base = sy * width + (x + dx);
					const __m256 sample_r = _mm256_loadu_ps(&r[static_cast<size_t>(sample_base)]);
					const __m256 sample_g = _mm256_loadu_ps(&g[static_cast<size_t>(sample_base)]);
					const __m256 sample_b = _mm256_loadu_ps(&b[static_cast<size_t>(sample_base)]);

					const __m256 dr = _mm256_sub_ps(sample_r, center_r);
					const __m256 dg = _mm256_sub_ps(sample_g, center_g);
					const __m256 db = _mm256_sub_ps(sample_b, center_b);
					__m256 dist2 = _mm256_mul_ps(dr, dr);
					dist2 = _mm256_add_ps(dist2, _mm256_mul_ps(dg, dg));
					dist2 = _mm256_add_ps(dist2, _mm256_mul_ps(db, db));

					const __m256 exponent = _mm256_mul_ps(dist2, inv_sigma2);
					alignas(32) std::array<float, 8> exponent_scalar;
					alignas(32) std::array<float, 8> weight_scalar;
					_mm256_store_ps(exponent_scalar.data(), exponent);
					for (int lane = 0; lane < 8; ++lane) {
						weight_scalar[static_cast<size_t>(lane)] = std::exp(exponent_scalar[static_cast<size_t>(lane)]);
					}
					const __m256 weight = _mm256_load_ps(weight_scalar.data());

					sum_r = _mm256_add_ps(sum_r, _mm256_mul_ps(weight, sample_r));
					sum_g = _mm256_add_ps(sum_g, _mm256_mul_ps(weight, sample_g));
					sum_b = _mm256_add_ps(sum_b, _mm256_mul_ps(weight, sample_b));
					sum_w = _mm256_add_ps(sum_w, weight);
				}
			}

			__m256 out_r = _mm256_div_ps(sum_r, sum_w);
			__m256 out_g = _mm256_div_ps(sum_g, sum_w);
			__m256 out_b = _mm256_div_ps(sum_b, sum_w);
			out_r = _mm256_min_ps(max255, _mm256_max_ps(zero, out_r));
			out_g = _mm256_min_ps(max255, _mm256_max_ps(zero, out_g));
			out_b = _mm256_min_ps(max255, _mm256_max_ps(zero, out_b));

			alignas(32) std::array<float, 8> out_r_scalar;
			alignas(32) std::array<float, 8> out_g_scalar;
			alignas(32) std::array<float, 8> out_b_scalar;
			_mm256_store_ps(out_r_scalar.data(), out_r);
			_mm256_store_ps(out_g_scalar.data(), out_g);
			_mm256_store_ps(out_b_scalar.data(), out_b);

			for (int lane = 0; lane < 8; ++lane) {
				const int idx = center_base + lane;
				PIXEL_RGBA p = g_input_pixels[static_cast<size_t>(idx)];
				p.r = static_cast<unsigned char>(out_r_scalar[static_cast<size_t>(lane)]);
				p.g = static_cast<unsigned char>(out_g_scalar[static_cast<size_t>(lane)]);
				p.b = static_cast<unsigned char>(out_b_scalar[static_cast<size_t>(lane)]);
				g_output_pixels[static_cast<size_t>(idx)] = p;
			}
		}
	}

	video->set_image_data(g_output_pixels.data(), width, height);
	return true;
}

// TODO: ExEdit2 向けに GPU 実処理を接続する。
bool apply_nlm_gpu_dx11(FILTER_PROC_VIDEO* video, int adapterOrdinal)
{
	if (video == nullptr || video->scene == nullptr || video->get_image_data == nullptr || video->set_image_data == nullptr) {
		return false;
	}
	if (g_gpu_runner == nullptr) {
		g_gpu_runner = std::unique_ptr<Exedit2GpuRunner>(new Exedit2GpuRunner());
	}
	if (!g_gpu_runner->initialize(adapterOrdinal)) {
		if (is_avx2_available()) {
			return apply_nlm_cpu_avx2(video);
		}
		return apply_nlm_cpu_naive(video);
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
	const int time_radius = std::max(0, static_cast<int>(item_time_radius.value));
	const double sigma = std::max(0.001, static_cast<double>(item_sigma.value));
	if (!g_gpu_runner->process(
		g_input_pixels.data(),
		g_output_pixels.data(),
		width,
		height,
		search_radius,
		time_radius,
		sigma)) {
		if (is_avx2_available()) {
			return apply_nlm_cpu_avx2(video);
		}
		return apply_nlm_cpu_naive(video);
	}
	video->set_image_data(g_output_pixels.data(), width, height);
	return true;
}

bool func_proc_video(FILTER_PROC_VIDEO* video)
{
	const ExecutionMode mode = resolve_execution_mode(item_mode.value);
	g_runtime_gpu_adapter_ordinal = get_selected_gpu_adapter_ordinal();
	switch (mode) {
	case ExecutionMode::CpuNaive:
		return apply_nlm_cpu_naive(video);
	case ExecutionMode::CpuAvx2:
		return apply_nlm_cpu_avx2(video);
	case ExecutionMode::GpuDx11:
		return apply_nlm_gpu_dx11(video, g_runtime_gpu_adapter_ordinal);
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
	g_gpu_runner = std::unique_ptr<Exedit2GpuRunner>(new Exedit2GpuRunner());
	rebuild_gpu_adapter_list();
	return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin()
{
	g_gpu_runner.reset();
}
#endif
