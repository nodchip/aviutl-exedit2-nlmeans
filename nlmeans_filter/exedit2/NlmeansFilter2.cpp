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
#include <dxgi1_6.h>
#include <atlbase.h>

#pragma comment(lib, "dxgi.lib")

#if __has_include("../aviutl2_sdk/filter2.h")
#include "Exedit2GpuRunner.h"
#include "FastModeConfig.h"
#include "GpuCoopPolicy.h"
#include "GpuCoopExecution.h"
#include "GpuCoopRecoveryPolicy.h"
#include "GpuRunnerDispatch.h"
#include "MultiGpuCompose.h"
#include "MultiGpuTiling.h"
#include "UiToDispatcherIntegration.h"
#include "../DxgiAdapterUtil.h"

namespace {

std::vector<std::wstring> g_gpu_adapter_names;
std::vector<FILTER_ITEM_SELECT::ITEM> g_gpu_adapter_items;
std::vector<PIXEL_RGBA> g_input_pixels;
std::vector<PIXEL_RGBA> g_output_pixels;
std::deque<std::vector<PIXEL_RGBA>> g_cpu_temporal_history;
int g_runtime_gpu_adapter_ordinal = -1;
extern FILTER_ITEM_TRACK item_search_radius;
extern FILTER_ITEM_TRACK item_time_radius;
extern FILTER_ITEM_TRACK item_sigma;
extern FILTER_ITEM_TRACK item_fast_spatial_step;
extern FILTER_ITEM_TRACK item_temporal_decay;
extern FILTER_ITEM_TRACK item_gpu_spatial_step;
extern FILTER_ITEM_TRACK item_gpu_temporal_decay;
extern FILTER_ITEM_TRACK item_gpu_coop_count;
extern FILTER_ITEM_SELECT item_mode;
extern FILTER_ITEM_SELECT item_gpu_adapter;

std::unique_ptr<Exedit2GpuRunner> g_gpu_runner;
std::vector<std::unique_ptr<Exedit2GpuRunner>> g_gpu_coop_runners;

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

// nlm.psh と同じ 3x3 ガウシアンパッチ係数。
constexpr std::array<float, 9> kPatchGaussianWeights = {
	0.07f, 0.12f, 0.07f,
	0.12f, 0.20f, 0.12f,
	0.07f, 0.12f, 0.07f
};

constexpr std::array<int, 9> kPatchOffsetX = { -1, 0, 1, -1, 0, 1, -1, 0, 1 };
constexpr std::array<int, 9> kPatchOffsetY = { -1, -1, -1, 0, 0, 0, 1, 1, 1 };

alignas(32) constexpr std::array<float, 8> kPatchGaussianWeights8 = {
	0.07f, 0.12f, 0.07f,
	0.12f, 0.20f, 0.12f,
	0.07f, 0.12f
};

// 3x3 パッチ距離から nlm.psh 相当の空間重みを計算する。
inline double nlm_spatial_weight_from_patch_distance(double patchDistance, double sigma)
{
	const double clampedDistance = std::max(0.0, patchDistance);
	const double invSigma = 1.0 / std::max(0.001, sigma);
	return std::exp(-std::sqrt(clampedDistance) * invSigma);
}

// 現在フレームパッチと対象フレームパッチの RGB 距離を 3x3 ガウシアンで集約する（Scalar）。
double compute_patch_distance_rgb_scalar(
	const std::vector<PIXEL_RGBA>& currentFrame,
	const std::vector<PIXEL_RGBA>& targetFrame,
	int width,
	int height,
	int x,
	int y,
	int sx,
	int sy)
{
	double patchDistance = 0.0;
	for (int i = 0; i < 9; ++i) {
		const int cx = std::clamp(x + kPatchOffsetX[static_cast<size_t>(i)], 0, width - 1);
		const int cy = std::clamp(y + kPatchOffsetY[static_cast<size_t>(i)], 0, height - 1);
		const int tx = std::clamp(sx + kPatchOffsetX[static_cast<size_t>(i)], 0, width - 1);
		const int ty = std::clamp(sy + kPatchOffsetY[static_cast<size_t>(i)], 0, height - 1);

		const PIXEL_RGBA& current = currentFrame[static_cast<size_t>(cy * width + cx)];
		const PIXEL_RGBA& target = targetFrame[static_cast<size_t>(ty * width + tx)];
		const double dr = static_cast<double>(current.r) - static_cast<double>(target.r);
		const double dg = static_cast<double>(current.g) - static_cast<double>(target.g);
		const double db = static_cast<double>(current.b) - static_cast<double>(target.b);
		const double rgbDistance2 = dr * dr + dg * dg + db * db;
		patchDistance += rgbDistance2 * static_cast<double>(kPatchGaussianWeights[static_cast<size_t>(i)]);
	}
	return patchDistance;
}

// 現在フレームパッチと対象フレームパッチの RGB 距離を 3x3 ガウシアンで集約する（AVX2）。
double compute_patch_distance_rgb_avx2(
	const std::vector<PIXEL_RGBA>& currentFrame,
	const std::vector<PIXEL_RGBA>& targetFrame,
	int width,
	int height,
	int x,
	int y,
	int sx,
	int sy)
{
	alignas(32) std::array<float, 8> dr{};
	alignas(32) std::array<float, 8> dg{};
	alignas(32) std::array<float, 8> db{};

	for (int i = 0; i < 8; ++i) {
		const int cx = std::clamp(x + kPatchOffsetX[static_cast<size_t>(i)], 0, width - 1);
		const int cy = std::clamp(y + kPatchOffsetY[static_cast<size_t>(i)], 0, height - 1);
		const int tx = std::clamp(sx + kPatchOffsetX[static_cast<size_t>(i)], 0, width - 1);
		const int ty = std::clamp(sy + kPatchOffsetY[static_cast<size_t>(i)], 0, height - 1);

		const PIXEL_RGBA& current = currentFrame[static_cast<size_t>(cy * width + cx)];
		const PIXEL_RGBA& target = targetFrame[static_cast<size_t>(ty * width + tx)];
		dr[static_cast<size_t>(i)] = static_cast<float>(static_cast<int>(current.r) - static_cast<int>(target.r));
		dg[static_cast<size_t>(i)] = static_cast<float>(static_cast<int>(current.g) - static_cast<int>(target.g));
		db[static_cast<size_t>(i)] = static_cast<float>(static_cast<int>(current.b) - static_cast<int>(target.b));
	}

	const __m256 vdr = _mm256_load_ps(dr.data());
	const __m256 vdg = _mm256_load_ps(dg.data());
	const __m256 vdb = _mm256_load_ps(db.data());
	__m256 vdist2 = _mm256_mul_ps(vdr, vdr);
	vdist2 = _mm256_add_ps(vdist2, _mm256_mul_ps(vdg, vdg));
	vdist2 = _mm256_add_ps(vdist2, _mm256_mul_ps(vdb, vdb));
	const __m256 vweights = _mm256_load_ps(kPatchGaussianWeights8.data());
	vdist2 = _mm256_mul_ps(vdist2, vweights);

	alignas(32) std::array<float, 8> weighted{};
	_mm256_store_ps(weighted.data(), vdist2);
	double patchDistance = 0.0;
	for (int i = 0; i < 8; ++i) {
		patchDistance += static_cast<double>(weighted[static_cast<size_t>(i)]);
	}

	const int cx9 = std::clamp(x + kPatchOffsetX[8], 0, width - 1);
	const int cy9 = std::clamp(y + kPatchOffsetY[8], 0, height - 1);
	const int tx9 = std::clamp(sx + kPatchOffsetX[8], 0, width - 1);
	const int ty9 = std::clamp(sy + kPatchOffsetY[8], 0, height - 1);
	const PIXEL_RGBA& current9 = currentFrame[static_cast<size_t>(cy9 * width + cx9)];
	const PIXEL_RGBA& target9 = targetFrame[static_cast<size_t>(ty9 * width + tx9)];
	const double dr9 = static_cast<double>(current9.r) - static_cast<double>(target9.r);
	const double dg9 = static_cast<double>(current9.g) - static_cast<double>(target9.g);
	const double db9 = static_cast<double>(current9.b) - static_cast<double>(target9.b);
	patchDistance += (dr9 * dr9 + dg9 * dg9 + db9 * db9) * static_cast<double>(kPatchGaussianWeights[8]);

	return patchDistance;
}

// ExEdit2 の画像バッファへ CPU Naive な NLM を適用する。
PIXEL_RGBA process_pixel_scalar(
	const std::vector<PIXEL_RGBA>& input,
	int width,
	int height,
	int x,
	int y,
	int search_radius,
	double sigma)
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
			const double patchDistance = compute_patch_distance_rgb_scalar(input, input, width, height, x, y, sx, sy);
			const double w = nlm_spatial_weight_from_patch_distance(patchDistance, sigma);

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

// 空間窓を間引く Fast NLM（近似）で 1 画素を処理する。
PIXEL_RGBA process_pixel_fast(
	const std::vector<PIXEL_RGBA>& input,
	int width,
	int height,
	int x,
	int y,
	int search_radius,
	double sigma,
	int spatial_step)
{
	const int step = std::max(1, spatial_step);
	const int center_index = y * width + x;
	const PIXEL_RGBA& center = input[static_cast<size_t>(center_index)];

	double sum_r = 0.0;
	double sum_g = 0.0;
	double sum_b = 0.0;
	double sum_w = 0.0;

	for (int dy = -search_radius; dy <= search_radius; dy += step) {
		const int sy = std::clamp(y + dy, 0, height - 1);
		for (int dx = -search_radius; dx <= search_radius; dx += step) {
			const int sx = std::clamp(x + dx, 0, width - 1);
			const PIXEL_RGBA& sample = input[static_cast<size_t>(sy * width + sx)];
			const double patchDistance = compute_patch_distance_rgb_scalar(input, input, width, height, x, y, sx, sy);
			const double w = nlm_spatial_weight_from_patch_distance(patchDistance, sigma);

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

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			g_output_pixels[static_cast<size_t>(y * width + x)] =
				process_pixel_scalar(g_input_pixels, width, height, x, y, search_radius, sigma);
		}
	}

	video->set_image_data(g_output_pixels.data(), width, height);
	return true;
}

// ExEdit2 の画像バッファへ CPU Fast な NLM 近似を適用する。
bool apply_nlm_cpu_fast(FILTER_PROC_VIDEO* video)
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
	const int spatial_step = resolve_fast_spatial_step(static_cast<int>(item_fast_spatial_step.value));

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			g_output_pixels[static_cast<size_t>(y * width + x)] =
				process_pixel_fast(g_input_pixels, width, height, x, y, search_radius, sigma, spatial_step);
		}
	}

	video->set_image_data(g_output_pixels.data(), width, height);
	return true;
}

