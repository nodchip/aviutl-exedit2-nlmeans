// Copyright 2026 nodchip
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

#ifndef CACHE_SIZING_H
#define CACHE_SIZING_H

// キャッシュ済み条件と要求条件を比較し、再初期化が必要かを判定する。
inline bool needs_cache_reset(
	int cachedWidth,
	int cachedHeight,
	int cachedTotalFrames,
	int cachedCacheSize,
	int requestedWidth,
	int requestedHeight,
	int requestedTotalFrames,
	int requestedCacheSize)
{
	return cachedWidth != requestedWidth ||
		cachedHeight != requestedHeight ||
		cachedTotalFrames != requestedTotalFrames ||
		cachedCacheSize != requestedCacheSize;
}

#endif
