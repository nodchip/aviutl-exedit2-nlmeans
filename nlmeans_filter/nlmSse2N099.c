// Copyright 2008 N099
// Copyright 2008 nod_chip (若干の修正のみ)
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

//------------------------------------------------------------------------
// nod_chip氏によるNL-MeansフィルタをSSE2で実装した（つもり）のフィルタ
//  （オリジナルは http://kishibe.dyndns.tv/ で公開されています）
//
//  NL-Meansフィルタのv0.04あたりを参考に実装してあります。
//  パラメータ値解釈はそのまま流用していますが、あとはかなり変更してます。
//  どちらかというと、HLSLのコードと同じようになるように作成しました。
//  また、指数関数をかなりアバウトに近似しているので、結果が微妙に
//  異なってきます。（ひょっとしたら、コード自体に間違いがあるのかも？）
//
//  一応、v0.99a以降のマルチスレッド処理に対応してあります。
//  また、当然ですがSSE2が必須です。
//
// ライセンス:
//  作者（私）は、動作保障やコードのメンテナンス、改良は行いません。
//  また、著作者に認められている権利の一切を行使しないことを保障します。
//  つまり勝手にコピーしたりマージしてもOKですし、その場合でも原作者（私）
//  のクレジット表記などは必要ありません。
//
//------------------------------------------------------------------------
#include <math.h>
#include <windows.h>
#include <intrin.h>
#include <emmintrin.h>

#include "filter.h"

//------------------------------------------------------------------------
// リソース管理用
//------------------------------------------------------------------------
typedef struct _work_mem {
    __m64 *p_head; //確保した先頭アドレス
    __m64 *p_work; //実際に利用する先頭
    LONG ww; //ストライド(@8byte)
} work_mem;

work_mem w_mem = { NULL, };
volatile long gcnt;
#define N_GUARD 24

static BOOL mt_exec_dummy(MULTI_THREAD_FUNC fn, void *p1, void *p2);
static void mt_proc_copy(int id, int num, void *para1, void *para2);
static void mt_proc_nlm(int id, int num, void *para1, void *para2);
static int alloc_wmem(work_mem *wm, int h, int w);
static int is_sse2_available(void);

BOOL func_WndProcSse2N099(HWND hwnd, UINT message, WPARAM wparam,
                  LPARAM lparam, void *editp, FILTER *fp);

//------------------------------------------------------------------------
//フィルタ構造体定義
//------------------------------------------------------------------------
#define TRACK_N 2
TCHAR *track_name[]   = { "範囲", "分散" };
int   track_default[] = { 1, 50 };
int   track_s[] = {  1,   0 };
int   track_e[] = { 16, 100 };

FILTER_DLL filter = {
    FILTER_FLAG_EX_INFORMATION, 0,0,
    "NLM テスト",
    TRACK_N, track_name, track_default, track_s, track_e,
    0, NULL, NULL,
    func_proc,
    NULL,
    func_exit,
    NULL,
    func_WndProcSse2N099,
    NULL, NULL, NULL, 0,
    "nlm_test by N099, ver0.1",
    NULL, NULL,

};

//------------------------------------------------------------------------
//     フィルタ構造体のポインタを渡す関数
//------------------------------------------------------------------------
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTableSse2N099( void )
{
    //SSE2が必須
    if(!is_sse2_available()){
        filter.func_proc = NULL;
    }
    return &filter;
}

//------------------------------------------------------------------------
// 終了時に呼ばれる関数
//------------------------------------------------------------------------
BOOL func_exit(FILTER *fp)
{
    if(w_mem.p_head != NULL){
        VirtualFree(w_mem.p_head, 0, MEM_RELEASE);
        ZeroMemory(&w_mem, sizeof(w_mem));
    }

    return TRUE;
}

