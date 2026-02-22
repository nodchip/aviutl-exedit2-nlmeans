// ExEdit2 の複数 GPU 協調向けタイル分割ロジック。
#ifndef EXEDIT2_MULTI_GPU_TILING_H
#define EXEDIT2_MULTI_GPU_TILING_H

#include <algorithm>
#include <cstddef>
#include <vector>

// 1 アダプタへ割り当てる行タイル情報。
struct GpuRowTile
{
	size_t adapterIndex;
	int yBegin;
	int yEnd;
};

// 行単位で画像を分割し、各 GPU へ連続領域を割り当てる。
inline std::vector<GpuRowTile> plan_gpu_row_tiles(int height, size_t adapterCount, int minRowsPerTile = 1)
{
	std::vector<GpuRowTile> tiles;
	if (height <= 0 || adapterCount == 0) {
		return tiles;
	}

	const int clampedMinRows = std::max(1, minRowsPerTile);
	const int maxTileCountByMinRows = std::max(1, height / clampedMinRows);
	const size_t tileCount = std::min(adapterCount, static_cast<size_t>(maxTileCountByMinRows));
	if (tileCount == 0) {
		return tiles;
	}

	tiles.reserve(tileCount);
	const int baseRows = height / static_cast<int>(tileCount);
	const int remainder = height % static_cast<int>(tileCount);
	int y = 0;
	for (size_t i = 0; i < tileCount; ++i) {
		const int rows = baseRows + (static_cast<int>(i) < remainder ? 1 : 0);
		const int yBegin = y;
		const int yEnd = y + rows;
		tiles.push_back({ i, yBegin, yEnd });
		y = yEnd;
	}
	return tiles;
}

#endif