// 履歴フレームを使う Temporal NLM（Naive）で 1 画素を処理する。
PIXEL_RGBA process_pixel_temporal(
	const std::deque<std::vector<PIXEL_RGBA>>& historyFrames,
	int width,
	int height,
	int x,
	int y,
	int search_radius,
	double sigma,
	double temporal_decay)
{
	const PIXEL_RGBA& center = historyFrames.back()[static_cast<size_t>(y * width + x)];
	const std::vector<PIXEL_RGBA>& currentFrame = historyFrames.back();
	double sum_r = 0.0;
	double sum_g = 0.0;
	double sum_b = 0.0;
	double sum_w = 0.0;

	for (size_t t = 0; t < historyFrames.size(); ++t) {
		const size_t reverseIndex = historyFrames.size() - 1 - t;
		const std::vector<PIXEL_RGBA>& frame = historyFrames[reverseIndex];
		const double temporalWeight = std::exp(-temporal_decay * static_cast<double>(t));
		for (int dy = -search_radius; dy <= search_radius; ++dy) {
			const int sy = std::clamp(y + dy, 0, height - 1);
			for (int dx = -search_radius; dx <= search_radius; ++dx) {
				const int sx = std::clamp(x + dx, 0, width - 1);
				const PIXEL_RGBA& sample = frame[static_cast<size_t>(sy * width + sx)];
				const double patchDistance = compute_patch_distance_rgb_scalar(
					currentFrame,
					frame,
					width,
					height,
					x,
					y,
					sx,
					sy);
				const double w = nlm_spatial_weight_from_patch_distance(patchDistance, sigma) * temporalWeight;

				sum_r += w * static_cast<double>(sample.r);
				sum_g += w * static_cast<double>(sample.g);
				sum_b += w * static_cast<double>(sample.b);
				sum_w += w;
			}
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

// ExEdit2 の画像バッファへ CPU Temporal NLM を適用する。
bool apply_nlm_cpu_temporal(FILTER_PROC_VIDEO* video)
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
	const int time_radius = std::max(0, static_cast<int>(item_time_radius.value));
	const int required_history = time_radius + 1;
	const double sigma = std::max(0.001, static_cast<double>(item_sigma.value));
	const double temporal_decay = std::max(0.0, static_cast<double>(item_temporal_decay.value));

	if (g_cpu_temporal_history.empty() ||
		g_cpu_temporal_history.back().size() != static_cast<size_t>(pixel_count)) {
		g_cpu_temporal_history.clear();
	}
	g_cpu_temporal_history.push_back(g_input_pixels);
	while (g_cpu_temporal_history.size() > static_cast<size_t>(required_history)) {
		g_cpu_temporal_history.pop_front();
	}

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			g_output_pixels[static_cast<size_t>(y * width + x)] = process_pixel_temporal(
				g_cpu_temporal_history,
				width,
				height,
				x,
				y,
				search_radius,
				sigma,
				temporal_decay);
		}
	}

	video->set_image_data(g_output_pixels.data(), width, height);
	return true;
}

