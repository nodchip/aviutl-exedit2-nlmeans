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

InputTextureCached::InputTextureCached(boost::shared_ptr<InputTexture> parent) : width(-1), height(-1), numberOfFrames(-1), maxNumberOfCache(-1)
{
	this->parent = parent;
}

InputTextureCached::~InputTextureCached()
{
}

CComPtr<IDirect3DTexture9> InputTextureCached::get(FILTER& fp, const FILTER_PROC_INFO& fpip, int frameIndex, const CComPtr<IDirect3DSurface9>& memorySurface, int threadId, int numberOfThreads, int spaceSearchRadius, int timeSearchRadius)
{
	const int width = fpip.w;
	const int height = fpip.h;
	const int numberOfFrames = fp.exfunc->get_frame_n(fpip.editp);
	const int numberOfCaches = numberOfThreads * 2;

	setMaxNumberOfCache(timeSearchRadius * 2 + 2);

	if (this->width != width || this->height != height || this->numberOfFrames != numberOfFrames){
		memo.clear();
		lru.clear();
		this->width = width;
		this->height = height;
		this->numberOfFrames = numberOfFrames;

		//前のキャッシュが残る不具合(?)対策
		//複数のスレッドから呼ばれるので別スレッドにキャッシュを消されないように多めに取ってみる
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, 0, NULL);
		fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, numberOfCaches, NULL);
	}

	frameIndex = max(0, min(numberOfFrames - 1, frameIndex));

	if (memo.count(frameIndex)){
		lru.erase(find(lru.begin(), lru.end(), frameIndex));
		lru.push_front(frameIndex);
		return memo[frameIndex];
	}

	CComPtr<IDirect3DTexture9> texture = parent->get(fp, fpip, frameIndex, memorySurface, threadId, numberOfThreads, spaceSearchRadius, timeSearchRadius);
	memo[frameIndex] = texture;
	lru.push_front(frameIndex);

	while (memo.size() > maxNumberOfCache){
		memo.erase(lru.back());
		lru.pop_back();
	}

	return texture;
}

void InputTextureCached::setMaxNumberOfCache(int maxNumberOfCache)
{
	this->maxNumberOfCache = maxNumberOfCache;
}