//------------------------------------------------------------------------
// フィルタ処理関数
//------------------------------------------------------------------------
BOOL func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{
    BOOL (*exec_mt)(MULTI_THREAD_FUNC, void*, void*) = NULL;
    
    //作業領域の確保
    if(alloc_wmem(&w_mem, fpip->max_h, fpip->max_w)){
        return FALSE;
    }
    
    //マルチスレッド関数が利用できる場合それを使う
    exec_mt = fp->exfunc->exec_multi_thread_func;
    if(exec_mt == NULL) exec_mt = mt_exec_dummy;
    
    //一旦コピー（コピーはMT化しない方が速いかも）
    _InterlockedExchange(&gcnt, 0);
    exec_mt(mt_proc_copy, fp, fpip);
    
    //フィルタ
    _InterlockedExchange(&gcnt, 0);
    exec_mt(mt_proc_nlm, fp, fpip);

    return TRUE;
}

//------------------------------------------------------------------------
// マルチスレッド処理関数
//------------------------------------------------------------------------
BOOL mt_exec_dummy(MULTI_THREAD_FUNC fn, void *p1, void *p2)
{
    fn(0, 1, p1, p2);
    return TRUE;
}

// ガウス関数を多項式 2x^3 - 3x^2 + 1 (0 : x>1.66) で近似
// ただし入力を適当にスケールして、ガウス関数に近づけてます
#define POLY_SCALE 0.6f
#define POLY_3 (2.0f * POLY_SCALE * POLY_SCALE * POLY_SCALE)
#define POLY_2 (-3.0f * POLY_SCALE * POLY_SCALE)
#define POLY_0 (1.0f / POLY_SCALE)

