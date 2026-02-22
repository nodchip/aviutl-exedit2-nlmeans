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
#include <algorithm>
#include <cmath>
#include <immintrin.h>
#include <intrin.h>
#include "ProcessorAvx2.h"
#include "CacheSizing.h"

using namespace std;

ProcessorAvx2::ProcessorAvx2()
	: currentYcpFilteringCacheSize(-1), width(-1), height(-1), numberOfFrames(-1), prepared(detectAvx2())
{
}

ProcessorAvx2::~ProcessorAvx2()
{
}

bool ProcessorAvx2::isPrepared() const
{
	return prepared;
}

BOOL ProcessorAvx2::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	const int width = fpip.w;
	const int height = fpip.h;

	const int searchRadius = fp.track[0];
	const int timeSearchRadius = fp.track[1];
	const int timeSearchDiameter = timeSearchRadius * 2 + 1;
	PIXEL_YC* frames[16];

	const int currentFrame = fp.exfunc->get_frame(fpip.editp);
	const int totalFrames = fp.exfunc->get_frame_n(fpip.editp);
	if (needs_cache_reset(
		this->width,
		this->height,
		this->numberOfFrames,
		this->currentYcpFilteringCacheSize,
		width,
		height,
		totalFrames,
		timeSearchDiameter)) {
		this->width = width;
		this->height = height;
		this->numberOfFrames = totalFrames;
		this->currentYcpFilteringCacheSize = timeSearchDiameter;

		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, 0, NULL);
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, timeSearchDiameter, NULL);
	}

	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		const int targetFrame = max(0, min(totalFrames - 1, currentFrame + dt));
		frames[timeSearchRadius + dt] = fp.exfunc->get_ycp_filtering_cache_ex(&fp, fpip.editp, targetFrame, NULL, NULL);
	}

	const double h = std::pow(10.0, static_cast<double>(fp.track[2]) / 22.0);
	const double h2 = 1.0 / (h * h);

	int y;
#pragma omp parallel for
	for (y = 0; y < height; ++y){
		for (int x = 0; x < width; ++x){
			for (int channel = 0; channel < 3; ++channel){
				double sum = 0.0;
				double value = 0.0;

				for (int dy = -searchRadius; dy <= searchRadius; ++dy){
					for (int dx = -searchRadius; dx <= searchRadius; ++dx){
						for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
							const long long sum2 = calcPatchDiffAvx2(fpip, frames, x, y, dx, dy, dt, channel, timeSearchRadius);
							const double w = exp(-sum2 * h2);
							sum += w;
							value += getPixelValue(fpip, frames, x + dx, y + dy, channel, dt + timeSearchRadius) * w;
						}
					}
				}

				const int out = (sum > 0.0) ? static_cast<int>(value / sum) : getPixelValue(fpip, frames, x, y, channel, timeSearchRadius);
				setPixelValue(fpip, x, y, channel, out);
			}
		}
	}

	swap(fpip.ycp_edit, fpip.ycp_temp);
	return TRUE;
}

BOOL ProcessorAvx2::wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	(void)hwnd;
	(void)message;
	(void)wparam;
	(void)lparam;
	(void)editp;
	(void)fp;
	return FALSE;
}

bool ProcessorAvx2::detectAvx2()
{
	int cpuInfo[4] = {0, 0, 0, 0};
	__cpuid(cpuInfo, 0);
	if (cpuInfo[0] < 7){
		return false;
	}

	__cpuidex(cpuInfo, 7, 0);
	return (cpuInfo[1] & (1 << 5)) != 0;
}

int ProcessorAvx2::getPixelValue(FILTER_PROC_INFO& fpip, PIXEL_YC* frames[], int x, int y, int channel, int t) const
{
	x = max(0, min(fpip.w - 1, x));
	y = max(0, min(fpip.h - 1, y));
	short* p = (short*)(&frames[t][x + y * fpip.w]);
	return p[channel];
}

void ProcessorAvx2::setPixelValue(FILTER_PROC_INFO& fpip, int x, int y, int channel, int value) const
{
	x = max(0, min(fpip.w - 1, x));
	y = max(0, min(fpip.h - 1, y));
	short* p = (short*)(&fpip.ycp_temp[x + y * fpip.max_w]);
	p[channel] = static_cast<short>(value);
}

long long ProcessorAvx2::calcPatchDiffAvx2(
	FILTER_PROC_INFO& fpip,
	PIXEL_YC* frames[],
	int x,
	int y,
	int dx,
	int dy,
	int dt,
	int channel,
	int timeSearchRadius) const
{
	int aVals[8];
	int bVals[8];

	int i = 0;
	for (int py = -1; py <= 1; ++py){
		for (int px = -1; px <= 1; ++px){
			if (i == 8){
				break;
			}
			aVals[i] = getPixelValue(fpip, frames, x + px, y + py, channel, timeSearchRadius);
			bVals[i] = getPixelValue(fpip, frames, x + dx + px, y + dy + py, channel, dt + timeSearchRadius);
			++i;
		}
	}

	__m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(aVals));
	__m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(bVals));
	__m256i d = _mm256_sub_epi32(a, b);
	__m256i s = _mm256_mullo_epi32(d, d);
	alignas(32) int tmp[8];
	_mm256_store_si256(reinterpret_cast<__m256i*>(tmp), s);

	long long sum2 = 0;
	for (int k = 0; k < 8; ++k){
		sum2 += tmp[k];
	}

	const int a9 = getPixelValue(fpip, frames, x + 1, y + 1, channel, timeSearchRadius);
	const int b9 = getPixelValue(fpip, frames, x + dx + 1, y + dy + 1, channel, dt + timeSearchRadius);
	const int d9 = a9 - b9;
	sum2 += d9 * d9;
	return sum2;
}