// ExEdit2 の画像バッファへ CPU AVX2 な NLM を適用する。
PIXEL_RGBA process_pixel_avx2(
	const std::vector<PIXEL_RGBA>& input,
	int width,
	int height,
	int x,
	int y,
	int search_radius,
	double sigma)
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
			const double patchDistance = compute_patch_distance_rgb_avx2(input, input, width, height, x, y, sx, sy);
			const double w = nlm_spatial_weight_from_patch_distance(patchDistance, sigma);

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
	const double sigma = std::max(0.001, static_cast<double>(item_sigma.value));
	const int pixel_count = width * height;

	g_input_pixels.resize(static_cast<size_t>(pixel_count));
	g_output_pixels.resize(static_cast<size_t>(pixel_count));
	video->get_image_data(g_input_pixels.data());

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			g_output_pixels[static_cast<size_t>(y * width + x)] =
				process_pixel_avx2(g_input_pixels, width, height, x, y, search_radius, sigma);
		}
	}

	video->set_image_data(g_output_pixels.data(), width, height);
	return true;
}

// CPU モードを切り替えて NLM を適用する。
bool apply_nlm_cpu_by_mode(FILTER_PROC_VIDEO* video, ExecutionMode mode)
{
	if (mode == ExecutionMode::CpuTemporal) {
		return apply_nlm_cpu_temporal(video);
	}
	if (mode == ExecutionMode::CpuFast) {
		return apply_nlm_cpu_fast(video);
	}
	if (mode == ExecutionMode::CpuAvx2) {
		return apply_nlm_cpu_avx2(video);
	}
	return apply_nlm_cpu_naive(video);
}

