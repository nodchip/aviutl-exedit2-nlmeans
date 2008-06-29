#ifndef PROCESSOR_CUDA_H
#define PROCESSOR_CUDA_H

#include <memory>
#include <vector>

#include <cuda_runtime.h>
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
private:
	bool prepared;

	int lastWidth;
	int lastHeight;
	int lastNumberOfFrames;
	int lastSpaceRadius;
	int lastTimeRadius;

	short3* deviceSource;	//width * height * キャッシュ最大値
	std::map<int, int> sourceCacheMap;	//フレーム番号→キャッシュ領域番号
	std::list<int> sourceCacheUnusedIndex;	//未使用キャッシュ領域リスト

	short3* deviceDest;

	bool prepareDevice(FILTER& fp, FILTER_PROC_INFO& fpip);
	bool prepareDeviceSource(int width, int height, int cacheSize);
	bool prepareDeviceDest(int width, int height);
	bool releaseDeviceSource();
	bool releaseDeviceDest();
};

#endif
