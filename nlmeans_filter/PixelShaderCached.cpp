// Copyright 2008 nodchip
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

#include "stdafx.h"
#include "PixelShaderCached.h"
#include "Cache.h"

using namespace std;

PixelShaderCached::PixelShaderCached(const std::shared_ptr<PixelShader>& parent) : cache(new Cache<std::pair<int, int>, CComPtr<IDirect3DPixelShader9> >())
{
	this->parent = parent;
}

PixelShaderCached::~PixelShaderCached()
{
}

CComPtr<IDirect3DPixelShader9> PixelShaderCached::create(int spaceSearchRadius, int timeSearchRadius)
{
	const pair<int, int> key = make_pair(spaceSearchRadius, timeSearchRadius);
	if (cache->contains(key)){
		return cache->get(key);
	}

	const CComPtr<IDirect3DPixelShader9> pixelShader = parent->create(spaceSearchRadius, timeSearchRadius);
	cache->add(key, pixelShader);
	return pixelShader;
}
