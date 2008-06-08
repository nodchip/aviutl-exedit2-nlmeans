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
#include <boost/shared_ptr.hpp>
#include "Processor.h"

class PixelShader;
class InputTexture;

template<class KEY_TYPE, class VALUE_TYPE> class Cache;

class ProcessorGpu : public Processor
{
public:
	ProcessorGpu();
	virtual ~ProcessorGpu();
	BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip);
	bool isPrepared() const{return prepared;}
	BOOL wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp){return FALSE;}
private:
	bool create();
	bool release();
	bool prepareTemporaryArea(FILTER_PROC_INFO& fpip);
	BOOL createFilteredFrame(FILTER& fp, FILTER_PROC_INFO& fpip, int frameIndex, boost::shared_ptr<std::vector<PIXEL_YC> >& output);
	boost::shared_ptr<std::vector<PIXEL_YC> > getFilteredFrame(FILTER& fp, FILTER_PROC_INFO& fpip, int frameIndex);

	volatile int frameCacheSize;
	volatile bool prepared;
	static LRESULT WINAPI msgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static const char* softwareName;
	static const WNDCLASSEX windowClass;
	static const D3DVERTEXELEMENT9 VERTEX_ELEMENTS[];

	HWND hwnd;
	CComPtr<IDirect3D9> direct3D;

	CComPtr<IDirect3DDevice9> device;
	CComPtr<IDirect3DTexture9> hostTexture;
	CComPtr<IDirect3DSurface9> hostSurface;
	CComPtr<ID3DXRenderToSurface> renderToSurface;
	boost::shared_ptr<PixelShader> pixelShaderCreator;
	boost::shared_ptr<InputTexture> inputTextureCreator;
	int textureWidth;
	int textureHeight;

	//先読み用
	struct FRAME_DATA
	{
		boost::shared_ptr<CEvent> event;
		boost::shared_ptr<std::vector<PIXEL_YC> > frame;
		FRAME_DATA() : event(new CEvent(FALSE, TRUE)){}
	};
	std::auto_ptr<Cache<int, boost::shared_ptr<FRAME_DATA> > > cache;
	CCriticalSection criticalSectionCache;
	volatile int baseFrameIndex;	//現在AviUtl本体がエンコード中であろうと思われる動画のフレーム番号
	FILTER* currentFp;
	FILTER_PROC_INFO* currentFpip;
	volatile bool priorReadThreadStarted;

	static UINT priorReadThreadProc(LPVOID param);
	UINT priorRead(FILTER& fp, FILTER_PROC_INFO& fpip);
	CEvent eventPriorReadStopper;
};

#endif
