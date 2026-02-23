// DirectX 12 PoC の最小単一フレーム経路を GoogleTest で検証する。
#include <gtest/gtest.h>
#include <vector>
#include "../exedit2/Dx12PocProcessor.h"

namespace {

HMODULE WINAPI load_library_missing(LPCWSTR)
{
	return nullptr;
}

HMODULE WINAPI load_library_fake(LPCWSTR)
{
	return reinterpret_cast<HMODULE>(0x1);
}

FARPROC WINAPI get_proc_missing(HMODULE, LPCSTR)
{
	return nullptr;
}

BOOL WINAPI free_library_dummy(HMODULE)
{
	return TRUE;
}

}

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

TEST(Dx12PocProcessorTests, ComputePathReturnsFalseWhenPocIsNotEnabled)
{
	Dx12PocProbeResult probe = {};
	probe.enabled = false;

	std::uint32_t src[4] = { 1, 2, 3, 4 };
	std::uint32_t dst[4] = {};
	EXPECT_FALSE(process_dx12_poc_compute_path(src, dst, 2, 2, true, probe));
}

TEST(Dx12PocProcessorTests, ComputePathSmoothsPixelsWhenEnabled)
{
	Dx12PocProbeResult probe = {};
	probe.enabled = true;

	std::vector<std::uint32_t> src = {
		0xFF000000u, 0xFFFFFFFFu, 0xFF000000u,
		0xFFFFFFFFu, 0xFF000000u, 0xFFFFFFFFu,
		0xFF000000u, 0xFFFFFFFFu, 0xFF000000u
	};
	std::vector<std::uint32_t> dst(src.size(), 0u);

	ASSERT_TRUE(process_dx12_poc_compute_path(src.data(), dst.data(), 3, 3, true, probe));
	EXPECT_NE(dst, src);
	const std::uint32_t center = dst[4];
	const std::uint32_t centerR = center & 0xffu;
	EXPECT_GT(centerR, 0u);
	EXPECT_LT(centerR, 255u);
}

TEST(Dx12PocProcessorTests, TryCreateDx12DeviceReturnsFalseWhenDllIsMissing)
{
	EXPECT_FALSE(try_create_dx12_device_for_poc(load_library_missing, get_proc_missing, free_library_dummy));
}

TEST(Dx12PocProcessorTests, TryCreateDx12DeviceReturnsFalseWhenEntryIsMissing)
{
	EXPECT_FALSE(try_create_dx12_device_for_poc(load_library_fake, get_proc_missing, free_library_dummy));
}

TEST(Dx12PocProcessorTests, TryCompileDx12ShaderReturnsFalseWhenDllIsMissing)
{
	EXPECT_FALSE(try_compile_dx12_poc_shader_for_poc(load_library_missing, get_proc_missing, free_library_dummy));
}

TEST(Dx12PocProcessorTests, TryCompileDx12ShaderReturnsFalseWhenEntryIsMissing)
{
	EXPECT_FALSE(try_compile_dx12_poc_shader_for_poc(load_library_fake, get_proc_missing, free_library_dummy));
}

TEST(Dx12PocProcessorTests, TryCreateDx12CommandObjectsReturnsFalseWhenDllIsMissing)
{
	EXPECT_FALSE(try_create_dx12_command_objects_for_poc(load_library_missing, get_proc_missing, free_library_dummy));
}

TEST(Dx12PocProcessorTests, TryCreateDx12CommandObjectsReturnsFalseWhenEntryIsMissing)
{
	EXPECT_FALSE(try_create_dx12_command_objects_for_poc(load_library_fake, get_proc_missing, free_library_dummy));
}

TEST(Dx12PocProcessorTests, TryCreateDx12ComputePipelineReturnsFalseWhenDllIsMissing)
{
	EXPECT_FALSE(try_create_dx12_compute_pipeline_for_poc(load_library_missing, get_proc_missing, free_library_dummy));
}

TEST(Dx12PocProcessorTests, TryCreateDx12ComputePipelineReturnsFalseWhenEntryIsMissing)
{
	EXPECT_FALSE(try_create_dx12_compute_pipeline_for_poc(load_library_fake, get_proc_missing, free_library_dummy));
}
