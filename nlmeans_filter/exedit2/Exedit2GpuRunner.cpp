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

#include "Exedit2GpuRunner.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "../D3d11BufferUtil.h"
#include "../D3d11ComputeUtil.h"
#include "../D3d11DeviceUtil.h"
#include "../ShaderCompileUtil.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

Exedit2GpuRunner::Exedit2GpuRunner()
	: activeAdapterOrdinal(-2), width(0), height(0), bufferedFrameCount(0)
{
}

bool Exedit2GpuRunner::initialize(int adapterOrdinal)
{
	if (device != nullptr && context != nullptr && activeAdapterOrdinal == adapterOrdinal) {
		return true;
	}

	clearResources();

	if (!create_d3d11_device_with_preferred_adapter(
		adapterOrdinal,
		&device,
		&context,
		&activeAdapterOrdinal,
		nullptr)) {
		clearResources();
		return false;
	}

	if (!ensurePipeline()) {
		clearResources();
		return false;
	}

	return true;
}

bool Exedit2GpuRunner::process(
	const PIXEL_RGBA* inputPixels,
	PIXEL_RGBA* outputPixels,
	int imageWidth,
	int imageHeight,
	int searchRadius,
	int timeRadius,
	double sigmaValue,
	int spatialStep,
	double temporalDecay,
	int processYBegin,
	int processYEnd)
{
	if (device == nullptr || context == nullptr || inputPixels == nullptr || outputPixels == nullptr) {
		return false;
	}
	if (imageWidth <= 0 || imageHeight <= 0) {
		return false;
	}
	const int clampedTimeRadius = std::max(0, timeRadius);
	const int requiredHistory = clampedTimeRadius + 1;
	const int clampedYBegin = std::clamp(processYBegin, 0, imageHeight);
	const int normalizedYEnd = (processYEnd < 0) ? imageHeight : processYEnd;
	const int clampedYEnd = std::clamp(normalizedYEnd, clampedYBegin, imageHeight);

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

	if (FAILED(map_write_discard_buffer(
		context,
		inputBuffer,
		uploadFrames.data(),
		uploadFrames.size() * sizeof(PIXEL_RGBA)))) {
		return false;
	}

	ShaderConstants constants = {};
	constants.width = static_cast<UINT>(imageWidth);
	constants.height = static_cast<UINT>(imageHeight);
	constants.searchRadius = static_cast<UINT>(std::max(1, searchRadius));
	constants.frameCount = static_cast<UINT>(frameCount);
	constants.currentFrameIndex = static_cast<UINT>(frameCount - 1);
	constants.processYBegin = static_cast<UINT>(clampedYBegin);
	constants.processYEnd = static_cast<UINT>(clampedYEnd);
	constants.reservedU0 = 0;
	constants.reservedU1 = 0;
	const double sigma = std::max(0.001, sigmaValue);
	const int clampedSpatialStep = std::max(1, spatialStep);
	const float clampedTemporalDecay = static_cast<float>(std::max(0.0, temporalDecay));
	const float currentFrameIndexF = static_cast<float>(std::max(0, frameCount - 1));
	float temporalWeightAtT0 = 1.0f;
	float temporalWeightStep = 1.0f;
	if (clampedTemporalDecay > 0.0f) {
		temporalWeightStep = std::exp(clampedTemporalDecay);
		temporalWeightAtT0 = std::exp(-clampedTemporalDecay * currentFrameIndexF);
	}
	constants.invSigma = static_cast<float>(1.0 / sigma);
	constants.temporalWeightAtT0 = temporalWeightAtT0;
	constants.temporalWeightStep = temporalWeightStep;
	constants.spatialStep = static_cast<UINT>(clampedSpatialStep);
	constants.reservedF0 = 0.0f;
	constants.reservedF1 = 0.0f;
	constants.reservedF2 = 0.0f;
	if (FAILED(map_write_discard_buffer(context, constantBuffer, &constants, sizeof(constants)))) {
		return false;
	}

	dispatch_compute_pass(
		context,
		computeShader,
		inputSrv,
		outputUav,
		constantBuffer,
		static_cast<UINT>(imageWidth),
		static_cast<UINT>(std::max(0, clampedYEnd - clampedYBegin)));

	context->CopyResource(readbackBuffer, outputBuffer);
	if (FAILED(map_read_buffer(context, readbackBuffer, outputPixels, pixelCount * sizeof(PIXEL_RGBA)))) {
		return false;
	}
	return true;
}

