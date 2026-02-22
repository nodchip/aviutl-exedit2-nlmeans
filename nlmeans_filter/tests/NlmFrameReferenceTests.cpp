// フレーム単位の NLM 基礎演算で Naive と AVX2 の一致を確認する。
#include <vector>
#include "NlmFrameKernel.h"

int main()
{
	const int width = 8;
	const int height = 8;
	const int frames = 3;
	const int searchRadius = 1;
	const int timeRadius = 1;
	const double h2 = 1.0 / (25.0 * 25.0);

	std::vector<short> input(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(frames) * 3u);
	for (size_t i = 0; i < input.size(); ++i) {
		input[i] = static_cast<short>((i * 17 + 13) % 1024 - 512);
	}

	const int x = 3;
	const int y = 4;
	const int channel = 0;
	const int currentFrame = 1;

	const int naive = nlm_filter_pixel_channel_naive(
		input.data(),
		width,
		height,
		frames,
		x,
		y,
		channel,
		currentFrame,
		searchRadius,
		timeRadius,
		h2);

	if (is_avx2_supported_runtime()) {
		const int avx2 = nlm_filter_pixel_channel_avx2(
			input.data(),
			width,
			height,
			frames,
			x,
			y,
			channel,
			currentFrame,
			searchRadius,
			timeRadius,
			h2);
		if (naive != avx2) {
			return 1;
		}
	}

	return 0;
}