void mt_proc_nlm(int id, int num, void *para1, void *para2)
{
    FILTER *fp = (FILTER *)para1;
    FILTER_PROC_INFO *fpip = (FILTER_PROC_INFO *)para2;
    const __m64 *work = (const __m64*)w_mem.p_work;
    PIXEL_YC *dst = fpip->ycp_edit;
    ULONG i,j;
    LONG p,q;
    double sig;
    __m128 xsig2;
    const ULONG h = fpip->h;
    const ULONG w = fpip->w;
    const ULONG ws = fpip->max_w;
    const ULONG ww = w_mem.ww;
    const LONG wnd = fp->track[0];
    static const __m128 xpoly3 = { POLY_3, POLY_3, POLY_3, POLY_3 };
    static const __m128 xpoly2 = { POLY_2, POLY_2, POLY_2, POLY_2 };
    static const __m128 xpoly1 = {   1.0f,   1.0f,   1.0f,   1.0f };
    static const __m128 xpoly0 = { POLY_0, POLY_0, POLY_0, POLY_0 };

    _mm_empty();
    
    sig = pow(10.0, fp->track[2] / 25.0);
    sig = 4.0 / (sig * sig); //桁あふれ防止のため2bitシフト
    xsig2 = _mm_set1_ps((float)sig);

    //ガウス関数でなくてもう少し簡単な関数にすればまだ速くなるはず
    //桁あふれ防止を省略するともう少し速くなる（が危険かも）
    
    while((i = _InterlockedExchangeAdd(&gcnt, 1)) < h){
        const __m64 *px = work + i * ww;
        PIXEL_YC *pp = dst + i * ws;

        //同時に2pixel=(6+2)要素処理を行う
        for(j=0; j<w; j+=2){
            __m128i xc0,xc1;
            __m128 xwgt0, xwgt1, xcol0, xcol1;
            xwgt0 = _mm_setzero_ps();
            xwgt1 = _mm_setzero_ps();
            xcol0 = _mm_setzero_ps();
            xcol1= _mm_setzero_ps();

            //中心画素の周り"wnd"ピクセルの重み付け和を計算
            for(p = -wnd; p <= wnd; ++p){
                const __m64 *py = px + p * ww - wnd;
                for(q = -wnd; q <= wnd; ++q){
                    __m128i xssd0,xssd1,x0,x1,x2;
                    __m128 xf0,xf1,xf2,xf3,xfm0,xfm1;

                    //色距離を3x3ブロックのSSDにより求める
                    //ここでは3*3個分をアンロールし、桁あふれ対策で2bit右シフト
                    //（本当は4bitシフトすると確実、実はシフトなしでも大体は問題なし）
                    
                    x0 = _mm_subs_epi16(_mm_loadu_si128((__m128i*)(px - ww - 1)),
                                        _mm_loadu_si128((__m128i*)(py - ww - 1)) );
                    x1 = _mm_subs_epi16(_mm_load_si128((__m128i*)(px - ww)),
                                        _mm_loadu_si128((__m128i*)(py - ww)) );
                    x2 = _mm_unpackhi_epi16(x1, x0);
                    x1 = _mm_unpacklo_epi16(x1, x0);
                    xssd1 = _mm_srli_epi32(_mm_madd_epi16(x2, x2), 2);
                    xssd0 = _mm_srli_epi32(_mm_madd_epi16(x1, x1), 2);
                    
                    x0 = _mm_subs_epi16(_mm_loadu_si128((__m128i*)(px - ww + 1)),
                                        _mm_loadu_si128((__m128i*)(py - ww + 1)) );
                    x1 = _mm_subs_epi16(_mm_loadu_si128((__m128i*)(px - 1)),
                                        _mm_loadu_si128((__m128i*)(py - 1)) );
                    x2 = _mm_unpackhi_epi16(x1, x0);
                    x1 = _mm_unpacklo_epi16(x1, x0);
                    x2 = _mm_madd_epi16(x2, x2);
                    x1 = _mm_madd_epi16(x1, x1);
                    xssd1 = _mm_add_epi32(xssd1, _mm_srli_epi32(x2, 2));
                    xssd0 = _mm_add_epi32(xssd0, _mm_srli_epi32(x1, 2));
                    
                    x0 = _mm_subs_epi16(_mm_load_si128((__m128i*)px),
                                        _mm_loadu_si128((__m128i*)py));
                    x1 = _mm_subs_epi16(_mm_loadu_si128((__m128i*)(px + 1)),
                                        _mm_loadu_si128((__m128i*)(py + 1)) );
                    x2 = _mm_unpackhi_epi16(x1, x0);
                    x1 = _mm_unpacklo_epi16(x1, x0);
                    x2 = _mm_madd_epi16(x2, x2);
                    x1 = _mm_madd_epi16(x1, x1);
                    xssd1 = _mm_add_epi32(xssd1, _mm_srli_epi32(x2, 2));
                    xssd0 = _mm_add_epi32(xssd0, _mm_srli_epi32(x1, 2));
                    
                    x0 = _mm_subs_epi16(_mm_loadu_si128((__m128i*)(px + ww - 1)),
                                        _mm_loadu_si128((__m128i*)(py + ww - 1)) );
                    x1 = _mm_subs_epi16(_mm_load_si128((__m128i*)(px + ww)),
                                        _mm_loadu_si128((__m128i*)(py + ww)) );
                    x2 = _mm_unpackhi_epi16(x1, x0);
                    x1 = _mm_unpacklo_epi16(x1, x0);
                    x2 = _mm_madd_epi16(x2, x2);
                    x1 = _mm_madd_epi16(x1, x1);
                    xssd1 = _mm_add_epi32(xssd1, _mm_srli_epi32(x2, 2));
                    xssd0 = _mm_add_epi32(xssd0, _mm_srli_epi32(x1, 2));
                    
                    x0 = _mm_subs_epi16(_mm_loadu_si128((__m128i*)(px + ww + 1)),
                                        _mm_loadu_si128((__m128i*)(py + ww + 1)) );
                    x1 = _mm_setzero_si128();
                    x2 = _mm_unpackhi_epi16(x1, x0);
                    x1 = _mm_unpacklo_epi16(x1, x0);
                    x2 = _mm_madd_epi16(x2, x2);
                    x1 = _mm_madd_epi16(x1, x1);
                    xssd1 = _mm_add_epi32(xssd1, _mm_srli_epi32(x2, 2));
                    xssd0 = _mm_add_epi32(xssd0, _mm_srli_epi32(x1, 2));

                    //9要素分のSSDが求まったので、近似式を利用して重みを求める
                    //ここからは単精度浮動小数で計算
                    
                    xf1 = _mm_cvtepi32_ps(xssd1);
                    xf0 = _mm_cvtepi32_ps(xssd0);
                    
                    xf1 = _mm_mul_ps(xf1, xsig2);
                    xf0 = _mm_mul_ps(xf0, xsig2);

                    //"x^2"の平方根を求め"x"を得る
                    //SSDが桁あふれしていないこと前提
                    xfm1 = _mm_sqrt_ps(xf1);
                    xfm0 = _mm_sqrt_ps(xf0);
                    
                    xf3 = xf1;
                    xf2 = xf0;

                    //-3x^2
                    xf3 = _mm_mul_ps(xf3, xpoly2);
                    xf2 = _mm_mul_ps(xf2, xpoly2);

                    //+1
                    xf3 = _mm_add_ps(xf3, xpoly1);
                    xf2 = _mm_add_ps(xf2, xpoly1);

                    //x^3
                    xf1 = _mm_mul_ps(xf1, xfm1);
                    xf0 = _mm_mul_ps(xf0, xfm0);

                    //x >= 1/0.6 なら 0 にする
                    xfm1 = _mm_cmple_ps(xfm1, xpoly0);
                    xfm0 = _mm_cmple_ps(xfm0, xpoly0);

                    //2x^3
                    xf1 = _mm_mul_ps(xf1, xpoly3);
                    xf0 = _mm_mul_ps(xf0, xpoly3);
                    
                    xf1 = _mm_add_ps(xf1, xf3);
                    xf0 = _mm_add_ps(xf0, xf2);
                    
                    xf1 = _mm_and_ps(xf1, xfm1);
                    xf0 = _mm_and_ps(xf0, xfm0);

                    //中心画素を再ロード
                    x1 = x0 = _mm_loadu_si128((__m128i*)py++);
                    x1 = _mm_unpackhi_epi16(x1, x1);
                    x0 = _mm_unpacklo_epi16(x0, x0);
                    x1 = _mm_srai_epi32(x1, 16);
                    x0 = _mm_srai_epi32(x0, 16);
                    
                    xf3 = _mm_cvtepi32_ps(x1);
                    xf2 = _mm_cvtepi32_ps(x0);

                    //重み
                    xwgt1 = _mm_add_ps(xwgt1, xf1);
                    xwgt0 = _mm_add_ps(xwgt0, xf0);
                    
                    xf3 = _mm_mul_ps(xf3, xf1);
                    xf2 = _mm_mul_ps(xf2, xf0);

                    //重み付け和
                    xcol1 = _mm_add_ps(xcol1, xf3);
                    xcol0 = _mm_add_ps(xcol0, xf2);
                }
            }
            
            //全重みの和で、重み付け和を正規化する
            xwgt0 = _mm_rcp_ps(xwgt0);
            xwgt1 = _mm_rcp_ps(xwgt1);
            xcol0 = _mm_mul_ps(xcol0, xwgt0);
            xcol1 = _mm_mul_ps(xcol1, xwgt1);
            
            xc0 = _mm_cvtps_epi32(xcol0);
            xc1 = _mm_cvtps_epi32(xcol1);
            
            xc0 = _mm_packs_epi32(xc0, xc0);
            xc1 = _mm_packs_epi32(xc1, xc1);
            
            _mm_storel_epi64((__m128i*)pp++, xc0);
            _mm_storel_epi64((__m128i*)pp++, xc1);
            px += 2;
        }
    }
}

