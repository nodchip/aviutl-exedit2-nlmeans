// DirectX 12 PoC 向け最小単一フレーム処理。
#ifndef EXEDIT2_DX12_POC_PROCESSOR_H
#define EXEDIT2_DX12_POC_PROCESSOR_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "Dx12PocBackend.h"

// DX12 PoC の最小経路として 1 フレーム分を転送する。
// 現段階では計算処理を持たず、実行経路の成立確認を目的とする。
inline bool process_dx12_poc_single_frame(
	const std::uint32_t* inputPixels,
	std::uint32_t* outputPixels,
	int width,
	int height,
	bool userRequestedDx12Poc,
	const Dx12PocProbeResult& probe)
{
	if (!can_enable_dx12_poc(userRequestedDx12Poc, probe)) {
		return false;
	}
	if (inputPixels == nullptr || outputPixels == nullptr || width <= 0 || height <= 0) {
		return false;
	}

	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	std::memcpy(outputPixels, inputPixels, pixelCount * sizeof(std::uint32_t));
	return true;
}

// DX12 PoC の最小コンピュート相当処理。
// 3x3 の近傍平均を取り、copy path よりも計算経路に近い出力を作る。
inline bool process_dx12_poc_compute_path(
	const std::uint32_t* inputPixels,
	std::uint32_t* outputPixels,
	int width,
	int height,
	bool userRequestedDx12Poc,
	const Dx12PocProbeResult& probe)
{
	if (!can_enable_dx12_poc(userRequestedDx12Poc, probe)) {
		return false;
	}
	if (inputPixels == nullptr || outputPixels == nullptr || width <= 0 || height <= 0) {
		return false;
	}

	const auto idx = [width](int x, int y) -> size_t {
		return static_cast<size_t>(y * width + x);
	};
	const auto unpack = [](std::uint32_t p, int shift) -> int {
		return static_cast<int>((p >> shift) & 0xffu);
	};

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int sumR = 0;
			int sumG = 0;
			int sumB = 0;
			int count = 0;
			for (int dy = -1; dy <= 1; ++dy) {
				const int sy = std::clamp(y + dy, 0, height - 1);
				for (int dx = -1; dx <= 1; ++dx) {
					const int sx = std::clamp(x + dx, 0, width - 1);
					const std::uint32_t sp = inputPixels[idx(sx, sy)];
					sumR += unpack(sp, 0);
					sumG += unpack(sp, 8);
					sumB += unpack(sp, 16);
					++count;
				}
			}
			const std::uint32_t center = inputPixels[idx(x, y)];
			const std::uint32_t a = (center >> 24) & 0xffu;
			const std::uint32_t r = static_cast<std::uint32_t>(sumR / count) & 0xffu;
			const std::uint32_t g = static_cast<std::uint32_t>(sumG / count) & 0xffu;
			const std::uint32_t b = static_cast<std::uint32_t>(sumB / count) & 0xffu;
			outputPixels[idx(x, y)] = (a << 24) | (b << 16) | (g << 8) | r;
		}
	}
	return true;
}

#endif
