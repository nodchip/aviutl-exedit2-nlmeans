// DirectX 12 PoC 向け最小単一フレーム処理。
#ifndef EXEDIT2_DX12_POC_PROCESSOR_H
#define EXEDIT2_DX12_POC_PROCESSOR_H

#include <cstddef>
#include <cstdint>
#include <cstring>
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

#endif
