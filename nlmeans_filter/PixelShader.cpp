#include "stdafx.h"
#include "PixelShader.h"
#include "PixelShaderCached.h"
#include "PixelShaderRaw.h"

using namespace std;

boost::shared_ptr<PixelShader> PixelShader::createInstance(const CComPtr<IDirect3DDevice9>& device)
{
	return boost::shared_ptr<PixelShader>(new PixelShaderCached(boost::shared_ptr<PixelShader>(new PixelShaderRaw(device))));
}
