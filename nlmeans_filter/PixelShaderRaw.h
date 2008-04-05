#ifndef PIXEL_SHADER_RAW_H
#define PIXEL_SHADER_RAW_H

#include "PixelShader.h"

class PixelShaderRaw : public PixelShader
{
public:
	PixelShaderRaw(const CComPtr<IDirect3DDevice9>& device);
	virtual ~PixelShaderRaw();
	CComPtr<IDirect3DPixelShader9> create(int spaceSearchRadius, int timeSearchRadius);
private:
	CComPtr<IDirect3DDevice9> device;
	static const char* PIXEL_SHADER;
};

#endif
