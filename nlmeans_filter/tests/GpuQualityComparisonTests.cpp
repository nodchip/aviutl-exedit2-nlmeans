// GPU 出力と CPU 参照出力の誤差を評価する。
#include <gtest/gtest.h>

#if __has_include("../aviutl2_sdk/filter2.h")

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../exedit2/Exedit2GpuRunner.h"

namespace {

struct QualityStats
{
	int maxAbsDiff;
	double meanAbsDiff;
};

struct QualityCase
{
	int width;
	int height;
	int searchRadius;
	int timeRadius;
	double sigma;
	int spatialStep;
	double temporalDecay;
	int seed0;
	int seed1;
	int maxAbsTolerance;
	double meanAbsTolerance;
};

std::vector<PIXEL_RGBA> make_frame(int width, int height, int seed)
{
	const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
	std::vector<PIXEL_RGBA> frame(pixel_count);
	for (size_t i = 0; i < pixel_count; ++i) {
		PIXEL_RGBA p = {};
		const int base = static_cast<int>((i * 31 + static_cast<size_t>(seed) * 17) & 255);
		p.r = static_cast<unsigned char>(base);
		p.g = static_cast<unsigned char>((base * 3 + 19) & 255);
		p.b = static_cast<unsigned char>((base * 5 + 7) & 255);
		p.a = 255;
		frame[i] = p;
	}
	return frame;
}

inline int clampi(int value, int low, int high)
{
	return std::min(high, std::max(low, value));
}

// nlm.psh と同じ 3x3 ガウシアン係数。
inline float patch_weight_3x3(int index)
{
	static const float kWeights[9] = {
		0.07f, 0.12f, 0.07f,
		0.12f, 0.20f, 0.12f,
		0.07f, 0.12f, 0.07f
	};
	return kWeights[index];
}

// Exedit2GpuRunner のシェーダー実装と同じ式で CPU 参照出力を作る。
std::vector<PIXEL_RGBA> build_cpu_reference(
	const std::vector<std::vector<PIXEL_RGBA>>& history_frames,
	int width,
	int height,
	int search_radius,
	double sigma,
	int spatial_step,
	double temporal_decay)
{
	const int frame_count = static_cast<int>(history_frames.size());
	const int current_frame_index = frame_count - 1;
	const float inv_sigma = static_cast<float>(1.0 / std::max(0.001, sigma));
	const int step = std::max(1, spatial_step);
	const float temporal_decay_value = static_cast<float>(std::max(0.0, temporal_decay));
	const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
	std::vector<PIXEL_RGBA> out(pixel_count);

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const PIXEL_RGBA& center = history_frames[static_cast<size_t>(current_frame_index)][static_cast<size_t>(y * width + x)];
			float sum_w = 0.0f;
			float sum_r = 0.0f;
			float sum_g = 0.0f;
			float sum_b = 0.0f;

			std::array<PIXEL_RGBA, 9> current_patch_rgb{};
			int patch_index = 0;
			for (int py = -1; py <= 1; ++py) {
				for (int px = -1; px <= 1; ++px) {
					const int cx = clampi(x + px, 0, width - 1);
					const int cy = clampi(y + py, 0, height - 1);
					current_patch_rgb[static_cast<size_t>(patch_index)] =
						history_frames[static_cast<size_t>(current_frame_index)][static_cast<size_t>(cy * width + cx)];
					++patch_index;
				}
			}

			for (int t = 0; t < frame_count; ++t) {
				const int dt = std::abs(t - current_frame_index);
				const float temporal_weight = std::exp(-temporal_decay_value * static_cast<float>(dt));
				for (int dy = -search_radius; dy <= search_radius; dy += step) {
					const int sy = clampi(y + dy, 0, height - 1);
					for (int dx = -search_radius; dx <= search_radius; dx += step) {
						const int sx = clampi(x + dx, 0, width - 1);
						float dist2 = 0.0f;
						int idx = 0;
						for (int py = -1; py <= 1; ++py) {
							for (int px = -1; px <= 1; ++px) {
								const int tx = clampi(sx + px, 0, width - 1);
								const int ty = clampi(sy + py, 0, height - 1);
								const PIXEL_RGBA& sample_patch =
									history_frames[static_cast<size_t>(t)][static_cast<size_t>(ty * width + tx)];
								const PIXEL_RGBA& current_patch_pixel = current_patch_rgb[static_cast<size_t>(idx)];
								const float dr = static_cast<float>(sample_patch.r) - static_cast<float>(current_patch_pixel.r);
								const float dg = static_cast<float>(sample_patch.g) - static_cast<float>(current_patch_pixel.g);
								const float db = static_cast<float>(sample_patch.b) - static_cast<float>(current_patch_pixel.b);
								dist2 += (dr * dr + dg * dg + db * db) * patch_weight_3x3(idx);
								++idx;
							}
						}
						const float w = std::exp(-std::sqrt(dist2) * inv_sigma) * temporal_weight;
						const PIXEL_RGBA& sample =
							history_frames[static_cast<size_t>(t)][static_cast<size_t>(sy * width + sx)];
						sum_w += w;
						sum_r += w * static_cast<float>(sample.r);
						sum_g += w * static_cast<float>(sample.g);
						sum_b += w * static_cast<float>(sample.b);
					}
				}
			}

			PIXEL_RGBA result = center;
			if (sum_w > 0.0f) {
				result.r = static_cast<unsigned char>(std::clamp(sum_r / sum_w, 0.0f, 255.0f));
				result.g = static_cast<unsigned char>(std::clamp(sum_g / sum_w, 0.0f, 255.0f));
				result.b = static_cast<unsigned char>(std::clamp(sum_b / sum_w, 0.0f, 255.0f));
			}
			out[static_cast<size_t>(y * width + x)] = result;
		}
	}

	return out;
}

