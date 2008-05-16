#include <cstdio>
#include <ctime>
#include <list>
#include <map>
#include <windows.h>
#include "ProcessorCuda.h"

using namespace std;

static FILE* logFile = NULL;
static const char* cudaLogFileName = "cudaLog.txt";

////////////////////////////////////////////////////////////////////////////////
//簡易ログファイル開閉用クラス
//本当はこんな簡易クラスを作らずにちゃんとしたロガーを使うべきなのだが、
//そもそもCUDAがどのような動作をするのかがわからないので、
//まぁいいや。
////////////////////////////////////////////////////////////////////////////////
class Log
{
public:
	Log();
	virtual ~Log();
	void write(const char* s);
};

Log::Log()
{
	logFile = fopen(cudaLogFileName, "at");
};

Log::~Log()
{
	fclose(logFile);
	logFile = NULL;
};

void Log::write(const char* s)
{
	time_t t;
	time(&t);
	char* timeString = ctime(&t);
	timeString[strlen(timeString) - 1] = '\0';
	fprintf(logFile, "%s - %s\n", timeString, s);
	printf("%s\n", ctime(&t), s);
}

static Log LOG;

////////////////////////////////////////////////////////////////////////////////
//デバイスメモリの抽象化
////////////////////////////////////////////////////////////////////////////////
class DeviceMemory
{
public:
	DeviceMemory();
	DeviceMemory(void* hostMemory, int size);
	virtual ~DeviceMemory();
	void* get() const;
private:
	bool create(void* hostMemory, int size);
	bool release();

	void* deviceMemory;
	int size;
};

DeviceMemory::DeviceMemory() : deviceMemory(NULL), size(0)
{
}

DeviceMemory::DeviceMemory(void* hostMemory, int size) : deviceMemory(NULL), size(0)
{
	create(hostMemory, size);
}

DeviceMemory::~DeviceMemory()
{
	release();
}

void* DeviceMemory::get() const
{
	return deviceMemory;
}

bool DeviceMemory::create(void* hostMemory, int size)
{
	if (cudaMalloc(&deviceMemory, size) != cudaSuccess){
		LOG.write("DeviceMemory::create() - デバイスメモリの確保に失敗しました");
		release();
		return false;
	}

	if (cudaMemcpy(deviceMemory, hostMemory, size, cudaMemcpyHostToDevice) != cudaSuccess){
		LOG.write("DeviceMemory::create() - デバイスメモリへのコピーに失敗しました");
		release();
		return false;
	}

	this->size = size;

	return true;
}

bool DeviceMemory::release()
{
	if (deviceMemory == NULL && size == 0){
		return true;
	}

	if (cudaFree(deviceMemory) != cudaSuccess){
		LOG.write("DeviceMemory::create() - デバイスメモリの開放に失敗しました");
		deviceMemory = NULL;
		size = 0;
		return false;
	}

	deviceMemory = NULL;
	size = 0;

	return true;
}

////////////////////////////////////////////////////////////////////////////////
//デバイスメモリの管理クラス
//LRUアルゴリズム部分を分離できるが、面倒なのでこのままいく
//なぜboost::shared_ptrが使えない？
//生のポインタとか使いたくない
////////////////////////////////////////////////////////////////////////////////
class DeviceMemoryManager
{
public:
	DeviceMemoryManager(int limit);
	virtual ~DeviceMemoryManager();
	void* get(FILTER& fp, FILTER_PROC_INFO& fpip, int frameIndex);
	int getLimit() const;
private:
	DeviceMemory* createDeviceMemory(FILTER& fp, FILTER_PROC_INFO& fpip, int frameIndex);
	int limit;
	list<int> frameIndexOrder;
	map<int, pair<DeviceMemory*, list<int>::iterator> > frameIndexToDeviceMemory;
};

DeviceMemoryManager::DeviceMemoryManager(int limit) : limit(limit)
{
}

DeviceMemoryManager::~DeviceMemoryManager()
{
	for (map<int, pair<DeviceMemory*, list<int>::iterator> >::iterator it = frameIndexToDeviceMemory.begin(); it != frameIndexToDeviceMemory.end(); ++it){
		delete it->second.first;
	}
}

