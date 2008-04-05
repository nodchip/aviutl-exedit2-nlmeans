#include "stdafx.h"
#include "InputTextureCached.h"

InputTextureCached::InputTextureCached(boost::shared_ptr<InputTexture> parent) : width(-1), height(-1), numberOfFrames(-1), maxNumberOfCache(-1)
{
	this->parent = parent;
}

InputTextureCached::~InputTextureCached()
{
}

CComPtr<IDirect3DTexture9> InputTextureCached::get(FILTER& fp, const FILTER_PROC_INFO& fpip, int frameIndex, const CComPtr<IDirect3DSurface9>& memorySurface)
{
	const int width = fpip.w;
	const int height = fpip.h;
	const int numberOfFrames = fp.exfunc->get_frame_n(fpip.editp);

	if (this->width != width || this->height != height || this->numberOfFrames != numberOfFrames){
		memo.clear();
		lru.clear();
		this->width = width;
		this->height = height;
		this->numberOfFrames = numberOfFrames;
	}

	frameIndex = max(0, min(numberOfFrames - 1, frameIndex));

	if (memo.count(frameIndex)){
		lru.erase(find(lru.begin(), lru.end(), frameIndex));
		lru.push_front(frameIndex);
		return memo[frameIndex];
	}

	CComPtr<IDirect3DTexture9> texture = parent->get(fp, fpip, frameIndex, memorySurface);
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
