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
	fflush(logFile);
	printf("%s\n", ctime(&t), s);
}

static Log LOG;

////////////////////////////////////////////////////////////////////////////////
//フィルタ本体
////////////////////////////////////////////////////////////////////////////////
ProcessorCuda::ProcessorCuda()
{
	prepared = false;
	deviceSource = deviceDest = NULL;
	lastWidth = lastHeight = lastNumberOfFrames = lastSpaceRadius = lastTimeRadius = -1;
	create();
}

ProcessorCuda::~ProcessorCuda()
{
	release();
}

bool ProcessorCuda::create()
{
	LOG.write("ProcessorCuda::create() - デバイスの初期化を開始しました");

	cudaError_t error;
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
	releaseDeviceSource();
	releaseDeviceDest();

	prepared = false;

	return true;
}

bool ProcessorCuda::isPrepared() const
{
	return prepared;
}

__global__ void kernelProccess(short3* in, short3* out);
__constant__ int constantWidth;
__constant__ int constantHeight;
__constant__ int constantMaxWidth;
__constant__ int constantMaxHeight;
__constant__ int constantSpaceSearchRadius;
__constant__ int constantTimeSearchRadius;
__constant__ int constantCurrentFrameCacheIndex;
__constant__ float constantH2;
texture<short, 1, cudaReadModeElementType> texRef;

BOOL ProcessorCuda::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	if (!prepared){
		return FALSE;
	}

	cudaError_t error;

	const int width = fpip.w;
	const int height = fpip.h;
	const int maxWidth = fpip.max_w;
	const int maxHeight = fpip.max_h;
	const int currentFrameIndex = fpip.frame;
	const int numberOfFrames = fpip.frame_n;

	const int spaceSearchRadius = fp.track[0];
	const int timeSearchRadius = fp.track[1];

	//デバイス準備
	if (!prepareDevice(fp, fpip)){
		LOG.write("ProcessorCuda::proc() - デバイスの準備に失敗しました");
		release();
		return FALSE;
	}

	//入力データ転送
	int currentFrameCacheIndex = -1;

	for (int dt = -timeSearchRadius; dt <= timeSearchRadius; ++dt){
		const int frameIndex = max(0, min(numberOfFrames - 1, currentFrameIndex + dt));

		//キャッシュに既に含まれている場合
		if (sourceCacheMap.count(frameIndex)){
			if (dt == 0){
				currentFrameCacheIndex = sourceCacheMap[frameIndex];
			}
			continue;
		}

		//キャッシュの空きがない場合
		if (sourceCacheUnusedIndex.empty()){
			const int removeFrameIndex = sourceCacheMap.begin()->first;
			const int cacheIndex = sourceCacheMap.begin()->second;
			sourceCacheUnusedIndex.push_back(cacheIndex);
			sourceCacheMap.erase(removeFrameIndex);
		}

		const int cacheIndex = sourceCacheUnusedIndex.front();
		sourceCacheUnusedIndex.pop_front();
		sourceCacheMap[frameIndex] = cacheIndex;

		//キャッシュにフレームデータをコピーする
		const PIXEL_YC* source = fp.exfunc->get_ycp_filtering_cache_ex(&fp, fpip.editp, frameIndex, NULL, NULL);
		const int sizeOfSourceDeviceMemory = width * height * sizeof(short3);
		if ((error = cudaMemcpy(deviceSource + width * height * cacheIndex, source, sizeOfSourceDeviceMemory, cudaMemcpyHostToDevice)) != cudaSuccess){
			LOG.write("ProcessorCuda::proc() - 入力データの転送に失敗しました");
			LOG.write(cudaGetErrorString(error));
			release();
			return FALSE;
		}

		if (dt == 0){
			currentFrameCacheIndex = cacheIndex;
		}
	}

	const float H = powf(10.0f, (float)fp.track[2] / 22.0f);
	const float H2 = 1.0f / (H * H);

	//const float H = (float)pow(10, (double)(fp.track[2]-100) / 55.0);
	//const float H2 = (float)1.0 / (H * H);

	const dim3 db = make_uint3(11, 11, 1);
	const dim3 dg = make_uint3((width - 1) / db.x + 1, (height - 1) / db.y + 1, 1);

	cudaMemcpyToSymbol("constantWidth", &width, sizeof(constantWidth));
	cudaMemcpyToSymbol("constantHeight", &height, sizeof(constantHeight));
	cudaMemcpyToSymbol("constantMaxWidth", &maxWidth, sizeof(constantMaxWidth));
	cudaMemcpyToSymbol("constantMaxHeight", &maxHeight, sizeof(constantMaxHeight));
	cudaMemcpyToSymbol("constantSpaceSearchRadius", &spaceSearchRadius, sizeof(constantSpaceSearchRadius));
	cudaMemcpyToSymbol("constantTimeSearchRadius", &timeSearchRadius, sizeof(constantTimeSearchRadius));
	cudaMemcpyToSymbol("constantCurrentFrameCacheIndex", &currentFrameCacheIndex, sizeof(constantCurrentFrameCacheIndex));
	cudaMemcpyToSymbol("constantH2", &H2, sizeof(constantH2));

	kernelProccess<<<dg, db>>>(deviceSource, deviceDest);

	if ((error = cudaGetLastError()) != cudaSuccess){
		LOG.write("ProcessorCuda::proc() - 処理に失敗しました");
		LOG.write(cudaGetErrorString(error));
		release();
		return FALSE;
	}

	if ((error = cudaMemcpy(fpip.ycp_edit, deviceDest, maxWidth * maxHeight * sizeof(short3), cudaMemcpyDeviceToHost)) != cudaSuccess){
		LOG.write("ProcessorCuda::proc() - 結果のコピーに失敗しました");
		LOG.write(cudaGetErrorString(error));
		release();
		return FALSE;
	}

	return TRUE;
}

