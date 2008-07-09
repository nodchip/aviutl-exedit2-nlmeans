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
#ifndef PROCESSOR_CUDA_H
#define PROCESSOR_CUDA_H

#include <memory>
#include <vector>

#include <cuda_runtime.h>
#include "Processor.h"

class ProcessorCuda : public Processor
{
public:
	ProcessorCuda();
	virtual ~ProcessorCuda();
	bool create();
	bool release();
	bool isPrepared() const;
	BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip);
	BOOL wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp);
private:
	bool prepared;

	int lastWidth;
	int lastHeight;
	int lastNumberOfFrames;
	int lastSpaceRadius;
	int lastTimeRadius;

	short3* deviceSource;	//width * height * キャッシュ最大値
	std::map<int, int> sourceCacheMap;	//フレーム番号→キャッシュ領域番号
	std::list<int> sourceCacheUnusedIndex;	//未使用キャッシュ領域リスト

	short3* deviceDest;

	bool prepareDevice(FILTER& fp, FILTER_PROC_INFO& fpip);
	bool prepareDeviceSource(int width, int height, int cacheSize);
	bool prepareDeviceDest(int width, int height);
	bool releaseDeviceSource();
	bool releaseDeviceDest();
};

#endif