// ExEdit2 向け GPU 実処理（DirectX 11 Compute Shader）を適用する。
bool apply_nlm_gpu_dx11(FILTER_PROC_VIDEO* video, int adapterOrdinal, ExecutionMode fallbackMode)
{
	if (video == nullptr || video->scene == nullptr || video->get_image_data == nullptr || video->set_image_data == nullptr) {
		return false;
	}
	if (g_gpu_runner == nullptr) {
		g_gpu_runner = std::unique_ptr<Exedit2GpuRunner>(new Exedit2GpuRunner());
	}
	const GpuRunnerDispatchOps ops = {
		[gpuRunner = g_gpu_runner.get()](int requestedAdapterOrdinal) {
			return gpuRunner != nullptr && gpuRunner->initialize(requestedAdapterOrdinal);
		},
		[video, gpuRunner = g_gpu_runner.get(), adapterOrdinal]() {
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
			const int gpu_spatial_step = std::max(1, static_cast<int>(item_gpu_spatial_step.value));
			const double gpu_temporal_decay = std::max(0.0, static_cast<double>(item_gpu_temporal_decay.value));
			// 協調失敗時の再試行経路として単一 GPU 実行を共通化する。
			const auto run_single_gpu = [&]() -> bool {
				if (gpuRunner == nullptr || !gpuRunner->initialize(adapterOrdinal)) {
					return false;
				}
				return gpuRunner->process(
					g_input_pixels.data(),
					g_output_pixels.data(),
					width,
					height,
					search_radius,
					time_radius,
					sigma,
					gpu_spatial_step,
					gpu_temporal_decay);
			};
			const size_t hardware_count = g_gpu_adapter_names.size() > 0 ? (g_gpu_adapter_names.size() - 1) : 0;
			const int requested_coop = static_cast<int>(item_gpu_coop_count.value);
			const bool enable_multi_gpu = should_enable_multi_gpu(adapterOrdinal, hardware_count, requested_coop);
			if (!enable_multi_gpu) {
				if (!run_single_gpu()) {
					return false;
				}
				video->set_image_data(g_output_pixels.data(), width, height);
				return true;
			}

			const size_t active_gpu_count = resolve_active_gpu_count(hardware_count, requested_coop);
			const std::vector<GpuRowTile> tiles = plan_gpu_row_tiles(height, active_gpu_count, 8);
			if (tiles.empty()) {
				return false;
			}
			if (g_gpu_coop_runners.size() < active_gpu_count) {
				g_gpu_coop_runners.resize(active_gpu_count);
			}
			std::vector<std::vector<PIXEL_RGBA>> temp_outputs;
			temp_outputs.resize(active_gpu_count);
			for (size_t i = 0; i < active_gpu_count; ++i) {
				if (g_gpu_coop_runners[i] == nullptr) {
					g_gpu_coop_runners[i] = std::unique_ptr<Exedit2GpuRunner>(new Exedit2GpuRunner());
				}
				temp_outputs[i].resize(static_cast<size_t>(pixel_count));
			}

			bool used_single_gpu_result = false;
			const bool coop_ok = execute_gpu_coop_with_recovery(
				tiles,
				enable_multi_gpu,
				2,
				[&](size_t, const GpuRowTile& tile) {
					Exedit2GpuRunner* runner = g_gpu_coop_runners[tile.adapterIndex].get();
					if (runner == nullptr) {
						return false;
					}
					if (!runner->initialize(static_cast<int>(tile.adapterIndex))) {
						return false;
					}
					return runner->process(
						g_input_pixels.data(),
						temp_outputs[tile.adapterIndex].data(),
						width,
						height,
						search_radius,
						time_radius,
						sigma,
						gpu_spatial_step,
						gpu_temporal_decay,
						tile.yBegin,
						tile.yEnd);
				},
				[&](size_t, const GpuRowTile& tile) {
					if (!run_single_gpu()) {
						return false;
					}
					for (int y = tile.yBegin; y < tile.yEnd; ++y) {
						const size_t rowBegin = static_cast<size_t>(y * width);
						std::memcpy(
							temp_outputs[tile.adapterIndex].data() + rowBegin,
							g_output_pixels.data() + rowBegin,
							static_cast<size_t>(width) * sizeof(PIXEL_RGBA));
					}
					return true;
				},
				[&]() {
					return run_single_gpu();
				},
				[&]() {
					return compose_row_tiled_output(width, height, tiles, temp_outputs, &g_output_pixels);
				},
				&used_single_gpu_result,
				true);
			if (!coop_ok) {
				return false;
			}

			video->set_image_data(g_output_pixels.data(), width, height);
			return true;
		},
		[video](ExecutionMode mode) {
			return apply_nlm_cpu_by_mode(video, mode);
		}
	};
	return dispatch_gpu_runner(ops, adapterOrdinal, fallbackMode);
}

