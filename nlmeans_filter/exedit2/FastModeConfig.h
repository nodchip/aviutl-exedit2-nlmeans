// ExEdit2 の CPU Fast モード設定解決ロジック。
#ifndef EXEDIT2_FAST_MODE_CONFIG_H
#define EXEDIT2_FAST_MODE_CONFIG_H

#include <algorithm>

// UI 値から Fast モードの空間間引きステップを決定する。
inline int resolve_fast_spatial_step(int requestedValue)
{
	return std::clamp(requestedValue, 1, 4);
}

#endif
