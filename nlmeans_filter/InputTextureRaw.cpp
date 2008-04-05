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
#include "InputTextureRaw.h"

using namespace std;

InputTextureRaw::InputTextureRaw(const CComPtr<IDirect3DDevice9>& device)
{
	this->device = device;
}

InputTextureRaw::~InputTextureRaw()
{
}

CComPtr<IDirect3DTexture9> InputTextureRaw::get(FILTER& fp, const FILTER_PROC_INFO& fpip, int frameIndex, const CComPtr<IDirect3DSurface9>& memorySurface)
{
	const int width = fpip.w;
	const int height = fpip.h;
	const int numberOfFrames = fp.exfunc->get_frame_n(fpip.editp);
	frameIndex = max(0, min(numberOfFrames - 1, frameIndex));

	//フレームをメインメモリ上のテクスチャにコピーする
	D3DLOCKED_RECT lockedRect = {0};
	if (FAILED(memorySurface->LockRect(&lockedRect, NULL, D3DLOCK_DISCARD))){
		return FALSE;
	}

	const PIXEL_YC* source = fp.exfunc->get_ycp_filtering_cache_ex(&fp, fpip.editp, frameIndex, NULL, NULL);

	const char* dest = (char*)lockedRect.pBits;
	int y;
#pragma omp parallel for
	for (y = 0; y < height; ++y){
		for (int x = 0; x < width; ++x){
			unsigned short* destBeggining = (unsigned short*)&dest[x * sizeof(short) * 4 + y * lockedRect.Pitch];
			const PIXEL_YC& sourcePixel = source[x + y * width];
			//入力データが範囲を超えている場合があるので
			//符号なし16bit整数の0x4000～0xbfffにマッピングする
			destBeggining[0] = ((int)sourcePixel.y + 2048) << 3;
			destBeggining[1] = (((int)sourcePixel.cb) + 4096) << 3;
			destBeggining[2] = (((int)sourcePixel.cr) + 4096) << 3;
		}
	}

	if (FAILED(memorySurface->UnlockRect())){
		return FALSE;
	}

	//ビデオメモリ上にテクスチャ領域を作成する
	CComPtr<IDirect3DTexture9> texture;
	if (FAILED(D3DXCreateTexture(device, width, height, 1, 0, D3DFMT_A16B16G16R16, D3DPOOL_DEFAULT, &texture))){
		return NULL;
	}

	CComPtr<IDirect3DSurface9> surface;
	if (FAILED(texture->GetSurfaceLevel(0, &surface))){
		return NULL;
	}

	//メモリからGPUに転送開始
	if (FAILED(device->UpdateSurface(memorySurface, NULL, surface, NULL))){
		return NULL;
	}

	return texture;
}
