#ifndef PIXEL_SHADER_H
#define PIXEL_SHADER_H

#include <atlbase.h>
#include <d3d9.h>

class PixelShader
{
protected:
	PixelShader(){}
public:
	virtual ~PixelShader(){}
	virtual CComPtr<IDirect3DPixelShader9> create(int spaceSearchRadius, int timeSearchRadius) = 0;
	static boost::shared_ptr<PixelShader> createInstance(const CComPtr<IDirect3DDevice9>& device);
};

#endif