bool dispatch_cpu_naive(void* context)
{
	return apply_nlm_cpu_naive(static_cast<FILTER_PROC_VIDEO*>(context));
}

bool dispatch_cpu_avx2(void* context)
{
	return apply_nlm_cpu_avx2(static_cast<FILTER_PROC_VIDEO*>(context));
}

bool dispatch_cpu_fast(void* context)
{
	return apply_nlm_cpu_fast(static_cast<FILTER_PROC_VIDEO*>(context));
}

bool dispatch_cpu_temporal(void* context)
{
	return apply_nlm_cpu_temporal(static_cast<FILTER_PROC_VIDEO*>(context));
}

bool dispatch_gpu_dx11(void* context, int adapterOrdinal, ExecutionMode fallbackMode)
{
	return apply_nlm_gpu_dx11(static_cast<FILTER_PROC_VIDEO*>(context), adapterOrdinal, fallbackMode);
}

bool func_proc_video(FILTER_PROC_VIDEO* video)
{
	const size_t hardware_count = g_gpu_adapter_names.size() > 0 ? (g_gpu_adapter_names.size() - 1) : 0;
	const UiSelectionSnapshot ui = {
		item_mode.value,
		item_gpu_adapter.value
	};
	const VideoProcessingHandlers handlers = {
		video,
		dispatch_cpu_naive,
		dispatch_cpu_avx2,
		dispatch_cpu_fast,
		dispatch_cpu_temporal,
		dispatch_gpu_dx11
	};
	ProcessingRoute route{};
	const bool result = dispatch_from_ui_selection(ui, hardware_count, is_avx2_available(), handlers, &route);
	g_runtime_gpu_adapter_ordinal = route.gpuAdapterOrdinal;
	return result;
}

