#ifndef INPUT_TEXTURE_H
#define INPUT_TEXTURE_H

#include <atlbase.h>

class InputTexture
{
public:
	InputTexture(){}
	virtual ~InputTexture(){}
	virtual CComPtr<IDirect3DTexture9> get(FILTER& fp, const FILTER_PROC_INFO& fpip, int frameIndex, const CComPtr<IDirect3DSurface9>& memorySurface) = 0;
	static boost::shared_ptr<InputTexture> createInstance(const CComPtr<IDirect3DDevice9>& device);
};

#endif
