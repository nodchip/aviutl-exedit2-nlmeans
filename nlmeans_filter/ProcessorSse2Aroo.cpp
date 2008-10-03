// Copyright 2008 nod_chip
// Copyright 2008 Aroo		(Vectorization with SSE2)
// Copyright 2008 61◆s3BkVamfwY
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
#include <malloc.h>

#include "ProcessorSse2Aroo.h"
//#include <intrin.h>
//#define _mm_loadu_si128 _mm_lddqu_si128
//SSE3ならこっちのほうがいいのかなぁと思ってみたりしますが
//AthlonX2 64 5600+だと大して差がありませんでした

// stdafx.hで_USE_MATH_DEFINESが定義されていないので
#define M_LN2      0.693147180559945309417

namespace {

// wikipediaのen:exponential_functionを参考に
// 引数を[-88.?? ,88.??]に絞ってベクトル化
// exp(x),x<-87は0ってことで
// u : [0,ln(2)]なのでとりあえず2乗で打ち切り <- 精度低下の原因

static const __m128 ln2 = _mm_set_ps1(M_LN2);
static const __m128 inv_ln2 = _mm_set_ps1(1/M_LN2);
static const __m128 f1 = _mm_set_ps1(1);
static const __m128 inv_f2 = _mm_set_ps1(0.5);
static const __m128i exp_nmin = _mm_set1_epi32(-128);
static const __m128i exp_bias = _mm_set1_epi32(127);

inline __m128 exp(const __m128 x)
{
	const __m128i n = _mm_cvttps_epi32(_mm_mul_ps(x,inv_ln2));
	const __m128 u = _mm_sub_ps(x,_mm_mul_ps(_mm_cvtepi32_ps(n),ln2));
	const __m128 m = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(u,u),inv_f2),_mm_add_ps(u,f1));
	const __m128i tPofn = _mm_slli_epi32(_mm_add_epi32(n,exp_bias),23);
	const __m128i mask = _mm_cmpgt_epi32(n,exp_nmin);
	return _mm_and_ps(_mm_mul_ps(m,*(const __m128*)&tPofn),*(const __m128*)&mask);
}

// 以下を参考に単精度版を作成
// 精度が悪いようなら
// http://citeseer.ist.psu.edu/schraudolph98fast.html
// y[31:16] = a * x + b - c
// a = (2^(23-16))/ln(2)
// 23 : frac bit width
// 16 : integer(short) bit width
// b = 127 * 2^(23-16)
// c = ?

static const __m128 fastexp_a = _mm_set_ps1(0x80/M_LN2);
static const __m128i fastexp_bmc = _mm_set1_epi32(127*0x80 - 9);
static const __m128 fastexp_min = _mm_set_ps1(-87);

inline __m128 fastexp(const __m128 x)
{
	union {
		__m128 f;
		__m128i i;
	};
	const __m128 mask = _mm_cmpge_ps(x,fastexp_min);
	const __m128i ax = _mm_cvtps_epi32(_mm_mul_ps(x,fastexp_a));
	i = _mm_and_si128(_mm_slli_epi32(_mm_add_epi32(ax,fastexp_bmc),16),*(__m128i*)&mask);
	return f;
}

// 一応
inline __m128 refexp(__m128 x)
{
	_mm_empty();
	for(int i=0;i<4;++i) {
		x.m128_f32[i] = std::exp(x.m128_f32[i]);
	}
	return x;
}

