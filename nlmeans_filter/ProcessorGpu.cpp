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

template<class T>
static void Message(char* message, T& arg0)
{
	AfxMessageBox((boost::format(message) % arg0).str().c_str());
}

LRESULT WINAPI ProcessorGpu::msgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc( hWnd, msg, wParam, lParam );
}

ProcessorGpu::ProcessorGpu() : prepared(false), hwnd(NULL), currentFp(NULL), currentFpip(NULL), frameCacheSize(-1)
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

	const int numberOfAdapters = direct3D->GetAdapterCount();

	threadParameters.resize(numberOfAdapters);

	for (int adapterIndex = 0; adapterIndex < numberOfAdapters; ++adapterIndex){
		D3DCAPS9 caps;
		if (FAILED(direct3D->GetDeviceCaps(adapterIndex, D3DDEVTYPE_HAL, &caps))){
			release();
			Message("デバイス能力の取得に失敗しました(adapterIndex:%1%)", adapterIndex);
			return false;
		}

		GPU gpu;

		D3DPRESENT_PARAMETERS presentParameters = {0};
		presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
		presentParameters.Windowed = TRUE;
		presentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
		presentParameters.MultiSampleQuality = 0;

		if (FAILED(direct3D->CreateDevice(adapterIndex, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &presentParameters, &gpu.device))){
			release();
			Message("デバイスの作成に失敗しました(adapterIndex:%1%)", adapterIndex);
			return false;
		}

		CComPtr<IDirect3DVertexDeclaration9> vertexDeclaration;
		if (FAILED(gpu.device->CreateVertexDeclaration(VERTEX_ELEMENTS, &vertexDeclaration))){
			release();
			Message("頂点宣言の作成に失敗しました(adapterIndex:%1%)", adapterIndex);
			return false;
		}

		if (FAILED(gpu.device->SetVertexDeclaration(vertexDeclaration))){
			release();
			Message("頂点宣言の設定に失敗しました(adapterIndex:%1%)", adapterIndex);
			return false;
		}

		gpu.device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		gpu.device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		gpu.device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		gpu.device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		gpu.device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

		gpu.pixelShaderCreator = PixelShader::createInstance(gpu.device);
		gpu.inputTextureCreator = InputTexture::createInstance(gpu.device);

		gpus.push_back(gpu);

		THREAD_PARAMETER& threadParameter = threadParameters[adapterIndex];
		threadParameter.processor = this;
		threadParameter.threadId = adapterIndex;
		threadParameter.eventWaitForNextRendering = boost::shared_ptr<CEvent>(new CEvent());
		threadParameter.eventWaitForRenderingDone = boost::shared_ptr<CEvent>(new CEvent());
		threadParameter.thread = ::AfxBeginThread(threadProc, &threadParameter);
	}

	prepared = true;

	return true;
}

bool ProcessorGpu::release()
{
	mutex.Lock();
	prepared = false;
	mutex.Unlock();

	const int numberOfAdapters = getNumberOfAdapters();
	for (int threadId = 0; threadId < numberOfAdapters; ++threadId){
		THREAD_PARAMETER& threadParameter = threadParameters[threadId];
		threadParameter.eventWaitForNextRendering->SetEvent();
		//::WaitForSingleObject(threadParameter.thread->m_hThread, INFINITE);
	}

	//スレッドインスタンスが消されるまで待っている(つもり)
	//本当はこうやっていはいけない
	Sleep(100);

	gpus.clear();

	direct3D = NULL;

	if (hwnd){
		DestroyWindow(hwnd);
		hwnd = NULL;

		UnregisterClass(softwareName, windowClass.hInstance);
	}

	return true;
}

