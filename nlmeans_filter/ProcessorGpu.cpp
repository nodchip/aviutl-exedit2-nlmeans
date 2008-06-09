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
#include <vector>
#include <set>
#include <boost/format.hpp>
#include <atlbase.h>
#include <afxwin.h>
#include <afxmt.h>
#include "ProcessorGpu.h"
#include "PixelShader.h"
#include "InputTexture.h"
#include "InputTextureCached.h"
#include "Cache.h"

using namespace std;

const char* ProcessorGpu::softwareName = "NL-Meansフィルタ";
const WNDCLASSEX ProcessorGpu::windowClass = {sizeof(WNDCLASSEX), CS_CLASSDC, msgProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, softwareName, NULL};
const D3DVERTEXELEMENT9 ProcessorGpu::VERTEX_ELEMENTS[] = {
	{0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0},
	D3DDECL_END()
};

struct VERTEX
{
	D3DXVECTOR3 pos;
};

struct TEXTURE_PIXEL
{
	unsigned short r, g, b, a;
};

static void Message(char* message)
{
	AfxMessageBox(message);
}

template<class T>
static void Message(char* message, T& arg0)
{
	AfxMessageBox((boost::format(message) % arg0).str().c_str());
}

LRESULT WINAPI ProcessorGpu::msgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc( hWnd, msg, wParam, lParam );
}

ProcessorGpu::ProcessorGpu() : prepared(false), hwnd(NULL), frameCacheSize(-1), baseFrameIndex(0), priorReadThreadStarted(false), eventPriorReadStopper(FALSE, TRUE), cache(new Cache<int, boost::shared_ptr<FRAME_DATA> >())
{
	create();
}

ProcessorGpu::~ProcessorGpu()
{
	release();
}

bool ProcessorGpu::create()
{
	if (RegisterClassEx(&windowClass) == 0){
		release();
		AfxMessageBox("ウィンドウクラスの設定に失敗しました");
		return false;
	}

	hwnd = CreateWindow(softwareName, softwareName, WS_OVERLAPPEDWINDOW, 0, 0, CW_USEDEFAULT, CW_USEDEFAULT, GetDesktopWindow(), NULL, windowClass.hInstance, NULL);
	if (hwnd == NULL){
		release();
		AfxMessageBox("ウィンドウの作成に失敗しました");
		return false;
	}

	direct3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (direct3D == NULL){
		release();
		AfxMessageBox("Direct3D9の作成に失敗しました");
		return false;
	}

	D3DPRESENT_PARAMETERS presentParameters = {0};
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.Windowed = TRUE;
	presentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
	presentParameters.MultiSampleQuality = 0;

	if (FAILED(direct3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &presentParameters, &device))){
		release();
		Message("デバイスの作成に失敗しました");
		return false;
	}

	CComPtr<IDirect3DVertexDeclaration9> vertexDeclaration;
	if (FAILED(device->CreateVertexDeclaration(VERTEX_ELEMENTS, &vertexDeclaration))){
		release();
		Message("頂点宣言の作成に失敗しました");
		return false;
	}

	if (FAILED(device->SetVertexDeclaration(vertexDeclaration))){
		release();
		Message("頂点宣言の設定に失敗しました");
		return false;
	}

	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	pixelShaderCreator = PixelShader::createInstance(device);
	inputTextureCreator = InputTexture::createInstance(device);

	prepared = true;

	return true;
}

bool ProcessorGpu::release()
{
	criticalSectionRelease.Lock();
	prepared = false;

	//先読みスレッドを終了させるために再開する
	eventPriorReadStopper.SetEvent();
	criticalSectionRelease.Unlock();

	//先読みスレッドを止めるための苦肉の策
	Sleep(100);

	inputTextureCreator.reset();
	pixelShaderCreator.reset();
	renderToSurface = NULL;
	hostSurface = NULL;
	hostTexture = NULL;
	device = NULL;
	direct3D = NULL;

	if (hwnd){
		DestroyWindow(hwnd);
		hwnd = NULL;

		UnregisterClass(softwareName, windowClass.hInstance);
	}

	return true;
}

