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
