#ifndef PROCESSOR_CUDA_H
#define PROCESSOR_CUDA_H

#include <memory>
#include "Processor.h"

class DeviceMemoryManager;
class ResultMemory;

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
private:
	bool releaseDeviceOutput();
	bool resetDeviceOutput(int width, int height);

	bool prepared;
	std::auto_ptr<DeviceMemoryManager> deviceMemoryManager;
	std::auto_ptr<ResultMemory> resultMemory;
};

#endif
