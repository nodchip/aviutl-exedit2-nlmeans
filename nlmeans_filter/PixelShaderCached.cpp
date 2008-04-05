// Copyright 2008 nod_chip
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