BOOL ProcessorGpu::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	currentFp = &fp;
	currentFpip = &fpip;

	//AviUtlキャッシュの設定
	const int width = fpip.w;
	const int height = fpip.h;
	const int numberOfCaches = 1;
	if (frameCacheSize != numberOfCaches){
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, numberOfCaches, NULL);
		frameCacheSize = numberOfCaches;
	}

	//作業用テクスチャチェック
	if (!prepareTemporaryArea(fpip)){
		release();
		AfxMessageBox("作業用テクスチャの作成に失敗しました");
		return FALSE;
	}

	//テクスチャキャッシュの設定
	const int timeSearchRadius = fp.track[1];
	boost::dynamic_pointer_cast<InputTextureCached>(inputTextureCreator)->setMaxNumberOfCache(timeSearchRadius * 2 + 2);

	//処理結果キャッシュの設定
	const int priorReadSize = fp.track[4];
	cache->setMaxCacheSize(priorReadSize);

	const int currentFrameIndex = fp.exfunc->get_frame(fpip.editp);
	boost::shared_ptr<vector<PIXEL_YC> > frame = getFilteredFrame(fp, fpip, currentFrameIndex);

	if (frame->empty()){
		return FALSE;
	}

	memcpy(fpip.ycp_edit, &*frame->begin(), fpip.max_w * fpip.h * sizeof(PIXEL_YC));

	return TRUE;
}

boost::shared_ptr<std::vector<PIXEL_YC> > ProcessorGpu::getFilteredFrame(FILTER& fp, FILTER_PROC_INFO& fpip, int frameIndex)
{
	const bool priorReadThread = fp.check[1] != 0;
	if (priorReadThread){
		if (!priorReadThreadStarted){
			//先読みスレッド開始
			priorReadThreadStarted = true;
			AfxBeginThread(priorReadThreadProc, this);
		}

		criticalSectionCache.Lock();
		boost::shared_ptr<FRAME_DATA> frameData = cache->get(frameIndex);
		criticalSectionCache.Unlock();

		baseFrameIndex = frameIndex;
		eventPriorReadStopper.SetEvent();
		frameData->event->Lock();

		return frameData->frame;
	} else {
		boost::shared_ptr<vector<PIXEL_YC> > frame;
		createFilteredFrame(fp, fpip, frameIndex, frame);
		return frame;
	}
}

UINT ProcessorGpu::priorReadThreadProc(LPVOID param)
{
	ProcessorGpu& processor = *(ProcessorGpu*)param;
	return processor.priorRead(*processor.currentFp, *processor.currentFpip);
}

UINT ProcessorGpu::priorRead(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	eventPriorReadStopper.Lock();

	int processingFrameIndex = baseFrameIndex;
	while (prepared && priorReadThreadStarted){
		criticalSectionCache.Lock();
		const int targetFrameIndex = baseFrameIndex;
		if (!cache->contains(targetFrameIndex)){
			processingFrameIndex = baseFrameIndex;
		}

		boost::shared_ptr<FRAME_DATA> frameData = cache->get(processingFrameIndex);
		criticalSectionCache.Unlock();

		++processingFrameIndex;

		//キャッシュの中に入っているフレームの数の最大値がpriorReadSize
		const int priorReadSize = fp.track[4];

		//終了処理中にResetEvent()が呼び出されるとまずいのでクリティカルセクションとする
		criticalSectionRelease.Lock();
		if (processingFrameIndex - baseFrameIndex >= priorReadSize || processingFrameIndex >= fpip.frame_n){
			//先読みスレッドを一時停止する
			eventPriorReadStopper.ResetEvent();
		}
		if (!prepared || !priorReadThreadStarted){
			eventPriorReadStopper.SetEvent();
		}
		criticalSectionRelease.Unlock();

		eventPriorReadStopper.Lock();
	}

	return TRUE;
}

