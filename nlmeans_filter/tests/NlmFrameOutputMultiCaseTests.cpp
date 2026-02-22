// 複数条件でフレーム全体の Naive / AVX2 一致を確認する。
#include <vector>
#include "NlmFrameKernel.h"

static bool run_case(int width, int height, int frames, int searchRadius, int timeRadius, int seed)
{
	const int currentFrame = frames / 2;
	const double h2 = 1.0 / (30.0 * 30.0);

	std::vector<short> input(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(frames) * 3u);
	for (size_t i = 0; i < input.size(); ++i) {
		input[i] = static_cast<short>((static_cast<int>(i) * 53 + seed * 17 + 11) % 1024 - 512);
	}

	std::vector<short> naive(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u);
	std::vector<short> avx2(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u);

	nlm_filter_frame_naive(
		input.data(),
		width,
		height,
		frames,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		naive.data());

	nlm_filter_frame_avx2(
		input.data(),
		width,
		height,
		frames,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		avx2.data());

	for (size_t i = 0; i < naive.size(); ++i) {
		if (naive[i] != avx2[i]) {
			return false;
		}
	}
	return true;
}

int main()
{
	if (!is_avx2_supported_runtime()) {
		return 0;
	}

	if (!run_case(8, 8, 3, 1, 1, 1)) {
		return 1;
	}
	if (!run_case(12, 10, 3, 1, 1, 2)) {
		return 2;
	}
	if (!run_case(16, 12, 5, 2, 2, 3)) {
		return 3;
	}
	return 0;
}
