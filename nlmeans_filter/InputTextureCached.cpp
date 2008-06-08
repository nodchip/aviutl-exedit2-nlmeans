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
#include "InputTextureCached.h"
#include "Cache.h"

InputTextureCached::InputTextureCached(boost::shared_ptr<InputTexture> parent) : width(-1), height(-1), numberOfFrames(-1), cache(new Cache<int, CComPtr<IDirect3DTexture9> >())
{
	this->parent = parent;
}

InputTextureCached::~InputTextureCached()
{
}

CComPtr<IDirect3DTexture9> InputTextureCached::get(FILTER& fp, const FILTER_PROC_INFO& fpip, int frameIndex, const CComPtr<IDirect3DSurface9>& hostSurface)
{
	const int width = fpip.w;
	const int height = fpip.h;
	const int numberOfFrames = fp.exfunc->get_frame_n(fpip.editp);

	if (this->width != width || this->height != height || this->numberOfFrames != numberOfFrames){
		cache->clear();
		this->width = width;
		this->height = height;
		this->numberOfFrames = numberOfFrames;

		//前のキャッシュが残る不具合(?)対策
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, 0, NULL);
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, 1, NULL);
	}

	frameIndex = max(0, min(numberOfFrames - 1, frameIndex));

	if (cache->contains(frameIndex)){
		return cache->get(frameIndex);
	}

	CComPtr<IDirect3DTexture9> texture = parent->get(fp, fpip, frameIndex, hostSurface);
	cache->add(frameIndex, texture);

	return texture;
}

void InputTextureCached::setMaxNumberOfCache(int maxNumberOfCache)
{
	cache->setMaxCacheSize(maxNumberOfCache);
}