BOOL ProcessorCuda::wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	return DefWindowProc( hwnd, message, wparam, lparam );
}

bool ProcessorCuda::prepareDevice(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	const int width = fpip.w;
	const int height = fpip.h;
	const int maxWidth = fpip.max_w;
	const int maxHeight = fpip.max_h;
	const int numberOfFrames = fpip.frame_n;

	const int spaceRadius = fp.track[0];
	const int timeRadius = fp.track[1];
	const int timeDiameter = timeRadius * 2 + 1;
	const int cacheSize = timeDiameter + 2;

	if (lastWidth == width && lastHeight == height && lastNumberOfFrames == numberOfFrames &&
		lastSpaceRadius == spaceRadius && lastTimeRadius == timeRadius && 
		deviceDest && deviceSource){
		return true;
	}

	//入力用デバイスメモリの準備
	if (!prepareDeviceSource(width, height, cacheSize)){
		return false;
	}

	//出力用デバイスメモリの再確保
	if (!prepareDeviceDest(maxWidth, maxHeight)){
		release();
		return false;
	}

	sourceCacheMap.clear();
	sourceCacheUnusedIndex.clear();

	for (int cacheIndex = 0; cacheIndex < cacheSize; ++cacheIndex){
		sourceCacheUnusedIndex.push_back(cacheIndex);
	}

	//キャッシュサイズの設定
	fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, 0, NULL);
	fp.exfunc->set_ycp_filtering_cache_size(&fp, width, height, 2, NULL);

	lastWidth = width;
	lastHeight = height;
	lastNumberOfFrames = numberOfFrames;
	lastSpaceRadius = spaceRadius;
	lastTimeRadius = timeRadius;

	return true;
}

bool ProcessorCuda::prepareDeviceSource(int width, int height, int cacheSize)
{
	if (!releaseDeviceSource()){
		return false;
	}

	cudaError_t error;
	const int mallocSize = width * height * cacheSize * sizeof(short3);
	if ((error = cudaMalloc((void**)&deviceSource, mallocSize)) != cudaSuccess){
		LOG.write("ProcessorCuda::prepareDeviceSource() - 入力用デバイスメモリの確保に失敗しました");
		LOG.write(cudaGetErrorString(error));
		return false;
	}

	if ((error = cudaBindTexture(0, texRef, deviceSource, mallocSize)) != cudaSuccess){
		LOG.write("ProcessorCuda::prepareDeviceSource() - 入力用デバイスメモリのバインドに失敗しました");
		LOG.write(cudaGetErrorString(error));
		return false;
	}

	return true;
}

bool ProcessorCuda::prepareDeviceDest(int width, int height)
{
	if (!releaseDeviceDest()){
		return false;
	}

	cudaError_t error;
	const int mallocSize = width * height * sizeof(short3);
	if ((error = cudaMalloc((void**)&deviceDest, mallocSize)) != cudaSuccess){
		LOG.write("ProcessorCuda::prepareDeviceDest() - 出力用デバイスメモリの確保に失敗しました");
		LOG.write(cudaGetErrorString(error));
		return false;
	}

	return true;
}

