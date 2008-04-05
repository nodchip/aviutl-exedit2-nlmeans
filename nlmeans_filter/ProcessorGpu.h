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
