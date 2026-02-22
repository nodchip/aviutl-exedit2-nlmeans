// フレーム全体出力で Naive と AVX2 の一致を確認する。
#include <vector>
#include "NlmFrameKernel.h"

int main()
{
	const int width = 16;
	const int height = 12;
	const int frames = 3;
	const int currentFrame = 1;
	const int searchRadius = 1;
	const int timeRadius = 1;
	const double h2 = 1.0 / (25.0 * 25.0);

	std::vector<short> input(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(frames) * 3u);
	for (size_t i = 0; i < input.size(); ++i) {
		input[i] = static_cast<short>((i * 97 + 23) % 1024 - 512);
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

	if (!is_avx2_supported_runtime()) {
		return 0;
	}

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
			return 1;
		}
	}

	return 0;
}