void mt_proc_copy(int id, int num, void *para1, void *para2)
{
    FILTER_PROC_INFO *fpip = (FILTER_PROC_INFO *)para2;
    __m64 *work = (__m64*)w_mem.p_head;
    const PIXEL_YC *src = fpip->ycp_edit;
    ULONG i,j;
    __m64 mtmp;
    const ULONG h = fpip->h;
    const ULONG w = fpip->w;
    const ULONG ws = fpip->max_w;
    const ULONG ww = w_mem.ww;
    
    while((i = _InterlockedExchangeAdd(&gcnt, 1)) < h){
        __m64 *pm,*pn;
        const PIXEL_YC *pp = src + i * ws;
        pm = pn = work + (i + N_GUARD) * ww;
        
        //一時領域にコピーする（左右24ピクセル分もコピー）
        mtmp = *(__m64*)pp;
        for(j=0; j<N_GUARD; ++j) *pm++ = mtmp;
        for(j=0; j<w; ++j){
            mtmp = *(__m64*)pp++;
            *pm++ = mtmp;
        }
        for(j=0; j<N_GUARD; ++j) *pm++ = mtmp;
        
        //上端と下端は24ライン分コピーする
        if(i == 0){
            for(j=0; j<N_GUARD; ++j){
                CopyMemory(work + j * ww, pn, (w + N_GUARD * 2) * sizeof(__m64));
            }
        }else if(i == h-1){
            for(j=0; j<N_GUARD; ++j){
                CopyMemory(pn + j * ww, pn, (w + N_GUARD * 2) * sizeof(__m64));
            }
        }
    }
    
    _mm_empty();
}

