// ExEdit2 の複数 GPU 出力合成ロジック。
#ifndef EXEDIT2_MULTI_GPU_COMPOSE_H
#define EXEDIT2_MULTI_GPU_COMPOSE_H

#include <cstddef>
#include <cstring>
#include <vector>
#include "MultiGpuTiling.h"

// 行タイル割り当てに基づき、GPUごとの出力から最終画像を合成する。
template <typename PixelT>
inline bool compose_row_tiled_output(
	int width,
	int height,
	const std::vector<GpuRowTile>& tiles,
	const std::vector<std::vector<PixelT>>& perAdapterOutputs,
	std::vector<PixelT>* outImage)
{
	if (outImage == nullptr || width <= 0 || height <= 0) {
		return false;
	}
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	if (outImage->size() != pixelCount) {
		outImage->resize(pixelCount);
	}

	for (const auto& tile : tiles) {
		if (tile.adapterIndex >= perAdapterOutputs.size()) {
			return false;
		}
		const std::vector<PixelT>& src = perAdapterOutputs[tile.adapterIndex];
		if (src.size() != pixelCount) {
			return false;
		}
		if (tile.yBegin < 0 || tile.yEnd < tile.yBegin || tile.yEnd > height) {
			return false;
		}

		for (int y = tile.yBegin; y < tile.yEnd; ++y) {
			const size_t rowBegin = static_cast<size_t>(y * width);
			const size_t rowBytes = static_cast<size_t>(width) * sizeof(PixelT);
			std::memcpy(
				outImage->data() + rowBegin,
				src.data() + rowBegin,
				rowBytes);
		}
	}
	return true;
}

#endif