void copyS3toS4(__m64 * const pOut,
				const int pitch,
				const PIXEL_YC * const pIn,
				const int w,
				const int h,
				const int max_w,
				const int clamp_w)
{
	const int pitch_m128 = pitch / 2;
	const int xcount = (w-2+7)/8;
	const int sp = 8 - (w-2)%8;
	const PIXEL_YC * const pIn_m128 = pIn + 1;
	const PIXEL_YC *pCrntIn = pIn_m128;
	__m128i * const pOut_m128 = (__m128i*)(pOut + 1);
	__m128i *pCrntOut = pOut_m128;

	for( int x=0; x < xcount; ++x ) {
		//__m128i a = _mm_loadu_si128((__m128i*)((short*)pCrntIn -1));
		//__m128i b = _mm_load_si128 ((__m128i*)((short*)pCrntIn +5));
		//__m128i c = _mm_loadu_si128((__m128i*)((short*)pCrntIn+11));
		//__m128i d = _mm_loadu_si128((__m128i*)((short*)pCrntIn+17));
		__m128i a = _mm_loadl_epi64((__m128i*)((short*)pCrntIn -1));
		a = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&a,(double*)((short*)pCrntIn+3));
		__m128i b = _mm_load_si128 ((__m128i*)((short*)pCrntIn +5));
		__m128i c = _mm_loadl_epi64((__m128i*)((short*)pCrntIn+11));
		c = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&c,(double*)((short*)pCrntIn+15));
		__m128i d = _mm_loadl_epi64((__m128i*)((short*)pCrntIn+17));
		d = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&d,(double*)((short*)pCrntIn+21));
		a = _mm_shufflelo_epi16(a,_MM_SHUFFLE(0,3,2,1));
		_mm_stream_si128(pCrntOut,a);
		b = _mm_shufflelo_epi16(b,_MM_SHUFFLE(0,3,2,1));
		_mm_stream_si128(pCrntOut+1,b);
		c = _mm_shufflelo_epi16(c,_MM_SHUFFLE(0,3,2,1));
		_mm_stream_si128(pCrntOut+2,c);
		d = _mm_shufflelo_epi16(d,_MM_SHUFFLE(0,3,2,1));
		_mm_stream_si128(pCrntOut+3,d);
		__m128i *pCrntOutClamp = pCrntOut;
		for(int y=0;y<clamp_w;++y) {
			__m128i *pCrntOutClampY = pCrntOutClamp -= pitch_m128;
			_mm_stream_si128(pCrntOutClampY++,a);
			_mm_stream_si128(pCrntOutClampY++,b);
			_mm_stream_si128(pCrntOutClampY++,c);
			_mm_stream_si128(pCrntOutClampY++,d);
		}
		pCrntIn += 8;
		pCrntOut += 4;
	}

	int y;
