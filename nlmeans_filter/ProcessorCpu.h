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

#ifndef PROCESSOR_CPU_H
#define PROCESSOR_CPU_H

#include "Processor.h"

class ProcessorCpu : public Processor
{
public:
	ProcessorCpu();
	virtual ~ProcessorCpu();
	BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip);
private:
	int currentYcpFilteringCacheSize;
	int getPixelValue(FILTER_PROC_INFO& fpip, PIXEL_YC* frames[], int x, int y, int channel, int t);
	void setPixelValue(FILTER_PROC_INFO& fpip, int x, int y, int channel, int value);
};

#endif
