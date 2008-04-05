#ifndef INPUT_TEXTURE_CACHED_H
#define INPUT_TEXTURE_CACHED_H

#include <map>
#include <list>
#include <boost/shared_ptr.hpp>
#include "InputTexture.h"

class InputTextureCached : public InputTexture
{
public:
	InputTextureCached(boost::shared_ptr<InputTexture> parent);
	virtual ~InputTextureCached();
	CComPtr<IDirect3DTexture9> get(FILTER& fp, const FILTER_PROC_INFO& fpip, int frameIndex, const CComPtr<IDirect3DSurface9>& memorySurface);
	void setMaxNumberOfCache(int maxNumberOfCache);
private:
	boost::shared_ptr<InputTexture> parent;
	std::map<int, CComPtr<IDirect3DTexture9> > memo;
	std::list<int> lru;
	int width;
	int height;
	int numberOfFrames;
	int maxNumberOfCache;
};

#endif
