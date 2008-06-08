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
	static void setMaxNumberOfCache(int maxNumberOfCache);
private:

	boost::shared_ptr<InputTexture> parent;
	std::map<int, CComPtr<IDirect3DTexture9> > memo;
	std::list<int> lru;
	int width;
	int height;
	int numberOfFrames;
	static int maxNumberOfCache;
};

#endif
