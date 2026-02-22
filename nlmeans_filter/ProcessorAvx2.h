// Copyright 2026 nodchip
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

#ifndef PROCESSOR_AVX2_H
#define PROCESSOR_AVX2_H

#include "Processor.h"

// AVX2 が利用可能な場合に CPU 処理を担当するクラス。
class ProcessorAvx2 : public Processor
{
public:
	ProcessorAvx2();
	virtual ~ProcessorAvx2();

	bool isPrepared() const;
	BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip);
	BOOL wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp);

private:
	static bool detectAvx2();
	int getPixelValue(FILTER_PROC_INFO& fpip, PIXEL_YC* frames[], int x, int y, int channel, int t) const;
	void setPixelValue(FILTER_PROC_INFO& fpip, int x, int y, int channel, int value) const;
	long long calcPatchDiffAvx2(FILTER_PROC_INFO& fpip, PIXEL_YC* frames[], int x, int y, int dx, int dy, int dt, int channel, int timeSearchRadius) const;
	int currentYcpFilteringCacheSize;
	int width;
	int height;
	int numberOfFrames;
	bool prepared;
};

#endif