void* DeviceMemoryManager::get(FILTER& fp, FILTER_PROC_INFO& fpip, int frameIndex)
{
	const int numberOfFrames = fp.exfunc->get_frame_n(fpip.editp);
	frameIndex = max(0, min(numberOfFrames - 1, frameIndex));

	map<int, pair<DeviceMemory*, list<int>::iterator> >::iterator it;

	if ((it = frameIndexToDeviceMemory.find(frameIndex)) != frameIndexToDeviceMemory.end()){
		frameIndexOrder.erase(it->second.second);
		frameIndexOrder.push_front(frameIndex);
		it->second.second = frameIndexOrder.begin();
		return it->second.first->get();
	}

	DeviceMemory* deviceMemory = createDeviceMemory(fp, fpip, frameIndex);

	frameIndexOrder.push_front(frameIndex);
	frameIndexToDeviceMemory.insert(make_pair(frameIndex, make_pair(deviceMemory, frameIndexOrder.begin())));

	if (frameIndexOrder.size() > limit){

		it = frameIndexToDeviceMemory.find(frameIndexOrder.back());
		delete it->second.first;
		frameIndexToDeviceMemory.erase(it);
		frameIndexOrder.pop_back();
	}

	return deviceMemory->get();
}

int DeviceMemoryManager::getLimit() const
{
	return limit;
}

DeviceMemory* DeviceMemoryManager::createDeviceMemory(FILTER& fp, FILTER_PROC_INFO& fpip, int frameIndex)
{
	const int width = fpip.w;
	const int height = fpip.h;
	const PIXEL_YC* source = fp.exfunc->get_ycp_filtering_cache_ex(&fp, fpip.editp, frameIndex, NULL, NULL);
	const int size = sizeof(PIXEL_YC) * width * height;

	return new DeviceMemory((void*)source, size);
}

////////////////////////////////////////////////////////////////////////////////
//結果出力用メモリ管理クラス
////////////////////////////////////////////////////////////////////////////////
class ResultMemory
{
public:
	ResultMemory();
	virtual ~ResultMemory();
	short3* get(int width, int height);
private:
	bool release();
	bool resize(int width, int height);
	int width;
	int height;
	short3* buffer;
};

ResultMemory::ResultMemory() : width(0), height(0), buffer(NULL)
{
}

ResultMemory::~ResultMemory()
{
	release();
}

short3* ResultMemory::get(int width, int height)
{
	if (!resize(width, height)){
		return NULL;
	}

	return buffer;
}

bool ResultMemory::release()
{
	if (buffer == NULL){
		return true;
	}

	if (cudaFree(buffer) != cudaSuccess){
		LOG.write("ResultMemory::release() - 結果出力用のメモリの開放に失敗しました");
		return false;
	}

	buffer = NULL;
	width = 0;
	height = 0;

	return true;
}

bool ResultMemory::resize(int width, int height)
{
	if (this->width == width && this->height == height){
		return true;
	}

	release();

	const int size = width * height * sizeof(short3);
	cudaError error;
	if ((error = cudaMalloc((void**)&buffer, size)) != cudaSuccess){
		LOG.write("ResultMemory::resize() - 結果出力用のメモリの確保に失敗しました");
		LOG.write(cudaGetErrorString(error));
		release();
		return false;
	}

	this->width = width;
	this->height = height;

	return true;
}


////////////////////////////////////////////////////////////////////////////////
//フィルタ本体
////////////////////////////////////////////////////////////////////////////////
ProcessorCuda::ProcessorCuda() : prepared(false)
{
	create();
}

ProcessorCuda::~ProcessorCuda()
{
	release();
}

bool ProcessorCuda::create()
{
	LOG.write("ProcessorCuda::create() - デバイスの初期化を開始しました");

	cudaError error;
	int deviceCount;

	if ((error = cudaGetDeviceCount(&deviceCount)) != cudaSuccess){
		LOG.write("ProcessorCuda::create() - デバイス数の取得に失敗しました");
		LOG.write(cudaGetErrorString(error));
		release();
		return false;
	}

	if (deviceCount == 0) {
		LOG.write("ProcessorCuda::create() - CUDAをサポートするデバイスが見つかりません");
		release();
		return false;
	}

	int dev = 0;
	cudaDeviceProp deviceProp;
	if ((error = cudaGetDeviceProperties(&deviceProp, dev)) != cudaSuccess){
		LOG.write("ProcessorCuda::create() - デバイスプロパティの取得に失敗しました");
		LOG.write(cudaGetErrorString(error));
		release();
		return false;
	}

	if (deviceProp.major < 1){
		LOG.write("ProcessorCuda::create() - デバイスがCUDAをサポートしていません");
		release();
		return false;
	}

	cudaSetDevice(dev);

	char buffer[1024];
	sprintf(buffer, "ProcessorCuda::create() - デバイス %d: %s を使用します", dev, deviceProp.name);
	LOG.write(buffer);

	prepared = true;
	return true;
}

bool ProcessorCuda::release()
{
	prepared = false;

	return true;
}

bool ProcessorCuda::isPrepared() const
{
	return prepared;
}

