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

#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "filter.hpp"

class Processor
{
protected:
	Processor(){}
public:
	virtual ~Processor(){}
	virtual bool isPrepared() const = 0;
	virtual BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip) = 0;
	virtual BOOL wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp) = 0;
};

#endif

