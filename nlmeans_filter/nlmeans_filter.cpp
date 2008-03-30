// nlmeans_filter.cpp : DLL アプリケーション用にエクスポートされる関数を定義します。
//

#include "stdafx.h"
#include <cmath>
#include <algorithm>

using namespace std;

//---------------------------------------------------------------------
//		サンプルインターレース解除プラグイン  for AviUtl ver0.98以降
//---------------------------------------------------------------------
#include	"filter.h"


//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define	TRACK_N	2									//	トラックバーの数
TCHAR	*track_name[] =		{"範囲", "分散"};	//	トラックバーの名前
int		track_default[] =	{1, 20};	//	トラックバーの初期値
int		track_s[] =			{1, 0,};	//	トラックバーの下限値
int		track_e[] =			{10, 100,};	//	トラックバーの上限値
#define	CHECK_N	0

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0,0,
	"NL-Meansフィルタ",
	TRACK_N,track_name,track_default,
	track_s,track_e,
	CHECK_N,NULL,NULL,
	func_proc,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,NULL,
	NULL,
	NULL,
	"NL-Meansフィルタ version 0.00 by nod_chip",
	NULL,NULL,
	NULL,NULL,NULL,
	NULL,
	NULL
};


//---------------------------------------------------------------------
//		フィルタ構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable( void )
{
	return &filter;
}


//---------------------------------------------------------------------
//		フィルタ処理関数
//---------------------------------------------------------------------
int get(FILTER_PROC_INFO *fpip, int x, int y, int channel);
void set(FILTER_PROC_INFO *fpip, int x, int y, int channel, int value);
static BOOL func_proc( FILTER *fp,FILTER_PROC_INFO *fpip )
{
//
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

	const int width = fpip->w;
	const int height = fpip->h;

	const int searchRadius = fp->track[0];
	const int neighborhoodRadius = 1;
	const double H = pow(10, (double)fp->track[1] / 10.0);
	const double H2 = 1.0 / (H * H);

	for (int channel = 0; channel < 3; ++channel){
		for (int y = 0; y < height; ++y){
			for (int x = 0; x < width; ++x){
				//重みを計算する
				double weights[1024];
				double sum = 0;
				int index = 0;
				for (int dy = -searchRadius; dy <= searchRadius; ++dy){
					for (int dx = -searchRadius; dx <= searchRadius; ++dx){
						double sum2 = 0;
						for (int dy2 = -neighborhoodRadius; dy2 <= neighborhoodRadius; ++dy2){
							for (int dx2 = -neighborhoodRadius; dx2 <= neighborhoodRadius; ++dx2){
								const double diff = get(fpip, x + dx2, y + dy2, channel) - get(fpip, x + dx + dx2, y + dy + dy2, channel);
								sum2 += diff * diff;
							}
						}
						const double w = exp(-sum2 * H2);
						weights[index++] = w;
						sum += w;
					}
				}

				//非局所平均化
				double value = 0;
				index = 0;
				for (int dy = -searchRadius; dy <= searchRadius; ++dy){
					for (int dx = -searchRadius; dx <= searchRadius; ++dx){
						const double r = get(fpip, x + dx, y + dy, channel) * weights[index++];
						value += r;
					}
				}

				value /= sum;

				set(fpip, x, y, channel, (int)value);
			}
		}
	}
	
	swap(fpip->ycp_edit, fpip->ycp_temp);
	
	return TRUE;
}

int get(FILTER_PROC_INFO *fpip, int x, int y, int channel)
{
	x = max(0, min(fpip->w - 1, x));
	y = max(0, min(fpip->h - 1, y));
	short* p = (short*)(&fpip->ycp_edit[x + y * fpip->max_w]);
	return p[channel];
}

void set(FILTER_PROC_INFO *fpip, int x, int y, int channel, int value)
{
	x = max(0, min(fpip->w - 1, x));
	y = max(0, min(fpip->h - 1, y));
	short* p = (short*)(&fpip->ycp_temp[x + y * fpip->max_w]);
	p[channel] = (short)value;
}
