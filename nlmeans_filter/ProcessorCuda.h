#ifndef PROCESSOR_CUDA_H
#define PROCESSOR_CUDA_H

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
};

#endif
