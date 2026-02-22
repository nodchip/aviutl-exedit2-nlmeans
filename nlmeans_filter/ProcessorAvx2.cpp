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

#include "stdafx.h"
#include <intrin.h>
#include "ProcessorAvx2.h"
#include "ProcessorCpu.h"

ProcessorAvx2::ProcessorAvx2() : fallbackProcessor(new ProcessorCpu()), prepared(detectAvx2())
{
}

ProcessorAvx2::~ProcessorAvx2()
{
	delete fallbackProcessor;
	fallbackProcessor = NULL;
}

bool ProcessorAvx2::isPrepared() const
{
	return prepared;
}

BOOL ProcessorAvx2::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	// TODO: AVX2 最適化版の実装へ置き換える。
	return fallbackProcessor->proc(fp, fpip);
}

BOOL ProcessorAvx2::wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	return fallbackProcessor->wndProc(hwnd, message, wparam, lparam, editp, fp);
}

bool ProcessorAvx2::detectAvx2()
{
	int cpuInfo[4] = {0, 0, 0, 0};
	__cpuid(cpuInfo, 0);
	if (cpuInfo[0] < 7){
		return false;
	}

	__cpuidex(cpuInfo, 7, 0);
	return (cpuInfo[1] & (1 << 5)) != 0;
}
