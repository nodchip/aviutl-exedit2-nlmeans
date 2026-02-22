// Copyright 2008 nodchip
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
#include "ProcessorGpu.h"
#include "GpuBackendDx11.h"

ProcessorGpu::ProcessorGpu() : prepared(false), backend(new GpuBackendDx11())
{
	prepared = backend->initialize();
}

ProcessorGpu::~ProcessorGpu()
{
}

BOOL ProcessorGpu::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	if (!prepared){
		return FALSE;
	}
	return backend->process(fp, fpip);
}

BOOL ProcessorGpu::wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	(void)hwnd;
	(void)message;
	(void)wparam;
	(void)lparam;
	(void)editp;
	(void)fp;
	return FALSE;
}
