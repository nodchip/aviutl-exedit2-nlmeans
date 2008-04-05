#ifndef PIXEL_SHADER_CACHED_H
#define PIXEL_SHADER_CACHED_H

#include <map>
#include <utility>
#include <boost/shared_ptr.hpp>
#include "PixelShader.h"

class PixelShaderCached : public PixelShader
{
public:
	PixelShaderCached(const boost::shared_ptr<PixelShader>& parent);
	virtual ~PixelShaderCached();
	CComPtr<IDirect3DPixelShader9> create(int spaceSearchRadius, int timeSearchRadius);
private:
	boost::shared_ptr<PixelShader> parent;
	std::map<std::pair<int, int>, CComPtr<IDirect3DPixelShader9> > memo;
};

#endif