#pragma omp parallel for
	for( y = 1; y < h-1; ++y ) {
		const PIXEL_YC *pCrntInX = pIn + max_w * y;

		__m64 head = *(__m64*)pCrntInX;
		__m64 *pCrntOutXClamp = pOut + pitch * y - clamp_w;
		for(int x=0; x < clamp_w + 1; ++x) {
			_mm_stream_pi(pCrntOutXClamp++,head);
		}

		++pCrntInX;
		__m128i *pCrntOutX = pOut_m128 + pitch_m128 * y;
		for( int x=0; x < xcount; ++x ) {
			//__m128i a = _mm_loadu_si128((__m128i*)((short*)pCrntInX -1));
			//__m128i b = _mm_load_si128 ((__m128i*)((short*)pCrntInX +5));
			//__m128i c = _mm_loadu_si128((__m128i*)((short*)pCrntInX+11));
			//__m128i d = _mm_loadu_si128((__m128i*)((short*)pCrntInX+17));
			__m128i a = _mm_loadl_epi64((__m128i*)((short*)pCrntInX -1));
			a = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&a,(double*)((short*)pCrntInX+3));
			__m128i b = _mm_load_si128 ((__m128i*)((short*)pCrntInX +5));
			__m128i c = _mm_loadl_epi64((__m128i*)((short*)pCrntInX+11));
			c = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&c,(double*)((short*)pCrntInX+15));
			__m128i d = _mm_loadl_epi64((__m128i*)((short*)pCrntInX+17));
			d = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&d,(double*)((short*)pCrntInX+21));
			a = _mm_shufflelo_epi16(a,_MM_SHUFFLE(0,3,2,1));
			_mm_stream_si128(pCrntOutX++,a);
			b = _mm_shufflelo_epi16(b,_MM_SHUFFLE(0,3,2,1));
			_mm_stream_si128(pCrntOutX++,b);
			c = _mm_shufflelo_epi16(c,_MM_SHUFFLE(0,3,2,1));
			_mm_stream_si128(pCrntOutX++,c);
			d = _mm_shufflelo_epi16(d,_MM_SHUFFLE(0,3,2,1));
			_mm_stream_si128(pCrntOutX++,d);

			pCrntInX += 8;
		}

		__m64 tail = *(__m64*)(pCrntInX - sp);
		pCrntOutXClamp = (__m64*)pCrntOutX - sp;
		for(int x=0; x < clamp_w + 1; ++x) {
			_mm_stream_pi(pCrntOutXClamp++,tail);
		}
	}

	pCrntIn = pIn_m128 + max_w * (h-1);
	pCrntOut = pOut_m128 + pitch_m128 * (h-1);

	for( int x=0; x < xcount; ++x ) {
		//__m128i a = _mm_loadu_si128((__m128i*)((short*)pCrntIn -1));
		//__m128i b = _mm_load_si128 ((__m128i*)((short*)pCrntIn +5));
		//__m128i c = _mm_loadu_si128((__m128i*)((short*)pCrntIn+11));
		//__m128i d = _mm_loadu_si128((__m128i*)((short*)pCrntIn+17));
		__m128i a = _mm_loadl_epi64((__m128i*)((short*)pCrntIn -1));
		a = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&a,(double*)((short*)pCrntIn+3));
		__m128i b = _mm_load_si128 ((__m128i*)((short*)pCrntIn +5));
		__m128i c = _mm_loadl_epi64((__m128i*)((short*)pCrntIn+11));
		c = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&c,(double*)((short*)pCrntIn+15));
		__m128i d = _mm_loadl_epi64((__m128i*)((short*)pCrntIn+17));
		d = *(__m128i*)&_mm_loadh_pd(*(__m128d*)&d,(double*)((short*)pCrntIn+21));
		a = _mm_shufflelo_epi16(a,_MM_SHUFFLE(0,3,2,1));
		_mm_stream_si128(pCrntOut,a);
		b = _mm_shufflelo_epi16(b,_MM_SHUFFLE(0,3,2,1));
		_mm_stream_si128(pCrntOut+1,b);
		c = _mm_shufflelo_epi16(c,_MM_SHUFFLE(0,3,2,1));
		_mm_stream_si128(pCrntOut+2,c);
		d = _mm_shufflelo_epi16(d,_MM_SHUFFLE(0,3,2,1));
		_mm_stream_si128(pCrntOut+3,d);
		__m128i *pCrntOutClamp = pCrntOut;
		for(int y=0;y<clamp_w;++y) {
			__m128i *pCrntOutClampY = pCrntOutClamp += pitch_m128;
			_mm_stream_si128(pCrntOutClampY++,a);
			_mm_stream_si128(pCrntOutClampY++,b);
			_mm_stream_si128(pCrntOutClampY++,c);
			_mm_stream_si128(pCrntOutClampY++,d);
		}
		pCrntIn += 8;
		pCrntOut += 4;
	}

	__m64 tl = *(__m64*)&pIn[0];
	__m64 tr = *(__m64*)&pIn[w-1];
	__m64 bl = *(__m64*)&pIn[max_w * (h-1)];
	__m64 br = *(__m64*)&pIn[max_w * (h-1) + w-1];
	__m64 *ptl = (__m64*)pOut - (pitch+1) * clamp_w;
	__m64 *ptr = ptl + clamp_w + w - 1;
	__m64 *pbl = ptl + pitch * (clamp_w + h - 1);
	__m64 *pbr = pbl + clamp_w + w - 1;
	for( int y = 0; y < clamp_w + 1; ++y) {
		for ( int x = 0; x < clamp_w + 1; ++x) {
			_mm_stream_pi(ptl + pitch * y + x, tl);
			_mm_stream_pi(ptr + pitch * y + x, tr);
			_mm_stream_pi(pbl + pitch * y + x, bl);
			_mm_stream_pi(pbr + pitch * y + x, br);
		}
	}
}

}

extern int track_e[];

ProcessorSse2Aroo::AllocMemory::AllocMemory()
: pitch(0),w(0),h(0),s(0),c(0),ap(NULL)
, max_radius(((track_e[0] + 1 + 2) / 2) * 2 + 1)
// 最初の + 1 はneighborhoodRadius分
// 最後の + 1 はアラインメントのため
{}

ProcessorSse2Aroo::AllocMemory::~AllocMemory()
{
	_aligned_free(ap);
}

__m64* ProcessorSse2Aroo::AllocMemory::allocate(int nw, int nh, int nc)
{
	if(nw == w && nh == h && nc == c)
		return ap;

	int np = nw + 2 * max_radius;
	int ns = np * (nh + 2 * max_radius);
	if(ns != s || nc != c) {
		deallocate();
		ap = (__m64*)_aligned_malloc(ns * sizeof(__m64) * nc,16);
		if(ap) {
			for(int i=0;i<nc;++i) {
				pv[i] = ap + ns * i + (np + 1) * max_radius;
			}
			pitch = np;
			w = nw;
			h = nh;
			s = ns;
			c = nc;
		}
	} else {
		w = nw;
		h = nh;
		pitch = np;
	}
	return ap;
}

