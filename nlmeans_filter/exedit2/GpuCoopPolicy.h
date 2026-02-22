// ExEdit2 の複数 GPU 協調有効化ポリシー。
#ifndef EXEDIT2_GPU_COOP_POLICY_H
#define EXEDIT2_GPU_COOP_POLICY_H

#include <algorithm>
#include <cstddef>

// 複数 GPU 協調を有効にするべきかを判定する。
inline bool should_enable_multi_gpu(int adapterOrdinal, size_t hardwareCount, int requestedCoopCount)
{
	if (adapterOrdinal >= 0) {
		return false;
	}
	if (hardwareCount <= 1) {
		return false;
	}
	return requestedCoopCount > 1;
}

// 有効な協調 GPU 数を返す（最小 1）。
inline size_t resolve_active_gpu_count(size_t hardwareCount, int requestedCoopCount)
{
	const size_t requested = static_cast<size_t>(std::max(1, requestedCoopCount));
	if (hardwareCount == 0) {
		return 1;
	}
	return std::max<size_t>(1, std::min(hardwareCount, requested));
}

#endif
