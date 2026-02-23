// DirectX 12 PoC の最小単一フレーム経路を GoogleTest で検証する。
#include <gtest/gtest.h>
#include <vector>
#include "../exedit2/Dx12PocProcessor.h"

TEST(Dx12PocProcessorTests, ReturnsFalseWhenPocIsNotEnabled)
{
	Dx12PocProbeResult probe = {};
	probe.enabled = false;

	std::uint32_t src[4] = { 1, 2, 3, 4 };
	std::uint32_t dst[4] = {};
	EXPECT_FALSE(process_dx12_poc_single_frame(src, dst, 2, 2, true, probe));
}

TEST(Dx12PocProcessorTests, ReturnsFalseOnInvalidArguments)
{
	Dx12PocProbeResult probe = {};
	probe.enabled = true;

	std::uint32_t src[4] = { 1, 2, 3, 4 };
	std::uint32_t dst[4] = {};
	EXPECT_FALSE(process_dx12_poc_single_frame(nullptr, dst, 2, 2, true, probe));
	EXPECT_FALSE(process_dx12_poc_single_frame(src, nullptr, 2, 2, true, probe));
	EXPECT_FALSE(process_dx12_poc_single_frame(src, dst, 0, 2, true, probe));
	EXPECT_FALSE(process_dx12_poc_single_frame(src, dst, 2, 0, true, probe));
}

TEST(Dx12PocProcessorTests, CopiesPixelsWhenEnabled)
{
	Dx12PocProbeResult probe = {};
	probe.enabled = true;

	std::vector<std::uint32_t> src = { 0x01020304u, 0xA0B0C0D0u, 0x10203040u, 0xFFEEDDCCu };
	std::vector<std::uint32_t> dst(src.size(), 0u);

	EXPECT_TRUE(process_dx12_poc_single_frame(src.data(), dst.data(), 2, 2, true, probe));
	EXPECT_EQ(dst, src);
}
