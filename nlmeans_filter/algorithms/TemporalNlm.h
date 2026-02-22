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

#ifndef TEMPORAL_NLM_H
#define TEMPORAL_NLM_H

#include <algorithm>
#include <cmath>
#include "../NlmFrameKernel.h"

// 時間差に応じた減衰重みを加える Temporal NLM を 1 フレームへ適用する。
inline void nlm_filter_frame_temporal_naive(
	const short* input,
	int width,
	int height,
	int frameCount,
	int currentFrame,
	int searchRadius,
	int timeRadius,
	double h2,
	double temporalDecay,
	short* output)
{
	const double decay = std::max(0.0, temporalDecay);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			for (int channel = 0; channel < 3; ++channel) {
				double sum = 0.0;
				double value = 0.0;

				for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
					for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
						for (int dt = -timeRadius; dt <= timeRadius; ++dt) {
							const long long ssd = patch_ssd_at_naive(
								input,
								width,
								height,
								frameCount,
								x,
								y,
								dx,
								dy,
								dt,
								channel,
								currentFrame);
							const double spatialWeight = std::exp(-static_cast<double>(ssd) * h2);
							const double temporalWeight = std::exp(-std::abs(dt) * decay);
							const double w = spatialWeight * temporalWeight;
							sum += w;
							value += static_cast<double>(get_frame_channel(
								input,
								width,
								height,
								frameCount,
								x + dx,
								y + dy,
								channel,
								currentFrame + dt)) * w;
						}
					}
				}

				const int center = get_frame_channel(input, width, height, frameCount, x, y, channel, currentFrame);
				const int outValue = (sum <= 0.0) ? center : static_cast<int>(value / sum);
				const size_t outIndex = static_cast<size_t>((y * width + x) * 3 + channel);
				output[outIndex] = static_cast<short>(outValue);
			}
		}
	}
}

#endif
