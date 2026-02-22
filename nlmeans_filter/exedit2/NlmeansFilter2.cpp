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

// 空間窓を間引く Fast NLM（近似）で 1 画素を処理する。
PIXEL_RGBA process_pixel_fast(
	const std::vector<PIXEL_RGBA>& input,
	int width,
	int height,
	int x,
	int y,
	int search_radius,
	double sigma2,
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
	const double sigma2 = sigma * sigma;
	const int spatial_step = resolve_fast_spatial_step(static_cast<int>(item_fast_spatial_step.value));

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			g_output_pixels[static_cast<size_t>(y * width + x)] =
				process_pixel_fast(g_input_pixels, width, height, x, y, search_radius, sigma2, spatial_step);
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
	double sigma2,
	double temporal_decay)
{
	const PIXEL_RGBA& center = historyFrames.back()[static_cast<size_t>(y * width + x)];
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

				const double dr = static_cast<double>(sample.r) - static_cast<double>(center.r);
				const double dg = static_cast<double>(sample.g) - static_cast<double>(center.g);
				const double db = static_cast<double>(sample.b) - static_cast<double>(center.b);
				const double dist2 = dr * dr + dg * dg + db * db;
				const double w = std::exp(-dist2 / sigma2) * temporalWeight;

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
	const double sigma2 = sigma * sigma;
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
				sigma2,
				temporal_decay);
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

			int failed_tile_count = 0;
			std::vector<bool> tile_success(tiles.size(), false);
			for (const auto& tile : tiles) {
				Exedit2GpuRunner* runner = g_gpu_coop_runners[tile.adapterIndex].get();
				if (runner == nullptr) {
					++failed_tile_count;
					continue;
				}
				if (!runner->initialize(static_cast<int>(tile.adapterIndex))) {
					++failed_tile_count;
					continue;
				}
				const bool ok = runner->process(
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
				if (!ok) {
					++failed_tile_count;
				}
				tile_success[tile.adapterIndex] = ok;
			}

			if (should_retry_failed_tile_on_single_gpu(enable_multi_gpu, failed_tile_count > 0, failed_tile_count, 2)) {
				for (const auto& tile : tiles) {
					if (tile_success[tile.adapterIndex]) {
						continue;
					}
					const bool retried = run_single_gpu();
					if (!retried) {
						break;
					}
					for (int y = tile.yBegin; y < tile.yEnd; ++y) {
						const size_t rowBegin = static_cast<size_t>(y * width);
						std::memcpy(
							temp_outputs[tile.adapterIndex].data() + rowBegin,
							g_output_pixels.data() + rowBegin,
							static_cast<size_t>(width) * sizeof(PIXEL_RGBA));
					}
					tile_success[tile.adapterIndex] = true;
				}
				failed_tile_count = 0;
				for (size_t i = 0; i < tile_success.size(); ++i) {
					if (!tile_success[i]) {
						++failed_tile_count;
					}
				}
			}

			bool coop_failed = failed_tile_count > 0;
			if (!coop_failed && !compose_row_tiled_output(
				width,
				height,
				tiles,
				temp_outputs,
				&g_output_pixels)) {
				coop_failed = true;
			}
			if (coop_failed) {
				if (!run_single_gpu()) {
					return false;
				}
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
