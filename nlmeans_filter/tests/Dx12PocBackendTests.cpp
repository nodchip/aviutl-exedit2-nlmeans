// DirectX 12 PoC ランタイム判定を GoogleTest で検証する。
#include <gtest/gtest.h>
#include <string>
#include "../exedit2/Dx12PocBackend.h"

namespace {

HMODULE WINAPI fake_load_library_missing(LPCWSTR)
{
	return nullptr;
}

HMODULE WINAPI fake_load_library_found(LPCWSTR)
{
	return reinterpret_cast<HMODULE>(1);
}

FARPROC WINAPI fake_get_proc_missing(HMODULE, LPCSTR)
{
	return nullptr;
}

FARPROC WINAPI fake_get_proc_found(HMODULE, LPCSTR name)
{
	if (name == nullptr) {
		return nullptr;
	}
	if (std::string(name) == "D3D12CreateDevice") {
		return reinterpret_cast<FARPROC>(1);
	}
	return nullptr;
}

BOOL WINAPI fake_free_library(HMODULE)
{
	return TRUE;
}

}

TEST(Dx12PocBackendTests, ProbeReturnsDisabledWhenDllIsMissing)
{
	const Dx12PocProbeResult result = probe_dx12_poc_runtime(
		fake_load_library_missing,
		fake_get_proc_found,
		fake_free_library);
	EXPECT_FALSE(result.d3d12DllFound);
	EXPECT_FALSE(result.d3d12CreateDeviceFound);
	EXPECT_FALSE(result.enabled);
}

TEST(Dx12PocBackendTests, ProbeReturnsDisabledWhenEntryIsMissing)
{
	const Dx12PocProbeResult result = probe_dx12_poc_runtime(
		fake_load_library_found,
		fake_get_proc_missing,
		fake_free_library);
	EXPECT_TRUE(result.d3d12DllFound);
	EXPECT_FALSE(result.d3d12CreateDeviceFound);
	EXPECT_FALSE(result.enabled);
}

TEST(Dx12PocBackendTests, ProbeReturnsEnabledWhenRuntimeIsAvailable)
{
	const Dx12PocProbeResult result = probe_dx12_poc_runtime(
		fake_load_library_found,
		fake_get_proc_found,
		fake_free_library);
	EXPECT_TRUE(result.d3d12DllFound);
	EXPECT_TRUE(result.d3d12CreateDeviceFound);
	EXPECT_TRUE(result.enabled);
}

TEST(Dx12PocBackendTests, CanEnableOnlyWhenRequestedAndSupported)
{
	Dx12PocProbeResult probe = {};
	probe.enabled = true;
	EXPECT_TRUE(can_enable_dx12_poc(true, probe));
	EXPECT_FALSE(can_enable_dx12_poc(false, probe));
	probe.enabled = false;
	EXPECT_FALSE(can_enable_dx12_poc(true, probe));
}
