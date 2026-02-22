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

#ifndef EXEDIT2_GPU_RUNNER_H
#define EXEDIT2_GPU_RUNNER_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <deque>
#include <vector>
#include <d3d11.h>
#include <atlbase.h>
#include "../aviutl2_sdk/filter2.h"

// ExEdit2 向け GPU 処理（RGBA 入出力）を担当する実行クラス。
class Exedit2GpuRunner
{
public:
	Exedit2GpuRunner();

	// 指定したアダプタ番号で初期化する。-1 は自動選択。
	bool initialize(int adapterOrdinal);

	// RGBA フレーム列に空間/時間 NLM を適用する。
	bool process(
		const PIXEL_RGBA* inputPixels,
		PIXEL_RGBA* outputPixels,
		int imageWidth,
		int imageHeight,
		int searchRadius,
		int timeRadius,
		double sigmaValue,
		int spatialStep = 1,
		double temporalDecay = 0.0);

private:
	struct ShaderConstants
	{
		UINT width;
		UINT height;
		UINT searchRadius;
		UINT frameCount;
		UINT currentFrameIndex;
		UINT spatialStep;
		float invSigma2;
		float temporalDecay;
		float reserved0;
		float reserved1;
		float reserved2;
		float reserved3;
	};

	bool ensurePipeline();
	bool ensureBuffers(int imageWidth, int imageHeight, int frameCount);
	void clearResources();

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

#endif
