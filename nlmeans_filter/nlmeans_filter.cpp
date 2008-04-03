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

//#include "stdafx.h"
//#include <omp.h>	//インクルードするとAviUtlから認識されなくなる。関数のエクスポート周りの問題？TAGETVERの問題？
#include <cmath>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <atlbase.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <d3d9types.h>

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
//		サンプルインターレース解除プラグイン  for AviUtl ver0.98以降
//---------------------------------------------------------------------
#include	"filter.h"


//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define	TRACK_N	2									//	トラックバーの数
TCHAR	*track_name[] =		{"範囲", "分散"};	//	トラックバーの名前
int		track_default[] =	{3, 50};	//	トラックバーの初期値
int		track_s[] =			{1, 0,};	//	トラックバーの下限値
int		track_e[] =			{16, 100,};	//	トラックバーの上限値
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
	"NL-Meansフィルタ version 0.03 by nod_chip",
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

BOOL filterByCpu(FILTER *fp,FILTER_PROC_INFO *fpip);
BOOL filterByGpu(FILTER *fp,FILTER_PROC_INFO *fpip);
bool createInstance();
bool releaseInstance();

bool gpuInstanceInitialized = false;

static BOOL func_proc( FILTER *fp,FILTER_PROC_INFO *fpip )
{
	if (gpuInstanceInitialized && fp->check[0] && filterByGpu(fp, fpip)){
		return TRUE;
	}

	return filterByCpu(fp, fpip);
}

static BOOL func_init(FILTER *fp)
{
	createInstance();
	return TRUE;
}

static BOOL func_exit(FILTER *fp)
{
	releaseInstance();
	return TRUE;
}


//---------------------------------------------------------------------
//		CPUによる演算ルーチン
//---------------------------------------------------------------------
int get(FILTER_PROC_INFO *fpip, int x, int y, int channel);
void set(FILTER_PROC_INFO *fpip, int x, int y, int channel, int value);
BOOL filterByCpu(FILTER *fp,FILTER_PROC_INFO *fpip)
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
	const double H = pow(10, (double)fp->track[1] / 22.0);
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
						long long sum2 = 0;
						for (int dy2 = -neighborhoodRadius; dy2 <= neighborhoodRadius; ++dy2){
							for (int dx2 = -neighborhoodRadius; dx2 <= neighborhoodRadius; ++dx2){
								const int diff = get(fpip, x + dx2, y + dy2, channel) - get(fpip, x + dx + dx2, y + dy + dy2, channel);
								sum2 += diff * diff;
							}
						}
						const double w = exp(-sum2 * H2);
						sum += w;
						value += get(fpip, x + dx, y + dy, channel) * w;
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