BOOL ProcessorGpu::createFilteredFrame(FILTER& fp, FILTER_PROC_INFO& fpip, int frameIndex, boost::shared_ptr<vector<PIXEL_YC> >& output)
{
	const int width = fpip.w;
	const int height = fpip.h;

	const int spaceSearchRadius = fp.track[0];
	const int timeSearchRadius = fp.track[1];

	//ピクセルシェーダ作成
	const CComPtr<IDirect3DPixelShader9> pixelShader = pixelShaderCreator->create(spaceSearchRadius, timeSearchRadius);
	if (pixelShader == NULL){
		release();
		AfxMessageBox("ピクセルシェーダの作成に失敗しました");
		return FALSE;
	}

	//ピクセルシェーダの設定
	if (FAILED(device->SetPixelShader(pixelShader))){
		release();
		AfxMessageBox("ピクセルシェーダの設定に失敗しました");
		return FALSE;
	}

	//レンダリング開始
	const double H = pow(10, (double)(fp.track[2]-100) / 55.0);
	const double H2 = 1.0 / (H * H);

	D3DXVECTOR4 pixelShaderConstant[2];
	memset(pixelShaderConstant, 0, sizeof(pixelShaderConstant));
	pixelShaderConstant[0].x = 1.0f / (float)width;
	pixelShaderConstant[0].y = 1.0f / (float)height;
	pixelShaderConstant[1].x = (float)H2;

	//テクスチャをセットする
	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		const CComPtr<IDirect3DTexture9> texture = inputTextureCreator->get(fp, fpip, frameIndex + dt, hostSurface);
		if (texture == NULL){
			release();
			AfxMessageBox("フレームテクスチャの作成に失敗しました");
			return FALSE;
		}

		if (FAILED(device->SetTexture(dt + timeSearchRadius, texture))){
			release();
			AfxMessageBox("テクスチャの設定に失敗しました");
			return FALSE;
		}
	}

	//シーンのレンダリング開始
	if (FAILED(device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 0, 0))){
		release();
		AfxMessageBox("フレームのクリアに失敗しました");
		return FALSE;
	}

	D3DVIEWPORT9 viewPort;
	viewPort.X = 0;
	viewPort.Y = 0;
	viewPort.Width = width;
	viewPort.Height = height;
	viewPort.MinZ = 0.0f;
	viewPort.MaxZ = 0.0;

	if (FAILED(renderToSurface->BeginScene(hostSurface, &viewPort))){
		release();
		AfxMessageBox("シーンの開始に失敗しました");
		return FALSE;
	}

	if (FAILED(device->SetPixelShaderConstantF(0, (float*)pixelShaderConstant, 2))){
		release();
		AfxMessageBox("ピクセルシェーダ定数の設定に失敗しました");
		return FALSE;
	}

	//一部のGeForce環境で画面の右上しか表示されない不具合があるため
	//座標が負の領域も十分にカバーする
	const VERTEX polygon[4] = {
		{D3DXVECTOR3((float)-width, (float)-height, 0)},
		{D3DXVECTOR3((float)-width, (float) height, 0)},
		{D3DXVECTOR3((float) width, (float)-height, 0)},
		{D3DXVECTOR3((float) width, (float) height, 0)},
	};

	if (FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, polygon, 12))){
		release();
		AfxMessageBox("プリミティブの描画に失敗しました");
		return FALSE;
	}

	if (FAILED(renderToSurface->EndScene(D3DX_FILTER_NONE))){
		release();
		AfxMessageBox("描画の終了に失敗しました");
		return FALSE;
	}

	//格納先領域
	//横=fpip.max_w 縦=fpip.h
	output = boost::shared_ptr<vector<PIXEL_YC> >(new vector<PIXEL_YC>(fpip.max_w * fpip.h));

	//画像データをVRAMから取得する
	{
		//メモリテクスチャからaviutlに書き戻す
		D3DLOCKED_RECT lockedRect;
		if (FAILED(hostSurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY))){
			release();
			AfxMessageBox("描画先テクスチャのロックに失敗しました");
			return FALSE;
		}

		const TEXTURE_PIXEL* source = (TEXTURE_PIXEL*)lockedRect.pBits;
		const int pitch = lockedRect.Pitch / sizeof(TEXTURE_PIXEL);
		PIXEL_YC* dest = &*output->begin();
#pragma omp parallel for
		for (int y = 0; y < height; ++y){
			for (int x = 0; x < width; ++x){
				const TEXTURE_PIXEL& sourcePixel = source[y * pitch + x];
				PIXEL_YC& destPixel = dest[y * fpip.max_w + x];
				destPixel.y  = (sourcePixel.r >> 3) - 2048;
				destPixel.cb = (sourcePixel.g >> 3) - 4096;
				destPixel.cr = (sourcePixel.b >> 3) - 4096;
			}
		}

		if (FAILED(hostSurface->UnlockRect())){
			release();
			AfxMessageBox("描画先テクスチャのロック解除に失敗しました");
			return FALSE;
		}
	}

	return true;
}

bool ProcessorGpu::prepareTemporaryArea(FILTER_PROC_INFO& fpip)
{
	const int width = fpip.w;
	const int height = fpip.h;

	if (hostTexture && hostSurface && textureWidth == width && textureHeight == height){
		return true;
	}
	textureWidth = width;
	textureHeight = height;

	//テクスチャ解放
	renderToSurface = NULL;
	hostSurface = NULL;
	hostTexture = NULL;

	//テクスチャ作成
	if (FAILED(D3DXCreateTexture(device, width, height, 1, D3DUSAGE_DYNAMIC , D3DFMT_A16B16G16R16, D3DPOOL_SYSTEMMEM, &hostTexture))){
		release();
		AfxMessageBox("メインメモリ上のテクスチャの作成に失敗しました");
		return false;
	}

	if (FAILED(hostTexture->GetSurfaceLevel(0, &hostSurface))){
		release();
		AfxMessageBox("メインメモリ上のてくちゃサーフェスの作成に失敗しました");
		return false;
	}

	if (FAILED(D3DXCreateRenderToSurface(device, width, height, D3DFMT_A16B16G16R16, FALSE, D3DFMT_UNKNOWN, &renderToSurface))){
		release();
		AfxMessageBox("テクスチャレンダリングインタフェースの作成に失敗しました");
		return false;
	}

	return true;
}
