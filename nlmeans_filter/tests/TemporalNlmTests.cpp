// Temporal NLM 亜種実装を GoogleTest で検証する。
#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include "../NlmFrameKernel.h"
#include "../algorithms/TemporalNlm.h"

namespace {

std::vector<short> make_temporal_input(int width, int height, int frameCount)
{
	std::vector<short> input(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(frameCount) * 3, 0);
	for (int t = 0; t < frameCount; ++t) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				for (int c = 0; c < 3; ++c) {
					const size_t idx = static_cast<size_t>(((t * height + y) * width + x) * 3 + c);
					input[idx] = static_cast<short>(120 + ((x + y + c) % 7));
				}
			}
		}
	}
	return input;
}

}

TEST(TemporalNlmTests, ZeroDecayMatchesBaseNaive)
{
	const int width = 6;
	const int height = 5;
	const int frameCount = 3;
	const int currentFrame = 1;
	const int searchRadius = 1;
	const int timeRadius = 1;
	const double h2 = 0.0002;

	const std::vector<short> input = make_temporal_input(width, height, frameCount);
	std::vector<short> base(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);
	std::vector<short> temporal(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);

	nlm_filter_frame_naive(
		input.data(),
		width,
		height,
		frameCount,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		base.data());
	nlm_filter_frame_temporal_naive(
		input.data(),
		width,
		height,
		frameCount,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		0.0,
		temporal.data());

	EXPECT_EQ(temporal, base);
}

TEST(TemporalNlmTests, HigherDecayKeepsOutputCloserToCurrentFrame)
{
	const int width = 5;
	const int height = 5;
	const int frameCount = 3;
	const int currentFrame = 2;
	const int searchRadius = 0;
	const int timeRadius = 2;
	const double h2 = 0.00001;

	std::vector<short> input = make_temporal_input(width, height, frameCount);

	// 過去フレームに大きな外れ値を入れる。
	const int cx = 2;
	const int cy = 2;
	for (int c = 0; c < 3; ++c) {
		const size_t noisy = static_cast<size_t>(((0 * height + cy) * width + cx) * 3 + c);
		const size_t current = static_cast<size_t>(((currentFrame * height + cy) * width + cx) * 3 + c);
		input[noisy] = static_cast<short>(255);
		input[current] = static_cast<short>(64);
	}

	std::vector<short> lowDecay(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);
	std::vector<short> highDecay(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);

	nlm_filter_frame_temporal_naive(
		input.data(),
		width,
		height,
		frameCount,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		0.0,
		lowDecay.data());
	nlm_filter_frame_temporal_naive(
		input.data(),
		width,
		height,
		frameCount,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		2.0,
		highDecay.data());

	const size_t center = static_cast<size_t>((cy * width + cx) * 3);
	const int currentValue = 64;
	const int lowDiff = std::abs(static_cast<int>(lowDecay[center]) - currentValue);
	const int highDiff = std::abs(static_cast<int>(highDecay[center]) - currentValue);
	EXPECT_LE(highDiff, lowDiff);
}
