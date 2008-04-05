#ifndef INPUT_TEXTURE_RAW_H
#define INPUT_TEXTURE_RAW_H

#include "InputTexture.h"

class InputTextureRaw : public InputTexture
{
public:
	InputTextureRaw(const CComPtr<IDirect3DDevice9>& device);
	virtual ~InputTextureRaw();
	CComPtr<IDirect3DTexture9> get(FILTER& fp, const FILTER_PROC_INFO& fpip, int frameIndex, const CComPtr<IDirect3DSurface9>& memorySurface);
private:
	CComPtr<IDirect3DDevice9> device;
};

#endif