bool Exedit2GpuRunner::ensurePipeline()
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
		"	uint SpatialStep;\n"
		"	uint ProcessYBegin;\n"
		"	uint ProcessYEnd;\n"
		"	uint ReservedU0;\n"
		"	uint ReservedU1;\n"
		"	float InvSigma;\n"
		"	float TemporalWeightAtT0;\n"
		"	float TemporalWeightStep;\n"
		"	float ReservedF0;\n"
		"	float ReservedF1;\n"
		"	float ReservedF2;\n"
		"};\n"
		"StructuredBuffer<uint> InputPixels : register(t0);\n"
		"RWStructuredBuffer<uint> OutputPixels : register(u0);\n"
		"static const int2 PatchOffsets[9] = {\n"
		"	int2(-1, -1), int2(0, -1), int2(1, -1),\n"
		"	int2(-1,  0), int2(0,  0), int2(1,  0),\n"
		"	int2(-1,  1), int2(0,  1), int2(1,  1)\n"
		"};\n"
		"static const float PatchWeights[9] = {\n"
		"	0.07f, 0.12f, 0.07f,\n"
		"	0.12f, 0.20f, 0.12f,\n"
		"	0.07f, 0.12f, 0.07f\n"
		"};\n"
		"int clampi(int v, int low, int high) { return min(high, max(low, v)); }\n"
		"uint frameIndex(uint t, uint x, uint y) { return (t * Height + y) * Width + x; }\n"
		"float3 unpack_rgb(uint packed) {\n"
		"	return float3((float)(packed & 0xffu), (float)((packed >> 8) & 0xffu), (float)((packed >> 16) & 0xffu));\n"
		"}\n"
		"uint unpack_a(uint packed) { return (packed >> 24) & 0xffu; }\n"
		"uint pack_rgba(float3 rgb, uint a) {\n"
		"	const uint r = (uint)clamp(rgb.x, 0.0f, 255.0f);\n"
		"	const uint g = (uint)clamp(rgb.y, 0.0f, 255.0f);\n"
		"	const uint b = (uint)clamp(rgb.z, 0.0f, 255.0f);\n"
		"	return (a << 24) | (b << 16) | (g << 8) | r;\n"
		"}\n"
		"[numthreads(16,16,1)]\n"
		"void main(uint3 tid : SV_DispatchThreadID)\n"
		"{\n"
		"	const uint yGlobal = tid.y + ProcessYBegin;\n"
		"	if (tid.x >= Width || yGlobal >= Height || yGlobal >= ProcessYEnd) return;\n"
		"	const int x = (int)tid.x;\n"
		"	const int y = (int)yGlobal;\n"
		"	const uint pixelsPerFrame = Width * Height;\n"
		"	const uint currentFrameBase = CurrentFrameIndex * pixelsPerFrame;\n"
		"	const uint centerIndex = currentFrameBase + yGlobal * Width + tid.x;\n"
		"	const uint centerPacked = InputPixels[centerIndex];\n"
		"	const float3 center = unpack_rgb(centerPacked);\n"
		"	const uint alpha = unpack_a(centerPacked);\n"
		"	float3 currentPatch[9];\n"
		"	[unroll]\n"
		"	for (int i = 0; i < 9; ++i){\n"
		"		const int cx = clampi(x + PatchOffsets[i].x, 0, (int)Width - 1);\n"
		"		const int cy = clampi(y + PatchOffsets[i].y, 0, (int)Height - 1);\n"
		"		currentPatch[i] = unpack_rgb(InputPixels[currentFrameBase + (uint)cy * Width + (uint)cx]);\n"
		"	}\n"
		"	float sumW = 0.0f;\n"
		"	float sumR = 0.0f;\n"
		"	float sumG = 0.0f;\n"
		"	float sumB = 0.0f;\n"
		"	float temporalWeight = TemporalWeightAtT0;\n"
		"	for (uint t = 0; t < FrameCount; ++t){\n"
		"		const uint frameBase = t * pixelsPerFrame;\n"
		"		for (int dy = -((int)SearchRadius); dy <= (int)SearchRadius; dy += (int)SpatialStep){\n"
		"			const int sy = clampi(y + dy, 0, (int)Height - 1);\n"
		"			for (int dx = -((int)SearchRadius); dx <= (int)SearchRadius; dx += (int)SpatialStep){\n"
		"				const int sx = clampi(x + dx, 0, (int)Width - 1);\n"
		"				float patchDistance = 0.0f;\n"
		"				// パッチ中心は候補画素そのものなので、先に読み出して加算色へ使う。\n"
		"				const float3 sample = unpack_rgb(InputPixels[frameBase + (uint)sy * Width + (uint)sx]);\n"
		"				[unroll]\n"
		"				for (int i = 0; i < 9; ++i){\n"
		"					const int tx = clampi(sx + PatchOffsets[i].x, 0, (int)Width - 1);\n"
		"					const int ty = clampi(sy + PatchOffsets[i].y, 0, (int)Height - 1);\n"
		"					const float3 samplePatch = unpack_rgb(InputPixels[frameBase + (uint)ty * Width + (uint)tx]);\n"
		"					const float3 diff = currentPatch[i] - samplePatch;\n"
		"					patchDistance += dot(diff, diff) * PatchWeights[i];\n"
		"				}\n"
		"				const float w = exp(-sqrt(max(patchDistance, 0.0f)) * InvSigma) * temporalWeight;\n"
		"				sumW += w;\n"
		"				sumR += w * sample.r;\n"
		"				sumG += w * sample.g;\n"
		"				sumB += w * sample.b;\n"
		"			}\n"
		"		}\n"
		"		temporalWeight *= TemporalWeightStep;\n"
		"	}\n"
		"	float3 outRgb = center;\n"
		"	if (sumW > 0.0f){\n"
		"		outRgb = float3(sumR / sumW, sumG / sumW, sumB / sumW);\n"
		"	}\n"
		"	OutputPixels[yGlobal * Width + tid.x] = pack_rgba(outRgb, alpha);\n"
		"}\n";

	CComPtr<ID3DBlob> shaderBlob;
	CComPtr<ID3DBlob> errorBlob;
	if (!compile_compute_shader_from_cso_or_file_or_embedded(
		reinterpret_cast<HMODULE>(&::__ImageBase),
		L"nlmeans_exedit2_cs.cso",
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

bool Exedit2GpuRunner::ensureBuffers(int imageWidth, int imageHeight, int frameCount)
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

	if (FAILED(create_structured_buffer(
		device,
		inputByteSize,
		sizeof(PIXEL_RGBA),
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		&inputBuffer))) {
		return false;
	}

	if (FAILED(create_structured_srv(device, inputBuffer, pixelCount * static_cast<UINT>(frameCount), &inputSrv))) {
		return false;
	}

	if (FAILED(create_structured_buffer(
		device,
		outputByteSize,
		sizeof(PIXEL_RGBA),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_UNORDERED_ACCESS,
		0,
		&outputBuffer))) {
		return false;
	}

	if (FAILED(create_structured_uav(device, outputBuffer, pixelCount, &outputUav))) {
		return false;
	}

	if (FAILED(create_structured_buffer(
		device,
		outputByteSize,
		sizeof(PIXEL_RGBA),
		D3D11_USAGE_STAGING,
		0,
		D3D11_CPU_ACCESS_READ,
		&readbackBuffer))) {
		return false;
	}

	return true;
}

void Exedit2GpuRunner::clearResources()
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
