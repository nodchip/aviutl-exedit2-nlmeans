// Fast NLM 亜種実装を GoogleTest で検証する。
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include "../NlmFrameKernel.h"
#include "../algorithms/FastNlm.h"

namespace {

std::vector<short> make_input(int width, int height, int frameCount)
{
	std::vector<short> input(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(frameCount) * 3);
	for (size_t i = 0; i < input.size(); ++i) {
		input[i] = static_cast<short>((i * 13 + 17) % 256);
	}
	return input;
}

}

TEST(FastNlmTests, Step1MatchesNaiveFrameOutput)
{
	const int width = 6;
	const int height = 5;
	const int frameCount = 3;
	const int currentFrame = 1;
	const int searchRadius = 1;
	const int timeRadius = 1;
	const double h2 = 0.0002;

	const std::vector<short> input = make_input(width, height, frameCount);
	std::vector<short> naive(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);
	std::vector<short> fast(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);

	nlm_filter_frame_naive(
		input.data(),
		width,
		height,
		frameCount,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		naive.data());
	nlm_filter_frame_fast_naive(
		input.data(),
		width,
		height,
		frameCount,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		1,
		fast.data());

	EXPECT_EQ(fast, naive);
}

TEST(FastNlmTests, Step2OutputRemainsInInputRange)
{
	const int width = 8;
	const int height = 6;
	const int frameCount = 3;
	const int currentFrame = 1;
	const int searchRadius = 2;
	const int timeRadius = 1;
	const double h2 = 0.00015;

	const std::vector<short> input = make_input(width, height, frameCount);
	std::vector<short> fast(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);
	nlm_filter_frame_fast_naive(
		input.data(),
		width,
		height,
		frameCount,
		currentFrame,
		searchRadius,
		timeRadius,
		h2,
		2,
		fast.data());

	const auto [minIt, maxIt] = std::minmax_element(input.begin(), input.end());
	ASSERT_NE(minIt, input.end());
	ASSERT_NE(maxIt, input.end());
	for (short v : fast) {
		EXPECT_GE(v, *minIt);
		EXPECT_LE(v, *maxIt);
	}
}
