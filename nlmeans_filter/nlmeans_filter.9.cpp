// Copyright 2008 nod_chip
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
//#include <omp.h>	//インクルードするとAviUtlから認識されなくなる。関数のエクスポート周りの問題？TAGETVERの問題？
#include <cmath>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include <atlbase.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <d3d9types.h>
#include "ProcessorCpu.h"
#include "ProcessorGpu.h"
#include "ProcessorSse2N099.h"
#include "ProcessorSse2Aroo.h"
#include "ProcessorCuda.h"

//---------------------------------------------------------------------
//		サンプルインターレース解除プラグイン  for AviUtl ver0.98以降
//---------------------------------------------------------------------
#include "filter.hpp"

using namespace std;


//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define	TRACK_N	4						//	トラックバーの数
TCHAR	*track_name[] =		{"空間範囲", "時間範囲", "分散", "CPUモード"};	//	トラックバーの名前
int		track_default[] =	{3, 0, 50, 2};	//	トラックバーの初期値
int		track_s[] =			{1, 0, 0, 0};	//	トラックバーの下限値
int		track_e[] =			{16, 7, 100, 2};	//	トラックバーの上限値
#define	CHECK_N	2														//	チェックボックスの数
TCHAR	*check_name[] = 	{"可能な場合はGPUを使用する", "可能な場合は複数GPUを使用する"};				//	チェックボックスの名前
int		check_default[] = 	{1, 0};				//	チェックボックスの初期値 (値は0か1)

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0,0,
	"NL-Meansフィルタ",
	TRACK_N,track_name,track_default,
	track_s,track_e,
	CHECK_N,check_name,check_default,
	func_proc,
	func_init,
	func_exit,
	NULL,
	func_WndProc,
	NULL,NULL,
	NULL,
	NULL,
	"NL-Meansフィルタ version 0.10 by nod_chip",
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
boost::shared_ptr<ProcessorCpu> processorCpu;
boost::shared_ptr<ProcessorGpu> processorGpu;
boost::shared_ptr<ProcessorSse2N099> processorSse2N099;
boost::shared_ptr<ProcessorSse2Aroo> processorSse2Aroo;
//boost::shared_ptr<ProcessorCuda> processorCuda;
static const int NUMBER_OF_ROUTINES = 5;
boost::shared_ptr<Processor> processors[NUMBER_OF_ROUTINES];
boost::shared_ptr<Processor> currentProcessor;

static BOOL func_proc( FILTER *fp,FILTER_PROC_INFO *fpip )
{
	const int useGpu = fp->check[0];
	const int useCuda = fp->check[1];
	int routineIndex = fp->track[3];

	if (useGpu){
		routineIndex = NUMBER_OF_ROUTINES - 2;
	}

	//if (useCuda){
	//	routineIndex = NUMBER_OF_ROUTINES - 1;
	//}

	if (processors[routineIndex]->isPrepared()){
		currentProcessor = processors[routineIndex];
	} else {
		currentProcessor = processorCpu;
	}

	return currentProcessor->proc(*fp, *fpip);
}

static BOOL func_init(FILTER *fp)
{
	processorCpu = boost::shared_ptr<ProcessorCpu>(new ProcessorCpu());
	processorGpu = boost::shared_ptr<ProcessorGpu>(new ProcessorGpu());
	processorSse2N099 = boost::shared_ptr<ProcessorSse2N099>(new ProcessorSse2N099());
	processorSse2Aroo = boost::shared_ptr<ProcessorSse2Aroo>(new ProcessorSse2Aroo());
	//processorCuda = boost::shared_ptr<ProcessorCuda>(new ProcessorCuda());

	processors[0] = processorCpu;
	processors[1] = processorSse2N099;
	processors[2] = processorSse2Aroo;
	processors[3] = processorGpu;
	//processors[4] = processorCuda;

	return TRUE;
}

static BOOL func_exit(FILTER *fp)
{
	//processorCuda.reset();
	processorSse2Aroo.reset();
	processorSse2N099.reset();
	processorGpu.reset();
	processorCpu.reset();

	return TRUE;
}

//------------------------------------------------------------------------
// 設定ウィンドウにウィンドウメッセージが来た時に呼ばれる関数
//------------------------------------------------------------------------
BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam,
                  LPARAM lparam, void *editp, FILTER *fp)
{
	if (currentProcessor == NULL){
		return FALSE;
	}

	return currentProcessor->wndProc(hwnd, message, wparam, lparam, editp, fp);
}
