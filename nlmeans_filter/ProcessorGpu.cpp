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
#include <atlbase.h>
#include <afxwin.h>
#include "ProcessorGpu.h"
#include "PixelShader.h"
#include "InputTexture.h"
#include "InputTextureCached.h"

using namespace std;

const char* ProcessorGpu::softwareName = "NL-Means filter";
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

LRESULT WINAPI ProcessorGpu::msgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc( hWnd, msg, wParam, lParam );
}

ProcessorGpu::ProcessorGpu() : prepared(false), hwnd(NULL), textureWidth(-1), textureHeight(-1), frameCacheSize(-1)
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

	D3DCAPS9 caps;
	if (FAILED(direct3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps))){
		release();
		AfxMessageBox("デバイス能力の取得に失敗しました");
		return false;
	}

	D3DPRESENT_PARAMETERS presentParameters = {0};
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.Windowed = TRUE;
	presentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
	presentParameters.MultiSampleQuality = 0;

	if (FAILED(direct3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &presentParameters, &device))){
		release();
		AfxMessageBox("デバイスの作成に失敗しました");
		return false;
	}

	CComPtr<IDirect3DVertexDeclaration9> vertexDeclaration;
	if (FAILED(device->CreateVertexDeclaration(VERTEX_ELEMENTS, &vertexDeclaration))){
		release();
		AfxMessageBox("頂点宣言の作成に失敗しました");
		return false;
	}

	if (FAILED(device->SetVertexDeclaration(vertexDeclaration))){
		release();
		AfxMessageBox("頂点宣言の設定に失敗しました");
		return false;
	}

	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	prepared = true;

	pixelShaderCreator = PixelShader::createInstance(device);
	inputTextureCreator = InputTexture::createInstance(device);

	return true;
}

bool ProcessorGpu::release()
{
	inputTextureCreator.reset();
	pixelShaderCreator.reset();

	renderToSurface = NULL;
	memorySurface = NULL;
	memoryTexture = NULL;
	device = NULL;
	direct3D = NULL;

	if (hwnd){
		DestroyWindow(hwnd);
		hwnd = NULL;

		UnregisterClass(softwareName, windowClass.hInstance);
	}

	prepared = false;

	return true;
}

BOOL ProcessorGpu::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	const int width = fpip.w;
	const int height = fpip.h;
	const int frameIndex = fp.exfunc->get_frame(fpip.editp);

	//キャッシュの設定
	const int numberOfCaches = 1;
	if (frameCacheSize != numberOfCaches){
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, numberOfCaches, NULL);
		frameCacheSize = numberOfCaches;
	}

	const int spaceSearchRadius = fp.track[0];
	const int timeSearchRadius = fp.track[1];

	boost::shared_ptr<InputTextureCached> inputTextureCached = boost::shared_dynamic_cast<InputTextureCached, InputTexture>(inputTextureCreator);
	inputTextureCached->setMaxNumberOfCache(timeSearchRadius * 2 + 1);

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

	//作業用テクスチャチェック
	if (!prepareTexture(width, height)){
		release();
		AfxMessageBox("作業用テクスチャの作成に失敗しました");
		return FALSE;
	}

	//レンダリング開始
	D3DVIEWPORT9 viewPort;
	viewPort.X = 0;
	viewPort.Y = 0;
	viewPort.Width = width;
	viewPort.Height = height;
	viewPort.MinZ = 0.0f;
	viewPort.MaxZ = 0.0;

	const double H = pow(10, (double)(fp.track[2]-100) / 55.0);
	const double H2 = 1.0 / (H * H);

	D3DXVECTOR4 pixelShaderConstant[2];
	memset(pixelShaderConstant, 0, sizeof(pixelShaderConstant));
	pixelShaderConstant[0].x = 1.0f / (float)width;
	pixelShaderConstant[0].y = 1.0f / (float)height;
	pixelShaderConstant[1].x = (float)H2;

	//テクスチャをセットする
	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		const CComPtr<IDirect3DTexture9> texture = inputTextureCreator->get(fp, fpip, frameIndex + dt, memorySurface);
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
	if (FAILED(renderToSurface->BeginScene(memorySurface, &viewPort))){
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

	//画像データをVRAMから取得する
	{
		//メモリテクスチャからaviutlに書き戻す
		D3DLOCKED_RECT lockedRect;
		if (FAILED(memorySurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY))){
			release();
			AfxMessageBox("描画先テクスチャのロックに失敗しました");
			return FALSE;
		}

		const TEXTURE_PIXEL* source = (TEXTURE_PIXEL*)lockedRect.pBits;
		const int pitch = lockedRect.Pitch / sizeof(TEXTURE_PIXEL);
#pragma omp parallel for
		for (int y = 0; y < height; ++y){
			for (int x = 0; x < width; ++x){
				const TEXTURE_PIXEL& sourcePixel = source[x + y * pitch];
				PIXEL_YC& destPixel = fpip.ycp_edit[y * fpip.max_w + x];
				destPixel.y  = (sourcePixel.r >> 3) - 2048;
				destPixel.cb = (sourcePixel.g >> 3) - 4096;
				destPixel.cr = (sourcePixel.b >> 3) - 4096;
			}
		}

		if (FAILED(memorySurface->UnlockRect())){
			release();
			AfxMessageBox("描画先テクスチャのロック解除に失敗しました");
			return FALSE;
		}
	}

	return TRUE;
}

bool ProcessorGpu::prepareTexture(int width, int height)
{
	if (memoryTexture != NULL && textureWidth == width && textureHeight == height){
		return true;
	}
	textureWidth = width;
	textureHeight = height;

	//テクスチャ解放
	renderToSurface = NULL;
	memorySurface = NULL;
	memoryTexture = NULL;

	//テクスチャ作成
	if (FAILED(D3DXCreateTexture(device, width, height, 1, D3DUSAGE_DYNAMIC , D3DFMT_A16B16G16R16, D3DPOOL_SYSTEMMEM, &memoryTexture))){
		release();
		AfxMessageBox("メインメモリ上のテクスチャの作成に失敗しました");
		return false;
	}

	if (FAILED(memoryTexture->GetSurfaceLevel(0, &memorySurface))){
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
