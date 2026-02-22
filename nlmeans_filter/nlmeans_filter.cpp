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
//#include <omp.h>	//インクルードするとAviUtlから認識されなくなる。関数のエクスポート周りの問題？TAGETVERの問題？
#include <cmath>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <atlbase.h>
#include <d3d9.h>
#include <d3d9types.h>
#include "ProcessorCpu.h"
#include "ProcessorAvx2.h"
#include "ProcessorGpu.h"

//---------------------------------------------------------------------
//		サンプルインターレース解除プラグイン  for AviUtl ver0.98以降
//---------------------------------------------------------------------
#include "filter.hpp"

using namespace std;


//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define	TRACK_N	5						//	トラックバーの数
TCHAR	*track_name[] =		{
	const_cast<TCHAR*>("空間範囲"),
	const_cast<TCHAR*>("時間範囲"),
	const_cast<TCHAR*>("分散"),
	const_cast<TCHAR*>("計算モード"),
	const_cast<TCHAR*>("GPUアダプタ")
};	//	トラックバーの名前
int		track_default[] =	{3, 0, 50, 2, 0};	//	トラックバーの初期値
int		track_s[] =			{1, 0, 0, 0, 0};	//	トラックバーの下限値
int		track_e[] =			{16, 7, 100, 2, 8};	//	トラックバーの上限値
#define	CHECK_N	0														//	チェックボックスの数
TCHAR	*check_name[] = 	{const_cast<TCHAR*>("ダミー")};				//	チェックボックスの名前
int		check_default[] = 	{0};				//	チェックボックスの初期値 (値は0か1)

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0,0,
	const_cast<TCHAR*>("NL-Meansフィルタ"),
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
	const_cast<TCHAR*>("NL-Meansフィルタ version 0.14 by nodchip"),
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
std::shared_ptr<ProcessorCpu> processorCpu;
std::shared_ptr<ProcessorAvx2> processorAvx2;
std::shared_ptr<ProcessorGpu> processorGpu;
static const int NUMBER_OF_ROUTINES = 3;
std::shared_ptr<Processor> processors[NUMBER_OF_ROUTINES];
std::shared_ptr<Processor> currentProcessor;

static BOOL func_proc( FILTER *fp,FILTER_PROC_INFO *fpip )
{
	int calculationMode = fp->track[3];
	if (processorGpu != NULL){
		// 0 は自動選択、1 以上は DXGI のアダプタ番号(1始まり)。
		const int requestedAdapter = fp->track[4] - 1;
		processorGpu->setPreferredAdapterIndex(requestedAdapter);
	}

	while (!processors[calculationMode]->isPrepared()){
		--calculationMode;
	}
	currentProcessor = processors[calculationMode];

	return currentProcessor->proc(*fp, *fpip);
}

static BOOL func_init(FILTER *fp)
{
	processorCpu = std::shared_ptr<ProcessorCpu>(new ProcessorCpu());
	processorAvx2 = std::shared_ptr<ProcessorAvx2>(new ProcessorAvx2());
	processorGpu = std::shared_ptr<ProcessorGpu>(new ProcessorGpu());

	processors[0] = processorCpu;
	processors[1] = processorAvx2;
	processors[2] = processorGpu;

	return TRUE;
}

static BOOL func_exit(FILTER *fp)
{
	processorGpu.reset();
	processorAvx2.reset();
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
