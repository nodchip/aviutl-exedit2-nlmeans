// Copyright 2008 nod_chip
// Copyright 2008 Aroo		(Vectorization with SSE2)
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

#ifndef PROCESSOR_SSE2_AROO_H
#define PROCESSOR_SSE2_AROO_H

#include "Processor.h"
#include <emmintrin.h>

class ProcessorSse2Aroo : public Processor
{
public:
	ProcessorSse2Aroo();
	virtual ~ProcessorSse2Aroo();
	bool isPrepared() const {return prepared;};
	BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip);
	BOOL wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp);

private:
	class AllocMemory
	{
	public:
		int pitch;
	private:
		int w,h,s,c;
		__m64 *ap;
	public:
		__m64 *pv[16];
		const int max_radius;

	public:
		AllocMemory();
		~AllocMemory();

		__m64* allocate(int nw, int nh, int nc);
		void deallocate();
	} mem;

	int width, height, numberOfFrames;
	int currentYcpFilteringCacheSize;
 	bool prepared;
};

#endif /*PROCESSOR_SSE2_AROO_H*/