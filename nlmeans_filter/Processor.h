#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "filter.hpp"

class Processor
{
protected:
	Processor(){}
public:
	virtual ~Processor(){}
	virtual BOOL proc(FILTER& fp, FILTER_PROC_INFO& fpip) = 0;
};

#endif