__global__ void kernelProccess(const short3* in[], int width, int height, int maxWidth, int maxHeight, int spaceSearchRadius, int timeSearchRadius, float h2, short3* out);

BOOL ProcessorCuda::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	if (!prepared){
		return FALSE;
	}

	const int width = fpip.w;
	const int height = fpip.h;
	const int maxWidth = fpip.max_w;
	const int maxHeight = fpip.max_h;
	const int frameIndex = fp.exfunc->get_frame(fpip.editp);

	const int spaceSearchRadius = fp.track[0];
	const int timeSearchRadius = fp.track[1];
	const int timeSearchDiameter = timeSearchRadius * 2 + 1;

	if (deviceMemoryManager.get() == NULL || deviceMemoryManager->getLimit() != timeSearchDiameter){
		deviceMemoryManager = auto_ptr<DeviceMemoryManager>(new DeviceMemoryManager(timeSearchDiameter));
	}

	if (resultMemory.get() == NULL){
		resultMemory = auto_ptr<ResultMemory>(new ResultMemory());
	}
	short3* resultMemoryPointer = resultMemory->get(maxWidth, maxHeight);

	const short3* deviceMemories[32];
	memset(deviceMemories, 0, sizeof(deviceMemories));
	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		deviceMemories[dt + timeSearchRadius] = (const short3*)deviceMemoryManager->get(fp, fpip, frameIndex + dt);
	}

	const float H = (float)pow(10, (double)(fp.track[2]-100) / 55.0);
	const float H2 = (float)1.0 / (H * H);

	const dim3 db = make_uint3(512, 512, 1);
	const dim3 dg = make_uint3((width - 1) / db.x + 1, (height - 1) / db.y + 1, 1);

	kernelProccess<<<dg, db>>>(deviceMemories, width, height, maxWidth, maxHeight, spaceSearchRadius, timeSearchRadius, H2, resultMemoryPointer);

	if (cudaMemcpy(fpip.ycp_edit, resultMemoryPointer, maxWidth * maxHeight * sizeof(short3), cudaMemcpyDeviceToHost) != cudaSuccess){
		LOG.write("ProcessorCuda::proc() - 結果のコピーに失敗しました");
		release();
		return FALSE;
	}

	return TRUE;
}

BOOL ProcessorCuda::wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	return DefWindowProc( hwnd, message, wparam, lparam );
}

__global__ void kernelProccess(const short3* in[], int width, int height, int maxWidth, int maxHeight, int spaceSearchRadius, int timeSearchRadius, float h2, short3* out)
{
	const int sourceX = blockIdx.x * blockDim.x + threadIdx.x;
	const int sourceY = blockIdx.y * blockDim.y + threadIdx.y;

	if (sourceX >= width || sourceY >= height){
		return;
	}

	const int timeSearchDiameter = timeSearchRadius * 2 + 1;

	float3 sum = make_float3(0, 0, 0);
	float3 value = make_float3(0, 0, 0);
	for (int dt = 0; dt < timeSearchDiameter; ++dt){
		for (int dx = -spaceSearchRadius; dx <= spaceSearchRadius; ++dx){
			for (int dy = -spaceSearchRadius; dy <= spaceSearchRadius; ++dy){
				int3 sum2 = make_int3(0, 0, 0);
				for (int xx = -1; xx <= 1; ++xx){
					for (int yy = -1; yy <= 1; ++yy){
						const int x0 = max(0, min(width - 1, sourceX + xx));
						const int y0 = max(0, min(height - 1, sourceY + yy));
						const int x1 = max(0, min(width - 1, sourceX + dx + xx));
						const int y1 = max(0, min(height - 1, sourceY + dy + yy));

						const short3 source = in[timeSearchRadius][x0 + width * y0];
						const short3 dest = in[dt][x0 + height * y0];

						const int3 diff = make_int3(source.x - dest.x, source.y - dest.y, source.z - dest.z);

						sum2.x += diff.x * diff.x;
						sum2.y += diff.y * diff.y;
						sum2.z += diff.z * diff.z;
					}
				}

				const float3 w = make_float3(__expf(-sum2.x * h2), __expf(-sum2.y * h2), __expf(-sum2.y * h2));
				sum.x += w.x;
				sum.y += w.y;
				sum.z += w.z;
				value.x += w.x * in[timeSearchRadius][sourceX + width * sourceY].x;
				value.y += w.y * in[timeSearchRadius][sourceX + width * sourceY].y;
				value.z += w.z * in[timeSearchRadius][sourceX + width * sourceY].z;
			}
		}
	}

	value.x /= sum.x;
	value.y /= sum.y;
	value.z /= sum.z;
	out[sourceX + maxWidth * sourceY] = make_short3((short)value.x, (short)value.y, (short)value.y);
}
