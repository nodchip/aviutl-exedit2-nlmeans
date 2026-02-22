// CPU キャッシュ更新判定の回帰テスト。
#include <cstdlib>
#include "CacheSizing.h"

static int run_tests()
{
	// 全条件が一致すれば更新不要。
	if (needs_cache_reset(1920, 1080, 300, 5, 1920, 1080, 300, 5)) {
		return 1;
	}

	// 総フレーム数が変わったら更新が必要。
	if (!needs_cache_reset(1920, 1080, 300, 5, 1920, 1080, 301, 5)) {
		return 2;
	}

	// キャッシュサイズが変わったら更新が必要。
	if (!needs_cache_reset(1920, 1080, 300, 5, 1920, 1080, 300, 7)) {
		return 3;
	}

	// 解像度が変わったら更新が必要。
	if (!needs_cache_reset(1920, 1080, 300, 5, 1280, 720, 300, 5)) {
		return 4;
	}

	return 0;
}

int main()
{
	return run_tests();
}