FILTER_ITEM_TRACK item_search_radius = FILTER_ITEM_TRACK(L"空間範囲", 3.0, 1.0, 16.0, 1.0);
FILTER_ITEM_TRACK item_time_radius = FILTER_ITEM_TRACK(L"時間範囲", 0.0, 0.0, 7.0, 1.0);
FILTER_ITEM_TRACK item_sigma = FILTER_ITEM_TRACK(L"分散", 50.0, 0.0, 100.0, 1.0);
FILTER_ITEM_TRACK item_fast_spatial_step = FILTER_ITEM_TRACK(L"Fast間引き", 2.0, 1.0, 4.0, 1.0);
FILTER_ITEM_TRACK item_temporal_decay = FILTER_ITEM_TRACK(L"Temporal減衰", 1.0, 0.0, 4.0, 0.1);
FILTER_ITEM_TRACK item_gpu_spatial_step = FILTER_ITEM_TRACK(L"GPU間引き", 1.0, 1.0, 4.0, 1.0);
FILTER_ITEM_TRACK item_gpu_temporal_decay = FILTER_ITEM_TRACK(L"GPU時間減衰", 0.0, 0.0, 4.0, 0.1);
FILTER_ITEM_TRACK item_gpu_coop_count = FILTER_ITEM_TRACK(L"GPU協調数", 1.0, 1.0, 4.0, 1.0);
FILTER_ITEM_SELECT::ITEM item_mode_list[] = {
	{ L"CPU (Naive)", kModeCpuNaive },
	{ L"CPU (AVX2)", kModeCpuAvx2 },
	{ L"CPU (Fast)", kModeCpuFast },
	{ L"CPU (Temporal)", kModeCpuTemporal },
	{ L"GPU (DirectX 11)", kModeGpuDx11 },
	{ nullptr }
};
FILTER_ITEM_SELECT item_mode = FILTER_ITEM_SELECT(L"計算モード", kModeGpuDx11, item_mode_list);
FILTER_ITEM_SELECT::ITEM item_gpu_adapter_list[] = {
	{ L"Auto", 0 },
	{ nullptr }
};
FILTER_ITEM_SELECT item_gpu_adapter = FILTER_ITEM_SELECT(L"GPUアダプタ", 0, item_gpu_adapter_list);
void* items[] = {
	&item_search_radius,
	&item_time_radius,
	&item_sigma,
	&item_fast_spatial_step,
	&item_temporal_decay,
	&item_gpu_spatial_step,
	&item_gpu_temporal_decay,
	&item_gpu_coop_count,
	&item_mode,
	&item_gpu_adapter,
	nullptr
};

FILTER_PLUGIN_TABLE filter_plugin_table = {
	FILTER_PLUGIN_TABLE::FLAG_VIDEO,
	L"NL-Means Filter (ExEdit2)",
	L"NL-Means",
	L"NL-Means Filter for AviUtl ExEdit2",
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

	CComPtr<IDXGIFactory1> factory;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
		g_gpu_adapter_items.push_back({ nullptr, 0 });
		item_gpu_adapter.list = g_gpu_adapter_items.data();
		item_gpu_adapter.value = 0;
		return;
	}

	std::vector<CComPtr<IDXGIAdapter1>> hardwareAdapters;
	std::vector<DXGI_ADAPTER_DESC1> hardwareAdapterDescs;
	enumerate_hardware_adapters(factory, hardwareAdapters, &hardwareAdapterDescs);
	for (size_t i = 0; i < hardwareAdapterDescs.size(); ++i) {
		g_gpu_adapter_names.emplace_back(hardwareAdapterDescs[i].Description);
		g_gpu_adapter_items.push_back({
			g_gpu_adapter_names.back().c_str(),
			static_cast<int>(g_gpu_adapter_items.size())
		});
	}

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
	g_gpu_coop_runners.clear();
	g_cpu_temporal_history.clear();
}
#endif