QualityStats evaluate_quality(const std::vector<PIXEL_RGBA>& gpu, const std::vector<PIXEL_RGBA>& cpu)
{
	const size_t pixel_count = gpu.size();
	long long total_abs = 0;
	int max_abs = 0;

	for (size_t i = 0; i < pixel_count; ++i) {
		const int dr = std::abs(static_cast<int>(gpu[i].r) - static_cast<int>(cpu[i].r));
		const int dg = std::abs(static_cast<int>(gpu[i].g) - static_cast<int>(cpu[i].g));
		const int db = std::abs(static_cast<int>(gpu[i].b) - static_cast<int>(cpu[i].b));
		max_abs = std::max(max_abs, std::max(dr, std::max(dg, db)));
		total_abs += static_cast<long long>(dr + dg + db);
	}

	const double denom = static_cast<double>(pixel_count) * 3.0;
	return { max_abs, denom > 0.0 ? (static_cast<double>(total_abs) / denom) : 0.0 };
}

void run_quality_case(const QualityCase& test_case)
{
	Exedit2GpuRunner runner;
	if (!runner.initialize(-1)) {
		GTEST_SKIP() << "GPU runner initialization failed.";
	}

	const std::vector<PIXEL_RGBA> frame0 = make_frame(test_case.width, test_case.height, test_case.seed0);
	std::vector<PIXEL_RGBA> gpu_out0(static_cast<size_t>(test_case.width) * static_cast<size_t>(test_case.height));

	ASSERT_TRUE(runner.process(
		frame0.data(),
		gpu_out0.data(),
		test_case.width,
		test_case.height,
		test_case.searchRadius,
		test_case.timeRadius,
		test_case.sigma,
		test_case.spatialStep,
		test_case.temporalDecay));

	if (test_case.timeRadius <= 0) {
		const std::vector<std::vector<PIXEL_RGBA>> history = { frame0 };
		const std::vector<PIXEL_RGBA> cpu_ref = build_cpu_reference(
			history,
			test_case.width,
			test_case.height,
			test_case.searchRadius,
			test_case.sigma,
			test_case.spatialStep,
			test_case.temporalDecay);
		const QualityStats stats = evaluate_quality(gpu_out0, cpu_ref);
		EXPECT_LE(stats.maxAbsDiff, test_case.maxAbsTolerance);
		EXPECT_LE(stats.meanAbsDiff, test_case.meanAbsTolerance);
		return;
	}

	const std::vector<PIXEL_RGBA> frame1 = make_frame(test_case.width, test_case.height, test_case.seed1);
	std::vector<PIXEL_RGBA> gpu_out1(static_cast<size_t>(test_case.width) * static_cast<size_t>(test_case.height));
	ASSERT_TRUE(runner.process(
		frame1.data(),
		gpu_out1.data(),
		test_case.width,
		test_case.height,
		test_case.searchRadius,
		test_case.timeRadius,
		test_case.sigma,
		test_case.spatialStep,
		test_case.temporalDecay));

	const std::vector<std::vector<PIXEL_RGBA>> history = { frame0, frame1 };
	const std::vector<PIXEL_RGBA> cpu_ref = build_cpu_reference(
		history,
		test_case.width,
		test_case.height,
		test_case.searchRadius,
		test_case.sigma,
		test_case.spatialStep,
		test_case.temporalDecay);
	const QualityStats stats = evaluate_quality(gpu_out1, cpu_ref);
	EXPECT_LE(stats.maxAbsDiff, test_case.maxAbsTolerance);
	EXPECT_LE(stats.meanAbsDiff, test_case.meanAbsTolerance);
}

}

TEST(GpuQualityComparisonTests, MatchesCpuReferenceAcrossQualityMatrix)
{
	const std::vector<QualityCase> cases = {
		// Single-frame spatial quality.
		{ 32, 24, 1, 0, 40.0, 1, 0.0, 1, 2, 1, 0.30 },
		{ 48, 36, 2, 0, 55.0, 1, 0.0, 3, 4, 1, 0.35 },
		{ 48, 36, 2, 0, 55.0, 2, 0.0, 5, 6, 1, 0.45 },
		// Temporal quality (2-frame history).
		{ 32, 24, 1, 1, 45.0, 1, 0.0, 10, 20, 1, 0.30 },
		{ 48, 36, 2, 1, 60.0, 1, 1.0, 30, 40, 1, 0.40 },
		{ 48, 36, 2, 1, 60.0, 2, 1.0, 50, 60, 1, 0.55 }
	};

	for (const auto& test_case : cases) {
		run_quality_case(test_case);
	}
}

#else

TEST(GpuQualityComparisonTests, SkippedWhenSdkIsUnavailable)
{
	GTEST_SKIP() << "aviutl2_sdk/filter2.h not found.";
}

#endif
