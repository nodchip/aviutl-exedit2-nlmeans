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

#ifndef PROCESSOR_SSE2_N099_H
#define PROCESSOR_SSE2_N099_H

#include "Processor.h"

class ProcessorSse2N099 : public Processor
{
public:
	ProcessorSse2N099();
	virtual ~ProcessorSse2N099();
	BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip);
	bool isPrepared() const{return prepared;}
	BOOL wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp);
private:
	bool prepared;
	FILTER_DLL& filterDll;
};

#endif
