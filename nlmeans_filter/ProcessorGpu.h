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

#ifndef PROCESSOR_GPU_H
#define PROCESSOR_GPU_H

#include "Processor.h"

class PixelShader;
class InputTexture;

class ProcessorGpu : public Processor
{
public:
	ProcessorGpu();
	virtual ~ProcessorGpu();
	bool isPrepared();
	BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip);
private:
	bool create();
	bool release();
	bool prepareTexture(int width, int height);

	bool prepared;
	static LRESULT WINAPI msgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static const char* softwareName;
	static const WNDCLASSEX windowClass;
	static const D3DVERTEXELEMENT9 VERTEX_ELEMENTS[];
	HWND hwnd;
	CComPtr<IDirect3D9> direct3D;
	CComPtr<IDirect3DDevice9> device;
	CComPtr<IDirect3DTexture9> memoryTexture;
	CComPtr<IDirect3DSurface9> memorySurface;
	CComPtr<ID3DXRenderToSurface> renderToSurface;
	int textureWidth;
	int textureHeight;
	boost::shared_ptr<PixelShader> pixelShaderCreator;
	boost::shared_ptr<InputTexture> inputTextureCreator;
	int frameCacheSize;
};

#endif