void ProcessorSse2Aroo::AllocMemory::deallocate()
{
	_aligned_free(ap);
	pitch = w = h = s = c = 0;
	std::fill_n(pv,_countof(pv),(__m64*)NULL);
	ap = NULL;
}

ProcessorSse2Aroo::ProcessorSse2Aroo()
: prepared(IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
, currentYcpFilteringCacheSize(-1), width(-1), height(-1), numberOfFrames(-1)
{
}

ProcessorSse2Aroo::~ProcessorSse2Aroo()
{
}

BOOL ProcessorSse2Aroo::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
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
	const int w = fpip.w;
	const int h = fpip.h;
	const int max_w = fpip.max_w;
	const int max_h = fpip.max_h;

	const int searchRadius = fp.track[0];
	const int neighborhoodRadius = 1;
	const float H = pow(10, (float)fp.track[2] / 22.0f);
	const __m128 mH2 = _mm_set_ps1(-1.0f / (H * H));

	const int timeSearchRadius = fp.track[1];
	const int timeSearchDiameter = timeSearchRadius * 2 + 1;

	if( !mem.allocate(max_w,max_h,timeSearchDiameter) ) {
		return FALSE;
	}

	const int clamp_w = searchRadius+neighborhoodRadius;
	const int pitch = mem.pitch;

	const int currentFrame = fpip.frame;
	const int numberOfFrames = fp.exfunc->get_frame_n(fpip.editp);

	//キャッシュサイズを指定する
	if (currentYcpFilteringCacheSize != timeSearchDiameter || this->width != w || this->height != h || this->numberOfFrames != numberOfFrames){
		this->width = w;
		this->height = h;
		this->numberOfFrames = numberOfFrames;
		this->currentYcpFilteringCacheSize = timeSearchDiameter;
		fp.exfunc->set_ycp_filtering_cache_size(&fp, max_w, h, 0, NULL);
		fp.exfunc->set_ycp_filtering_cache_size(&fp, max_w, h, timeSearchDiameter, NULL);
	}
	//if (currentYcpFilteringCacheSize != timeSearchDiameter){
	//	currentYcpFilteringCacheSize = timeSearchDiameter;
	//}

	//フレームを読み込む
	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		const int targetFrame = max(0, min(numberOfFrames - 1, currentFrame + dt));
		__m64 *p = mem.pv[timeSearchRadius + dt];
		PIXEL_YC *q = fp.exfunc->get_ycp_filtering_cache_ex(&fp, fpip.editp, targetFrame, NULL, NULL);
		copyS3toS4(p, pitch, q, w, h, max_w, clamp_w);
	}

	__m64 ** p = mem.pv;
	PIXEL_YC *q = fpip.ycp_edit;
	const int xcount = (w+1)/2;

	int y;
