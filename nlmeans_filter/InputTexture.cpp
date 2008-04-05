#include "stdafx.h"
#include "InputTexture.h"
#include "InputTextureCached.h"
#include "InputTextureRaw.h"

boost::shared_ptr<InputTexture> InputTexture::createInstance(const CComPtr<IDirect3DDevice9>& device)
{
	return boost::shared_ptr<InputTexture>(new InputTextureCached(boost::shared_ptr<InputTexture>(new InputTextureRaw(device))));
}
