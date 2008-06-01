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

#include <vector>
#include <atlutil.h>
#include "Processor.h"

class PixelShader;
class InputTexture;

class ProcessorGpu : public Processor
{
public:
	ProcessorGpu();
	virtual ~ProcessorGpu();
	BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip);
	bool isPrepared() const{return prepared;}
	BOOL wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp){return FALSE;}
	int getNumberOfAdapters();
private:
	bool create();
	bool release();
	bool prepareTexture(int threadId, int width, int height);
	bool procBody(int threadId, int numberOfThreads);
	static DWORD WINAPI threadProc(LPVOID lpParameter);

	struct THREAD_PARAMETER
	{
		ProcessorGpu* processor;
		int threadId;
		HANDLE handle;
	};
	std::vector<THREAD_PARAMETER> threadParameters;

	struct GPU
	{
		GPU() : textureWidth(-1), textureHeight(-1){}
		virtual ~GPU(){}
		CComPtr<IDirect3DDevice9> device;
		CComPtr<IDirect3DTexture9> memoryTexture;
		CComPtr<IDirect3DSurface9> memorySurface;
		CComPtr<ID3DXRenderToSurface> renderToSurface;
		boost::shared_ptr<PixelShader> pixelShaderCreator;
		boost::shared_ptr<InputTexture> inputTextureCreator;
		int textureWidth;
		int textureHeight;
	};

	volatile bool prepared;
	static LRESULT WINAPI msgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static const char* softwareName;
	static const WNDCLASSEX windowClass;
	static const D3DVERTEXELEMENT9 VERTEX_ELEMENTS[];

	char titleBuffer[1024];

	HWND hwnd;
	CComPtr<IDirect3D9> direct3D;
	std::vector<GPU> gpus;

	//ここvolatileつけないとだめだと思う・・・
	FILTER* currentFp;
	FILTER_PROC_INFO* currentFpip;

	volatile int frameCacheSize;
};

#endif
