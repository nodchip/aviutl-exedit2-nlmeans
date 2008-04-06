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