bool ProcessorCuda::releaseDeviceSource()
{
	if (deviceSource){
		cudaError_t error;
		if ((error = cudaFree(deviceSource)) != cudaSuccess){
			LOG.write("ProcessorCuda::releaseDeviceSource() - 入力用デバイスメモリの開放に失敗しました");
			LOG.write(cudaGetErrorString(error));
			return false;
		}

		deviceSource = NULL;
	}

	return true;
}

bool ProcessorCuda::releaseDeviceDest()
{
	if (deviceDest){
		cudaError_t error;
		if ((error = cudaFree(deviceDest)) != cudaSuccess){
			LOG.write("ProcessorCuda::releaseDeviceDest() - 出力用デバイスメモリの開放に失敗しました");
			LOG.write(cudaGetErrorString(error));
			return false;
		}

		deviceDest = NULL;
	}

	return true;
}

__device__ short3 kernelGetPixel(int x, int y, int frameIndex, const short3* in)
{
	x = max(0, min(constantWidth - 1, x));
	y = max(0, min(constantHeight - 1, y));
	return in[(frameIndex * constantHeight + y) * constantWidth + x];
	//const int offset = ((frameIndex * constantHeight + y) * constantWidth + x) * 3;
	//const short r = tex1Dfetch(texRef, offset + 0);
	//const short g = tex1Dfetch(texRef, offset + 1);
	//const short b = tex1Dfetch(texRef, offset + 2);
	//return make_short3(r, g, b);
}

__global__ void kernelProccess(short3* in, short3* out)
{
	const int spaceRadius = constantSpaceSearchRadius;
	const int offsetX = blockIdx.x * blockDim.x;
	const int offsetY = blockIdx.y * blockDim.y;
	const int sourceX = offsetX + threadIdx.x;
	const int sourceY = offsetY + threadIdx.y;

	const int timeSearchDiameter = constantTimeSearchRadius * 2 + 1;

	__shared__ short3 sharedPixel[32][32];
	short3 pixel[3][3];
	for (int x = 0; x < 3; ++x){
		for (int y = 0; y < 3; ++y){
			pixel[x][y] = kernelGetPixel(sourceX + x - 1, sourceY + y - 1, constantCurrentFrameCacheIndex, in);
		}
	}
	
	float3 sum = make_float3(0, 0, 0);
	float3 value = make_float3(0, 0, 0);
	for (int dt = 0; dt < timeSearchDiameter; ++dt){

		//フレームデータをシェアードメモリに乗せる
		for (int sx = threadIdx.x; sx < blockDim.x + spaceRadius * 2 + 2; sx += blockDim.x){
			for (int sy = threadIdx.y; sy < blockDim.y + spaceRadius * 2 + 2; sy += blockDim.y){
				sharedPixel[sx][sy] = kernelGetPixel(offsetX + sx - spaceRadius - 1, offsetY + sy - spaceRadius - 1, dt, in);
			}
		}

		__syncthreads();

		if (sourceX < constantWidth || sourceY < constantHeight){
			for (int dx = -spaceRadius; dx <= spaceRadius; ++dx){
				for (int dy = -spaceRadius; dy <= spaceRadius; ++dy){
					float3 sum2 = make_float3(0, 0, 0);
					for (int xx = -1; xx <= 1; ++xx){
						for (int yy = -1; yy <= 1; ++yy){
							const short3 source = pixel[xx + 1][yy + 1];
							const short3 dest = sharedPixel[dx + spaceRadius + xx + 1 + threadIdx.x][dy + spaceRadius + yy + 1 + threadIdx.y];

							float3 diff = make_float3((float)source.x - dest.x, (float)source.y - dest.y, (float)source.z - dest.z);
							diff.x *= diff.x;
							diff.y *= diff.y;
							diff.z *= diff.z;
							sum2.x += diff.x;
							sum2.y += diff.y;
							sum2.z += diff.z;
						}
					}

					const float3 w = make_float3(__expf(-sum2.x * constantH2), __expf(-sum2.y * constantH2), __expf(-sum2.z * constantH2));
					sum.x += w.x;
					sum.y += w.y;
					sum.z += w.z;

					const short3 destPixel = sharedPixel[dx + spaceRadius + threadIdx.x + 1][dy + spaceRadius + threadIdx.y + 1];
					value.x += w.x * destPixel.x;
					value.y += w.y * destPixel.y;
					value.z += w.z * destPixel.z;
				}
			}
		}

		__syncthreads();
	}

	value.x /= sum.x;
	value.y /= sum.y;
	value.z /= sum.z;
	out[sourceX + constantMaxWidth * sourceY] = make_short3((short)value.x, (short)value.y, (short)value.z);
}