UINT ProcessorGpu::threadProc(LPVOID pParam)
{
	THREAD_PARAMETER& threadParameter = *(THREAD_PARAMETER*)pParam;
	ProcessorGpu& processor = *threadParameter.processor;
	const int threadId = threadParameter.threadId;
	const int numberOfAdapters = processor.getNumberOfAdapters();


	while (::WaitForSingleObject(threadParameter.eventWaitForNextRendering->m_hObject, INFINITE) == WAIT_OBJECT_0){
		processor.mutex.Lock();
		if (!processor.prepared){
			processor.mutex.Unlock();
			break;
		}
		processor.mutex.Unlock();

		processor.procBody(threadId, numberOfAdapters);
		threadParameter.eventWaitForRenderingDone->SetEvent();
	}

	return 0;
}

BOOL ProcessorGpu::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	const int numberOfAdapters = getNumberOfAdapters();
	const bool useMultiGpu = fp.check[1] != 0;
	if (useMultiGpu){
		sprintf(titleBuffer, "NL-Meansフィルタ(%d並列動作中)", numberOfAdapters);
		::SetWindowText(fp.hwnd, titleBuffer);
	} else {
		sprintf(titleBuffer, "NL-Meansフィルタ");
		::SetWindowText(fp.hwnd, titleBuffer);
	}

	currentFp = &fp;
	currentFpip = &fpip;

	//キャッシュの設定
	const int width = fpip.w;
	const int height = fpip.h;
	const int numberOfCaches = numberOfAdapters * 2;
	if (frameCacheSize != numberOfCaches){
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, numberOfCaches, NULL);
		frameCacheSize = numberOfCaches;
	}

	if (useMultiGpu){
		for (int threadId = 0; threadId < numberOfAdapters; ++threadId){
			THREAD_PARAMETER& threadParameter = threadParameters[threadId];
			threadParameter.eventWaitForNextRendering->SetEvent();
		}

		for (int threadId = 0; threadId < numberOfAdapters; ++threadId){
			THREAD_PARAMETER& threadParameter = threadParameters[threadId];
			::WaitForSingleObject(threadParameter.eventWaitForRenderingDone->m_hObject, INFINITE);
		}

		return TRUE;
	} else {
		return procBody(0, 1);
	}
}

bool ProcessorGpu::prepareTexture(int threadId, int width, int height)
{
	GPU& gpu = gpus[threadId];

	if (gpu.memoryTexture != NULL && gpu.textureWidth == width && gpu.textureHeight == height){
		return true;
	}
	gpu.textureWidth = width;
	gpu.textureHeight = height;

	//テクスチャ解放
	gpu.renderToSurface = NULL;
	gpu.memorySurface = NULL;
	gpu.memoryTexture = NULL;

	//テクスチャ作成
	if (FAILED(D3DXCreateTexture(gpu.device, width, height, 1, D3DUSAGE_DYNAMIC , D3DFMT_A16B16G16R16, D3DPOOL_SYSTEMMEM, &gpu.memoryTexture))){
		release();
		AfxMessageBox("メインメモリ上のテクスチャの作成に失敗しました");
		return false;
	}

	if (FAILED(gpu.memoryTexture->GetSurfaceLevel(0, &gpu.memorySurface))){
		release();
		AfxMessageBox("メインメモリ上のてくちゃサーフェスの作成に失敗しました");
		return false;
	}

	if (FAILED(D3DXCreateRenderToSurface(gpu.device, width, height, D3DFMT_A16B16G16R16, FALSE, D3DFMT_UNKNOWN, &gpu.renderToSurface))){
		release();
		AfxMessageBox("テクスチャレンダリングインタフェースの作成に失敗しました");
		return false;
	}

	return true;

}

