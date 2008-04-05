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

//---------------------------------------------------------------------
//		サンプルインターレース解除プラグイン  for AviUtl ver0.98以降
//---------------------------------------------------------------------
#include "filter.hpp"

using namespace std;

//---------------------------------------------------------------------
//		デバッグ出力用ルーチン
//---------------------------------------------------------------------
static const string LOG_FILE_NAME = "nlmeans_log.txt";
ofstream ofs(LOG_FILE_NAME.c_str(), ios_base::out | ios_base::app);
void outputLogMessage(const string& message)
{
	char buffer[1024];
	time_t t = time(NULL);
	strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S ", localtime(&t));
	ofs << buffer << message << endl;
}



//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define	TRACK_N	3								//	トラックバーの数
TCHAR	*track_name[] =		{"空間範囲", "時間範囲", "分散"};	//	トラックバーの名前
int		track_default[] =	{3, 0, 50};	//	トラックバーの初期値
int		track_s[] =			{1, 0, 0,};	//	トラックバーの下限値
int		track_e[] =			{16, 7, 100,};	//	トラックバーの上限値
#define	CHECK_N	1														//	チェックボックスの数
TCHAR	*check_name[] = 	{"可能な場合はGPUによる計算を行う"};				//	チェックボックスの名前
int		check_default[] = 	{1};				//	チェックボックスの初期値 (値は0か1)

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
	NULL,
	NULL,NULL,
	NULL,
	NULL,
	"NL-Meansフィルタ version 0.05 by nod_chip",
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

static BOOL func_proc( FILTER *fp,FILTER_PROC_INFO *fpip )
{
	const int useGpu = fp->check[0];
	const bool gpuPrepared = processorGpu->isPrepared();

	if (useGpu && gpuPrepared && processorGpu->proc(*fp, *fpip)){
		return TRUE;
	}

	return processorCpu->proc(*fp, *fpip);
}

static BOOL func_init(FILTER *fp)
{
	processorCpu = boost::shared_ptr<ProcessorCpu>(new ProcessorCpu());
	processorGpu = boost::shared_ptr<ProcessorGpu>(new ProcessorGpu());
	return TRUE;
}

static BOOL func_exit(FILTER *fp)
{
	processorGpu.reset();
	processorCpu.reset();

	return TRUE;
}