//---------------------------------------------------------------------
//		GPUによる演算ルーチン
//---------------------------------------------------------------------
static const char* PIXEL_SHADER =
"sampler2D textureSampler;\n"
"float2 textureSizeInverse;\n"
"float H2;\n"
"static const int searchRadius = SEARCH_RADIUS;\n"
"static const float2 DELTA[9] = {\n"
"	float2(-1, -1),\n"
"	float2( 0, -1),\n"
"	float2( 1, -1),\n"
"	float2(-1,  0),\n"
"	float2( 0,  0),\n"
"	float2( 1,  0),\n"
"	float2(-1,  1),\n"
"	float2( 0,  1),\n"
"	float2( 1,  1)\n"
"};\n"
"\n"
"float4 process(float2 pos : VPOS) : COLOR0\n"
"{\n"
"	//あらかじめ摂動を入れてサンプルポイントが画素の中央を指すようにする\n"
"	pos += 0.25;\n"
"	pos *= textureSizeInverse;\n"
"\n"
"	const float2 delta[9] = {\n"
"		DELTA[0] * textureSizeInverse,\n"
"		DELTA[1] * textureSizeInverse,\n"
"		DELTA[2] * textureSizeInverse,\n"
"		DELTA[3] * textureSizeInverse,\n"
"		DELTA[4] * textureSizeInverse,\n"
"		DELTA[5] * textureSizeInverse,\n"
"		DELTA[6] * textureSizeInverse,\n"
"		DELTA[7] * textureSizeInverse,\n"
"		DELTA[8] * textureSizeInverse,\n"
"	};\n"
"\n"
"	const float3 currentPixels[9] = {\n"
"		tex2D(textureSampler, pos + delta[0]).xyz,\n"
"		tex2D(textureSampler, pos + delta[1]).xyz,\n"
"		tex2D(textureSampler, pos + delta[2]).xyz,\n"
"		tex2D(textureSampler, pos + delta[3]).xyz,\n"
"		tex2D(textureSampler, pos + delta[4]).xyz,\n"
"		tex2D(textureSampler, pos + delta[5]).xyz,\n"
"		tex2D(textureSampler, pos + delta[6]).xyz,\n"
"		tex2D(textureSampler, pos + delta[7]).xyz,\n"
"		tex2D(textureSampler, pos + delta[8]).xyz,\n"
"	};\n"
"\n"
"	float3 sum = 0;\n"
"	float3 value = 0;\n"
"	for (int dx = -searchRadius; dx <= searchRadius; ++dx) {\n"
"		for (int dy = -searchRadius; dy <= searchRadius; ++dy) {\n"
"			const float2 targetPos = pos + float2(dx, dy) * textureSizeInverse;\n"
"\n"
"			float3 sum2 = 0;\n"
"			float3 targetCenter;\n"
"			float3 diff;\n"
"			diff = currentPixels[0] - tex2D(textureSampler, targetPos + delta[0]);\n"
"			sum2 += diff * diff;\n"
"			diff = currentPixels[1] - tex2D(textureSampler, targetPos + delta[1]);\n"
"			sum2 += diff * diff;\n"
"			diff = currentPixels[2] - tex2D(textureSampler, targetPos + delta[2]);\n"
"			sum2 += diff * diff;\n"
"			diff = currentPixels[3] - tex2D(textureSampler, targetPos + delta[3]);\n"
"			sum2 += diff * diff;\n"
"			diff = currentPixels[4] - (targetCenter = tex2D(textureSampler, targetPos + delta[4]));\n"
"			sum2 += diff * diff;\n"
"			diff = currentPixels[5] - tex2D(textureSampler, targetPos + delta[5]);\n"
"			sum2 += diff * diff;\n"
"			diff = currentPixels[6] - tex2D(textureSampler, targetPos + delta[6]);\n"
"			sum2 += diff * diff;\n"
"			diff = currentPixels[7] - tex2D(textureSampler, targetPos + delta[7]);\n"
"			sum2 += diff * diff;\n"
"			diff = currentPixels[8] - tex2D(textureSampler, targetPos + delta[8]);\n"
"			sum2 += diff * diff;\n"
"\n"
"			const float3 w = exp(-sqrt(sum2) * H2);\n"
"			sum += w;\n"
"			value += targetCenter * w;\n"
"		}\n"
"	}\n"
"\n"
"	value /= sum;\n"
"\n"
"	return float4(value, 0);\n"
"};\n";

struct VERTEX
{
	D3DXVECTOR3 pos;
};

//VertexShaderを通らないようにする
static const D3DVERTEXELEMENT9 VERTEX_ELEMENTS[] = {
	{0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0},
	D3DDECL_END()
};

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc( hWnd, msg, wParam, lParam );
}

//DirectX/CPU-GPU間データ転送 - Satoshi OHSHIMA's web site
//http://aaa.jspeed.jp/~ohshima/cgi-bin/fswiki/wiki.cgi?page=DirectX%2FCPU-GPU%B4%D6%A5%C7%A1%BC%A5%BF%C5%BE%C1%F7

static const string SOFTWARE_NAME = "NL-Means filter";
WNDCLASSEX windowClass = {sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, SOFTWARE_NAME.c_str(), NULL};
HWND hWnd = NULL;
CComPtr<IDirect3D9> direct3D;
CComPtr<IDirect3DDevice9> device;
CComPtr<IDirect3DTexture9> memoryTexture;
CComPtr<IDirect3DTexture9> sourceTexture;
CComPtr<IDirect3DTexture9> destTexture;
CComPtr<IDirect3DSurface9> memorySurface;
CComPtr<IDirect3DSurface9> sourceSurface;
CComPtr<IDirect3DSurface9> destSurface;
CComPtr<ID3DXRenderToSurface> renderToSurface;
CComPtr<IDirect3DPixelShader9> pixelShader;
int searchRadiusOfCreatedPixelShader = 0;