bool ProcessorGpu::procBody(int threadId, int numberOfThreads)
{
	GPU& gpu = gpus[threadId];

	const int width = currentFpip->w;
	const int height = currentFpip->h;
	const int frameIndex = currentFp->exfunc->get_frame(currentFpip->editp);

	const int spaceSearchRadius = currentFp->track[0];
	const int timeSearchRadius = currentFp->track[1];

	//ピクセルシェーダ作成
	const CComPtr<IDirect3DPixelShader9> pixelShader = gpu.pixelShaderCreator->create(spaceSearchRadius, timeSearchRadius);
	if (pixelShader == NULL){
		release();
		AfxMessageBox("ピクセルシェーダの作成に失敗しました");
		return FALSE;
	}

	//ピクセルシェーダの設定
	if (FAILED(gpu.device->SetPixelShader(pixelShader))){
		release();
		AfxMessageBox("ピクセルシェーダの設定に失敗しました");
		return FALSE;
	}

	//作業用テクスチャチェック
	if (!prepareTexture(threadId, width, height)){
		release();
		AfxMessageBox("作業用テクスチャの作成に失敗しました");
		return FALSE;
	}

	//レンダリング開始
	const double H = pow(10, (double)(currentFp->track[2]-100) / 55.0);
	const double H2 = 1.0 / (H * H);

	D3DXVECTOR4 pixelShaderConstant[2];
	memset(pixelShaderConstant, 0, sizeof(pixelShaderConstant));
	pixelShaderConstant[0].x = 1.0f / (float)width;
	pixelShaderConstant[0].y = 1.0f / (float)height;
	pixelShaderConstant[1].x = (float)H2;

	//テクスチャをセットする
	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		static CMutex mutex;
		mutex.Lock();
		const CComPtr<IDirect3DTexture9> texture = gpu.inputTextureCreator->get(*currentFp, *currentFpip, frameIndex + dt, gpu.memorySurface, threadId, numberOfThreads, spaceSearchRadius, timeSearchRadius);
		mutex.Unlock();
		if (texture == NULL){
			release();
			AfxMessageBox("フレームテクスチャの作成に失敗しました");
			return FALSE;
		}

		if (FAILED(gpu.device->SetTexture(dt + timeSearchRadius, texture))){
			release();
			AfxMessageBox("テクスチャの設定に失敗しました");
			return FALSE;
		}
	}

	//シーンのレンダリング開始
	if (FAILED(gpu.device->Clear(0, NULL, D3DCLEAR_TARGET, 0, 0, 0))){
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

	if (FAILED(gpu.renderToSurface->BeginScene(gpu.memorySurface, &viewPort))){
		release();
		AfxMessageBox("シーンの開始に失敗しました");
		return FALSE;
	}

	if (FAILED(gpu.device->SetPixelShaderConstantF(0, (float*)pixelShaderConstant, 2))){
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

	if (FAILED(gpu.device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, polygon, 12))){
		release();
		AfxMessageBox("プリミティブの描画に失敗しました");
		return FALSE;
	}

	if (FAILED(gpu.renderToSurface->EndScene(D3DX_FILTER_NONE))){
		release();
		AfxMessageBox("描画の終了に失敗しました");
		return FALSE;
	}

	//画像データをVRAMから取得する
	{
		//メモリテクスチャからaviutlに書き戻す
		D3DLOCKED_RECT lockedRect;
		if (FAILED(gpu.memorySurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY))){
			release();
			AfxMessageBox("描画先テクスチャのロックに失敗しました");
			return FALSE;
		}

		const int yBegin = height * threadId / numberOfThreads;
		const int yEnd = height * (threadId + 1) / numberOfThreads;
		const TEXTURE_PIXEL* source = (TEXTURE_PIXEL*)lockedRect.pBits;
		const int pitch = lockedRect.Pitch / sizeof(TEXTURE_PIXEL);
#pragma omp parallel for
		for (int y = yBegin; y < yEnd; ++y){
			for (int x = 0; x < width; ++x){
				const TEXTURE_PIXEL& sourcePixel = source[y * pitch + x];
				PIXEL_YC& destPixel = currentFpip->ycp_edit[y * currentFpip->max_w + x];
				destPixel.y  = (sourcePixel.r >> 3) - 2048;
				destPixel.cb = (sourcePixel.g >> 3) - 4096;
				destPixel.cr = (sourcePixel.b >> 3) - 4096;
			}
		}

		if (FAILED(gpu.memorySurface->UnlockRect())){
			release();
			AfxMessageBox("描画先テクスチャのロック解除に失敗しました");
			return FALSE;
		}
	}

	return true;
}

int ProcessorGpu::getNumberOfAdapters()
{
	return (int)gpus.size();
}
