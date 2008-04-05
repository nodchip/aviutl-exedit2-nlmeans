#include "stdafx.h"
#include "PixelShaderCached.h"

using namespace std;

PixelShaderCached::PixelShaderCached(const boost::shared_ptr<PixelShader>& parent)
{
	this->parent = parent;
}

PixelShaderCached::~PixelShaderCached()
{
}

CComPtr<IDirect3DPixelShader9> PixelShaderCached::create(int spaceSearchRadius, int timeSearchRadius)
{
	const pair<int, int> key = make_pair(spaceSearchRadius, timeSearchRadius);
	if (memo.count(key)){
		return memo[key];
	}

	const CComPtr<IDirect3DPixelShader9> pixelShader = parent->create(spaceSearchRadius, timeSearchRadius);
	memo[key] = pixelShader;
	return pixelShader;
}