//------------------------------------------------------------------------
// 設定ウィンドウにウィンドウメッセージが来た時に呼ばれる関数
//------------------------------------------------------------------------
BOOL func_WndProcSse2N099(HWND hwnd, UINT message, WPARAM wparam,
                  LPARAM lparam, void *editp, FILTER *fp)
{
    switch(message){
    case WM_FILTER_CHANGE_ACTIVE:
        if(!fp->exfunc->is_filter_active(fp)){
            //リソースを開放
            func_exit(NULL);
        }
        break;
    default:
        break;
    }

    return FALSE;
}

//------------------------------------------------------------------------
// 処理領域を確保
//------------------------------------------------------------------------
int alloc_wmem(work_mem *wm, int h, int w)
{
    size_t sz;
    
    // 最大サイズは変化しないはずなので一度確保すればOK
    if(wm->p_head){
        return 0;
    }
    
    // 一時領域の確保
    // h,wは2の倍数であることを仮定
    // 上下左右に24ピクセル分余裕を持たせる
    h += N_GUARD * 2;
    w += N_GUARD * 2;
    sz = h * w * sizeof(__m64);
    wm->p_head = VirtualAlloc(NULL, sz, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == wm->p_head){
        return -1;
    }
    
    wm->p_work = wm->p_head + N_GUARD * w + N_GUARD;
    wm->ww = w;
    
    return 0;
}

//------------------------------------------------------------------------
// SSE2に対応しているかチェック
//------------------------------------------------------------------------
int is_sse2_available(void)
{
    long r_edx;

    // CPUIDによるチェック
    __asm {
        mov eax, 1
        cpuid
        mov r_edx, edx
    }

    // MMX  0x0800000 edx
    // SSE  0x2000000 edx
    // SSE2 0x4000000 edx
    if((r_edx & 0x4000000) == 0)
        return 0;

    // OSがサポートしているか
    __try {
        __asm xorps xmm0, xmm0
    }

    __except(EXCEPTION_EXECUTE_HANDLER){
        if(_exception_code()==STATUS_ILLEGAL_INSTRUCTION)
            return 0;
    }
    
    return 1;
}

