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

#ifndef NLM_FRAME_KERNEL_H
#define NLM_FRAME_KERNEL_H

#include <algorithm>
#include <cmath>
#include "NlmKernel.h"

// フレーム列から1チャンネル値を取得する。範囲外はクランプする。
inline int get_frame_channel(
	const short* input,
	int width,
	int height,
	int frameCount,
	int x,
	int y,
	int channel,
	int t)
{
	const int cx = std::max(0, std::min(width - 1, x));
	const int cy = std::max(0, std::min(height - 1, y));
	const int ct = std::max(0, std::min(frameCount - 1, t));
	const size_t index = static_cast<size_t>(((ct * height + cy) * width + cx) * 3 + channel);
	return static_cast<int>(input[index]);
}

inline long long patch_ssd_at_naive(
	const short* input,
	int width,
	int height,
	int frameCount,
	int x,
	int y,
	int dx,
	int dy,
	int dt,
	int channel,
	int currentFrame)
{
	int a[9];
	int b[9];
	int i = 0;
	for (int py = -1; py <= 1; ++py) {
		for (int px = -1; px <= 1; ++px) {
			a[i] = get_frame_channel(input, width, height, frameCount, x + px, y + py, channel, currentFrame);
			b[i] = get_frame_channel(input, width, height, frameCount, x + dx + px, y + dy + py, channel, currentFrame + dt);
			++i;
		}
	}
	return patch_ssd_scalar_3x3(a, b);
}

inline long long patch_ssd_at_avx2(
	const short* input,
	int width,
	int height,
	int frameCount,
	int x,
	int y,
	int dx,
	int dy,
	int dt,
	int channel,
	int currentFrame)
{
	int a[9];
	int b[9];
	int i = 0;
	for (int py = -1; py <= 1; ++py) {
		for (int px = -1; px <= 1; ++px) {
			a[i] = get_frame_channel(input, width, height, frameCount, x + px, y + py, channel, currentFrame);
			b[i] = get_frame_channel(input, width, height, frameCount, x + dx + px, y + dy + py, channel, currentFrame + dt);
			++i;
		}
	}
	return patch_ssd_avx2_3x3(a, b);
}

// Naive 実装で 1 画素 1 チャンネルを計算する。
inline int nlm_filter_pixel_channel_naive(
	const short* input,
	int width,
	int height,
	int frameCount,
	int x,
	int y,
	int channel,
	int currentFrame,
	int searchRadius,
	int timeRadius,
	double h2)
{
	double sum = 0.0;
	double value = 0.0;

	for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
		for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
			for (int dt = -timeRadius; dt <= timeRadius; ++dt) {
				const long long ssd = patch_ssd_at_naive(input, width, height, frameCount, x, y, dx, dy, dt, channel, currentFrame);
				const double w = std::exp(-static_cast<double>(ssd) * h2);
				sum += w;
				value += static_cast<double>(get_frame_channel(input, width, height, frameCount, x + dx, y + dy, channel, currentFrame + dt)) * w;
			}
		}
	}

	if (sum <= 0.0) {
		return get_frame_channel(input, width, height, frameCount, x, y, channel, currentFrame);
	}
	return static_cast<int>(value / sum);
}

// AVX2 実装で 1 画素 1 チャンネルを計算する。
inline int nlm_filter_pixel_channel_avx2(
	const short* input,
	int width,
	int height,
	int frameCount,
	int x,
	int y,
	int channel,
	int currentFrame,
	int searchRadius,
	int timeRadius,
	double h2)
{
	double sum = 0.0;
	double value = 0.0;

	for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
		for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
			for (int dt = -timeRadius; dt <= timeRadius; ++dt) {
				const long long ssd = patch_ssd_at_avx2(input, width, height, frameCount, x, y, dx, dy, dt, channel, currentFrame);
				const double w = std::exp(-static_cast<double>(ssd) * h2);
				sum += w;
				value += static_cast<double>(get_frame_channel(input, width, height, frameCount, x + dx, y + dy, channel, currentFrame + dt)) * w;
			}
		}
	}

	if (sum <= 0.0) {
		return get_frame_channel(input, width, height, frameCount, x, y, channel, currentFrame);
	}
	return static_cast<int>(value / sum);
}

#endif
