// Copyright 2008 nodchip
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
#include <cmath>
#include <algorithm>
#include <atlbase.h>
#include "ProcessorCpu.h"

using namespace std;

ProcessorCpu::ProcessorCpu() : currentYcpFilteringCacheSize(-1)
{
}

ProcessorCpu::~ProcessorCpu()
{
}

//---------------------------------------------------------------------
//		CPUによる演算ルーチン
//---------------------------------------------------------------------
BOOL ProcessorCpu::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	//	fp->track[n]			: トラックバーの数値
	//	fp->check[n]			: チェックボックスの数値
	//	fpip->w 				: 実際の画像の横幅
	//	fpip->h 				: 実際の画像の縦幅
	//	fpip->max_w				: 画像領域の横幅
	//	fpip->max_h				: 画像領域の縦幅
	//	fpip->ycp_edit			: 画像領域へのポインタ
	//	fpip->ycp_temp			: テンポラリ領域へのポインタ
	//	fpip->ycp_edit[n].y		: 画素(輝度    )データ (     0 ～ 4095 )
	//	fpip->ycp_edit[n].cb	: 画素(色差(青))データ ( -2048 ～ 2047 )
	//	fpip->ycp_edit[n].cr	: 画素(色差(赤))データ ( -2048 ～ 2047 )

	const int width = fpip.w;
	const int height = fpip.h;
	const int maxW = fpip.max_w;

	const int searchRadius = fp.track[0];
	const int neighborhoodRadius = 1;

	const int timeSearchRadius = fp.track[1];
	const int timeSearchDiameter = timeSearchRadius * 2 + 1;
	PIXEL_YC* frames[16];

	if (this->width != width || this->height != height || this->numberOfFrames != numberOfFrames || currentYcpFilteringCacheSize != timeSearchDiameter){
		this->width = width;
		this->height = height;
		this->numberOfFrames = numberOfFrames;
		this->currentYcpFilteringCacheSize = timeSearchDiameter;

		//前のキャッシュが残る不具合(?)対策
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, 0, NULL);
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, timeSearchDiameter, NULL);
	}

	//フレームを読み込む
	const int currentFrame = fp.exfunc->get_frame(fpip.editp);
	const int numberOfFrames = fp.exfunc->get_frame_n(fpip.editp);
	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		const int targetFrame = max(0, min(numberOfFrames - 1, currentFrame + dt));
		frames[timeSearchRadius + dt] = fp.exfunc->get_ycp_filtering_cache_ex(&fp, fpip.editp, targetFrame, NULL, NULL);
	}

	const double H = pow(10, (double)fp.track[2] / 22.0);
	const double H2 = 1.0 / (H * H);

	int y;
	//何も書かないとすべてprivate変数扱いとなるらしい
#pragma omp parallel for
	for (y = 0; y < height; ++y){
		for (int x = 0; x < width; ++x){
			for (int channel = 0; channel < 3; ++channel){
				//重みを計算する
				//非局所平均化
				double sum = 0;
				double value = 0;
				int index = 0;
				for (int dy = -searchRadius; dy <= searchRadius; ++dy){
					for (int dx = -searchRadius; dx <= searchRadius; ++dx){
						for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
							long long sum2 = 0;
							for (int dy2 = -neighborhoodRadius; dy2 <= neighborhoodRadius; ++dy2){
								for (int dx2 = -neighborhoodRadius; dx2 <= neighborhoodRadius; ++dx2){
									const int a = getPixelValue(fpip, frames, x + dx2, y + dy2, channel, 0 + timeSearchRadius);
									const int b = getPixelValue(fpip, frames, x + dx + dx2, y + dy + dy2, channel, dt + timeSearchRadius);
									const int diff = a - b;
									sum2 += diff * diff;
								}
							}
							const double w = exp(-sum2 * H2);
							sum += w;
							value += getPixelValue(fpip, frames, x + dx, y + dy, channel, dt + timeSearchRadius) * w;
						}
					}
				}

				value /= sum;

				setPixelValue(fpip, x, y, channel, (int)value);
			}
		}
	}

	swap(fpip.ycp_edit, fpip.ycp_temp);

	return TRUE;
}

int ProcessorCpu::getPixelValue(FILTER_PROC_INFO& fpip, PIXEL_YC* frames[], int x, int y, int channel, int t)
{
	x = max(0, min(fpip.w - 1, x));
	y = max(0, min(fpip.h - 1, y));
	short* p = (short*)(&frames[t][x + y * fpip.w]);
	return p[channel];
}

void ProcessorCpu::setPixelValue(FILTER_PROC_INFO& fpip, int x, int y, int channel, int value)
{
	x = max(0, min(fpip.w - 1, x));
	y = max(0, min(fpip.h - 1, y));
	short* p = (short*)(&fpip.ycp_temp[x + y * fpip.max_w]);
	p[channel] = (short)value;
}