bool checkAndRecreateTexture(int width, int height)
{
	bool recreate = false;
	if (memoryTexture == NULL){
		recreate = true;
	} else {
		D3DSURFACE_DESC desc;
		if (FAILED(memorySurface->GetDesc(&desc))){
			outputLogMessage("メモリテクスチャのサーフェス情報の取得に失敗しました");
			return false;
		}
		recreate = width != desc.Width || height != desc.Height;
	}

	if (!recreate){
		return true;
	}

	//テクスチャ解放
	renderToSurface = NULL;
	destSurface = NULL;
	sourceSurface = NULL;
	memorySurface = NULL;
	destTexture = NULL;
	sourceTexture = NULL;
	memoryTexture = NULL;

	//テクスチャ作成
	if (FAILED(D3DXCreateTexture(device, width, height, 1, D3DUSAGE_DYNAMIC , D3DFMT_A16B16G16R16, D3DPOOL_SYSTEMMEM, &memoryTexture))){
		releaseInstance();
		outputLogMessage("メモリテクスチャの作成に失敗しました");
		return false;
	}

	if (FAILED(D3DXCreateTexture(device, width, height, 1, 0, D3DFMT_A16B16G16R16, D3DPOOL_DEFAULT, &sourceTexture))){
		releaseInstance();
		outputLogMessage("処理元テクスチャの作成に失敗しました");
		return false;
	}

	if (FAILED(D3DXCreateTexture(device, width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16, D3DPOOL_DEFAULT, &destTexture))){
		releaseInstance();
		outputLogMessage("処理先テクスチャの作成に失敗しました");
		return false;
	}

	if (FAILED(memoryTexture->GetSurfaceLevel(0, &memorySurface))){
		releaseInstance();
		outputLogMessage("メモリテクスチャサーフィスの取得に失敗しました");
		return false;
	}

	if (FAILED(sourceTexture->GetSurfaceLevel(0, &sourceSurface))){
		releaseInstance();
		outputLogMessage("処理元テクスチャサーフィスの取得に失敗しました");
		return false;
	}

	if (FAILED(destTexture->GetSurfaceLevel(0, &destSurface))){
		releaseInstance();
		outputLogMessage("処理先テクスチャサーフィスの取得に失敗しました");
		return false;
	}

	if (FAILED(D3DXCreateRenderToSurface(device, width, height, D3DFMT_A16B16G16R16, FALSE, D3DFMT_UNKNOWN, &renderToSurface))){
		releaseInstance();
		outputLogMessage("レンダリングインタフェースの作成に失敗しました");
		return false;
	}

	return true;
}

bool createInstance()
{
	outputLogMessage("インスタンス作成開始");

	if (RegisterClassEx(&windowClass) == 0){
		releaseInstance();
		outputLogMessage("ウィンドウクラスの登録に失敗しました");
		return false;
	}

	hWnd = CreateWindow(SOFTWARE_NAME.c_str(), SOFTWARE_NAME.c_str(), WS_OVERLAPPEDWINDOW, 0, 0, CW_USEDEFAULT, CW_USEDEFAULT, GetDesktopWindow(), NULL, windowClass.hInstance, NULL);
	if (hWnd == NULL){
		releaseInstance();
		outputLogMessage("ウィンドウの作成に失敗しました");
		return false;
	}

	direct3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (direct3D == NULL){
		releaseInstance();
		outputLogMessage("Direct3Dのインスタンスの作成に失敗しました");
		return false;
	}

	D3DCAPS9 caps;
	if (FAILED(direct3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps))){
		releaseInstance();
		outputLogMessage("デバイス能力の取得に失敗しました");
		return false;
	}

	D3DPRESENT_PARAMETERS presentParameters = {0};
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.Windowed = TRUE;
	presentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
	presentParameters.MultiSampleQuality = 0;

	if (FAILED(direct3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &presentParameters, &device))){
		releaseInstance();
		outputLogMessage("デバイスの作成に失敗しました");
		return false;
	}

	CComPtr<IDirect3DVertexDeclaration9> vertexDeclaration;
	if (FAILED(device->CreateVertexDeclaration(VERTEX_ELEMENTS, &vertexDeclaration))){
		releaseInstance();
		outputLogMessage("頂点宣言の作成に失敗しました");
		return false;
	}

	if (FAILED(device->SetVertexDeclaration(vertexDeclaration))){
		releaseInstance();
		outputLogMessage("頂点宣言の設定に失敗しました");
		return false;
	}

	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	gpuInstanceInitialized = true;

	return true;
}