#pragma omp parallel for
	for(y = 0; y < h; ++y) {
		for(int x = 0; x < xcount; ++x) {
			__m128 usum = _mm_setzero_ps();
			__m128 uvalue = _mm_setzero_ps();
			__m128 vsum = _mm_setzero_ps();
			__m128 vvalue = _mm_setzero_ps();
			for(int dy = -searchRadius; dy <= searchRadius; ++dy) {
				for(int dx = -searchRadius; dx <= searchRadius; ++dx) {
					for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
						__m128i usum2 = _mm_setzero_si128();
						__m128i vsum2 = _mm_setzero_si128();
						const __m64 *mp = p[timeSearchRadius];
						const __m64 *np = p[timeSearchRadius + dt];
						for(int dy2 = -neighborhoodRadius; dy2 <= neighborhoodRadius; ++dy2) {
							__m128i u1 = _mm_load_si128((const __m128i*)(mp + pitch * (y + dy2) + 2 * x - 1));
							__m128i u2 = _mm_load_si128((const __m128i*)(mp + pitch * (y + dy2) + 2 * x + 1));
							__m128i v1;
							__m128i v2;
							if(dx&1) {
								v1 = _mm_loadu_si128((const __m128i*)(np + pitch * (y + dy + dy2) + 2 * x + dx - 1));
								v2 = _mm_loadu_si128((const __m128i*)(np + pitch * (y + dy + dy2) + 2 * x + dx + 1));
							} else {
								v1 = _mm_load_si128 ((const __m128i*)(np + pitch * (y + dy + dy2) + 2 * x + dx - 1));
								v2 = _mm_load_si128 ((const __m128i*)(np + pitch * (y + dy + dy2) + 2 * x + dx + 1));
							}
							u1 = _mm_sub_epi16(u1,v1);
							u2 = _mm_sub_epi16(u2,v2);
							v1 = _mm_unpacklo_epi16(u1,u2);
							v2 = _mm_unpackhi_epi16(u1,u2);
							v1 = _mm_madd_epi16(v1,v1);
							v2 = _mm_madd_epi16(v2,v2);

							usum2 = _mm_add_epi32(usum2,v1);
							vsum2 = _mm_add_epi32(vsum2,v2);

							u1 = *(__m128i*)&_mm_shuffle_pd(*(__m128d*)&u1,*(__m128d*)&u2,_MM_SHUFFLE2(0,1));
							v1 = _mm_mullo_epi16(u1,u1);
							v2 = _mm_mulhi_epi16(u1,u1);
							u1 = _mm_unpacklo_epi16(v1,v2);
							u2 = _mm_unpackhi_epi16(v1,v2);

							usum2 = _mm_add_epi32(usum2,u1);
							vsum2 = _mm_add_epi32(vsum2,u2);
						}
						__m128 usum2f = _mm_cvtepi32_ps(usum2);
						__m128 vsum2f = _mm_cvtepi32_ps(vsum2);
						usum2f = _mm_mul_ps(usum2f,mH2);
						vsum2f = _mm_mul_ps(vsum2f,mH2);
						// 精度やや低
						//usum2f = exp(_mm_mul_ps(usum2f,mH2));
						//vsum2f = exp(_mm_mul_ps(vsum2f,mH2));
						// 精度悪
						usum2f = fastexp(usum2f);
						vsum2f = fastexp(vsum2f);

						usum = _mm_add_ps(usum,usum2f);
						vsum = _mm_add_ps(vsum,vsum2f);

						__m128i v = _mm_loadu_si128((const __m128i*)(np + pitch * (y + dy) + 2 * x + dx));
						__m128i u = _mm_unpacklo_epi16(v,v);
						v = _mm_unpackhi_epi16(v,v);
						u = _mm_srai_epi32(u,16);
						v = _mm_srai_epi32(v,16);
						__m128 uf = _mm_cvtepi32_ps(u);
						__m128 vf = _mm_cvtepi32_ps(v);

						uf = _mm_mul_ps(uf,usum2f);
						vf = _mm_mul_ps(vf,vsum2f);
						uvalue = _mm_add_ps(uvalue,uf);
						vvalue = _mm_add_ps(vvalue,vf);
					}
				}
			}
			// 精度高
			//uvalue = _mm_div_ps(uvalue,usum);
			//vvalue = _mm_div_ps(vvalue,vsum);
			// 精度低
			// 11bit精度逆数の積なので精度低し
			usum = _mm_rcp_ps(usum);
			vsum = _mm_rcp_ps(vsum);
			uvalue = _mm_mul_ps(uvalue,usum);
			vvalue = _mm_mul_ps(vvalue,vsum);

			__m128i valuei = _mm_cvtps_epi32(uvalue);
			__m128i vvaluei = _mm_cvtps_epi32(vvalue);
			valuei = _mm_packs_epi32(valuei,vvaluei);
			valuei = _mm_shufflelo_epi16(valuei,_MM_SHUFFLE(2,1,0,3));
			valuei = _mm_srli_si128(valuei,2);
			_mm_stream_pi((__m64*)&q[max_w * y + 2 * x],*(__m64*)&valuei);
			_mm_stream_si32((int*)&q[max_w * y + 2 * x] + 2,valuei.m128i_i32[2]);
		}
	}

	_mm_empty();

	return TRUE;
}

BOOL ProcessorSse2Aroo::wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	if(message == WM_FILTER_CHANGE_ACTIVE && !fp->exfunc->is_filter_active(fp)) {
		mem.deallocate();
	}
	return FALSE;
}