bool releaseInstance()
{
	pixelShader = NULL;
	renderToSurface = NULL;
	destSurface = NULL;
	sourceSurface = NULL;
	memorySurface = NULL;
	destTexture = NULL;
	sourceTexture = NULL;
	memoryTexture = NULL;
	device = NULL;
	direct3D = NULL;

	if (hWnd){
		DestroyWindow(hWnd);
		hWnd = NULL;

		UnregisterClass(SOFTWARE_NAME.c_str(), windowClass.hInstance);
	}

	gpuInstanceInitialized = false;

	return true;
}

//シェーダプログラム本体
//SEARCH_RADIUS定数は必ず「定数」にしなければならない。
//さもないとコンパイルが通らない。
//C++のコードでSEARCH_RADIUSの値を設定している

BOOL filterByGpu(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	const int width = fpip->w;
	const int height = fpip->h;

	const int searchRadius = fp->track[0];
	if (searchRadius != searchRadiusOfCreatedPixelShader){
		//ピクセルシェーダをコンパイルし直す
		pixelShader = NULL;

		searchRadiusOfCreatedPixelShader = searchRadius;

		//シェーダプログラムの中のマクロ定義
		char digit[16];
		sprintf(digit, "%d", searchRadius);

		D3DXMACRO macros[2] = {0};
		macros[0].Name = "SEARCH_RADIUS";
		macros[0].Definition = digit;

		//コンパイル
		CComPtr<ID3DXBuffer> buffer;
		CComPtr<ID3DXBuffer> errorMessage;

		if (FAILED(D3DXCompileShader(PIXEL_SHADER, strlen(PIXEL_SHADER), macros, NULL, "process", "ps_3_0", D3DXSHADER_PREFER_FLOW_CONTROL, &buffer, &errorMessage, NULL))){
			releaseInstance();
			outputLogMessage("ピクセルシェーダのコンパイルに失敗しました");
			outputLogMessage((char*)errorMessage->GetBufferPointer());
			return FALSE;
		}

		//ピクセルシェーダの作成
		if (FAILED(device->CreatePixelShader((DWORD*)buffer->GetBufferPointer(), &pixelShader))){
			releaseInstance();
			outputLogMessage("ピクセルシェーダの作成に失敗しました");
			return FALSE;
		}

		//ピクセルシェーダの設定
		if (FAILED(device->SetPixelShader(pixelShader))){
			releaseInstance();
			outputLogMessage("ピクセルシェーダの設定に失敗しました");
			return false;
		}
	}

	//テクスチャチェック
	if (!checkAndRecreateTexture(width, height)){
		releaseInstance();
		outputLogMessage("テクスチャの作成に失敗しました");
		return FALSE;
	}

	//画像データをGPUに転送する
	{
		D3DLOCKED_RECT lockedRect = {0};
		if (FAILED(memorySurface->LockRect(&lockedRect, NULL, D3DLOCK_DISCARD))){
			releaseInstance();
			outputLogMessage("メモリサーフィスのロックに失敗しました");
			return FALSE;
		}

		const char* dest = (char*)lockedRect.pBits;
		int y;
#pragma omp parallel for
		for (y = 0; y < height; ++y){
			for (int x = 0; x < width; ++x){
				unsigned short* destBeggining = (unsigned short*)&dest[x * 2 * 4 + y * lockedRect.Pitch];
				const short* sourcePixel = (short*)(&fpip->ycp_edit[x + y * fpip->max_w]);
				//入力データが範囲を超えている場合があるので
				//符号なし16bit整数の0x4000～0xbfffにマッピングする
				destBeggining[0] = ((int)sourcePixel[0] + 2048) << 3;
				destBeggining[1] = (((int)sourcePixel[1]) + 4096) << 3;
				destBeggining[2] = (((int)sourcePixel[2]) + 4096) << 3;
			}
		}

		if (FAILED(memorySurface->UnlockRect())){
			releaseInstance();
			outputLogMessage("メモリサーフィスのロック解除に失敗しました");
			return FALSE;
		}

		//D3DXSaveSurfaceToFile("beforeMemory.bmp", D3DXIFF_BMP, memorySurface, NULL, NULL);

		//メモリからGPUに転送開始
		if (FAILED(device->UpdateSurface(memorySurface, NULL, sourceSurface, NULL))){
			releaseInstance();
			outputLogMessage("メモリサーフィスから処理元サーフィスへのデータ転送に失敗しました");
			return FALSE;
		}

		//D3DXSaveSurfaceToFile("beforeSource.bmp", D3DXIFF_BMP, sourceSurface, NULL, NULL);
	}

	//レンダリング開始
	D3DVIEWPORT9 viewPort;
	viewPort.X = 0;
	viewPort.Y = 0;
	viewPort.Width = width;
	viewPort.Height = height;
	viewPort.MinZ = 0.0f;
	viewPort.MaxZ = 0.0;

	const double H = pow(10, (double)(fp->track[1]-100) / 55.0);
	const double H2 = 1.0 / (H * H);

	D3DXVECTOR4 pixelShaderConstant[2];
	memset(pixelShaderConstant, 0, sizeof(pixelShaderConstant));
	pixelShaderConstant[0].x = 1.0f / (float)width;
	pixelShaderConstant[0].y = 1.0f / (float)height;
	pixelShaderConstant[1].x = (float)H2;

	if (FAILED(renderToSurface->BeginScene(destSurface, &viewPort))){
		releaseInstance();
		outputLogMessage("描画の開始に失敗しました");
		return FALSE;
	}

	if (FAILED(device->SetTexture(0, sourceTexture))){
		releaseInstance();
		outputLogMessage("テクスチャの設定に失敗しました");
		return false;
	}

	if (FAILED(device->SetPixelShaderConstantF(0, (float*)pixelShaderConstant, 2))){
		releaseInstance();
		outputLogMessage("ピクセルシェーダ定数の設定に失敗しました");
		return FALSE;
	}

	//一部のGeForce環境で画面の右上しか表示されない不具合があるため
	//座標が負の領域も十分にカバーする
	const VERTEX polygon[4] = {
		{D3DXVECTOR3(-width, -height, 0)},
		{D3DXVECTOR3(-width, height, 0)},
		{D3DXVECTOR3(width, -width, 0)},
		{D3DXVECTOR3(width, height, 0)},
	};

	if (FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, polygon, 12))){
		releaseInstance();
		outputLogMessage("プリミティブの描画に失敗しました");
		return FALSE;
	}

	if (FAILED(renderToSurface->EndScene(D3DX_FILTER_NONE))){
		releaseInstance();
		outputLogMessage("描画の終了に失敗しました");
		return FALSE;
	}

	//画像データをVRAMから取得する
	{
		//D3DXSaveSurfaceToFile("afterDest.bmp", D3DXIFF_BMP, destSurface, NULL, NULL);

		//GPUからメモリに転送開始
		if (FAILED(device->GetRenderTargetData(destSurface, memorySurface))){
			releaseInstance();
			outputLogMessage("処理先サーフィスからメモリサーフィスへのデータ転送に失敗しました");
			return FALSE;
		}

		//D3DXSaveSurfaceToFile("afterMemory.bmp", D3DXIFF_BMP, memorySurface, NULL, NULL);

		//メモリテクスチャからaviutlに書き戻す
		D3DLOCKED_RECT lockedRect;
		if (FAILED(memorySurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY))){
			releaseInstance();
			outputLogMessage("描画先テクスチャのロックに失敗しました");
			return FALSE;
		}

		const char* source = (char*)lockedRect.pBits;
		int y;
#pragma omp parallel for
		for (y = 0; y < height; ++y){
			for (int x = 0; x < width; ++x){
				const unsigned short* sourceBeggining = (const unsigned short*)&source[x * 2 * 4 + y * lockedRect.Pitch];
				short* destPixel = (short*)(&fpip->ycp_edit[x + y * fpip->max_w]);
				destPixel[0] = (short)((((int)sourceBeggining[0]) >> 3) - 2048);
				destPixel[1] = (short)((((int)sourceBeggining[1]) >> 3) - 4096);
				destPixel[2] = (short)((((int)sourceBeggining[2]) >> 3) - 4096);
			}
		}

		if (FAILED(memorySurface->UnlockRect())){
			releaseInstance();
			outputLogMessage("描画先テクスチャのロック解除に失敗しました");
			return FALSE;
		}
	}

	return TRUE;
}
