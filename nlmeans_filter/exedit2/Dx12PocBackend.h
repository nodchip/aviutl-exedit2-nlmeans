// DirectX 12 PoC の実行可否判定を提供する。
#ifndef EXEDIT2_DX12_POC_BACKEND_H
#define EXEDIT2_DX12_POC_BACKEND_H

#include <windows.h>

// DirectX 12 PoC のランタイム検出結果を保持する。
struct Dx12PocProbeResult
{
	bool d3d12DllFound;
	bool d3d12CreateDeviceFound;
	bool enabled;
};

// テスト用に差し替え可能な Win32 API 関数型。
using Dx12LoadLibraryFn = HMODULE(WINAPI*)(LPCWSTR);
using Dx12GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);
using Dx12FreeLibraryFn = BOOL(WINAPI*)(HMODULE);

// DX12 PoC ランタイム（d3d12.dll と D3D12CreateDevice）の存在を検出する。
inline Dx12PocProbeResult probe_dx12_poc_runtime(
	Dx12LoadLibraryFn loadLibraryFn = ::LoadLibraryW,
	Dx12GetProcAddressFn getProcAddressFn = ::GetProcAddress,
	Dx12FreeLibraryFn freeLibraryFn = ::FreeLibrary)
{
	Dx12PocProbeResult result = {};
	if (loadLibraryFn == nullptr || getProcAddressFn == nullptr || freeLibraryFn == nullptr) {
		return result;
	}

	HMODULE d3d12Module = loadLibraryFn(L"d3d12.dll");
	result.d3d12DllFound = (d3d12Module != nullptr);
	if (!result.d3d12DllFound) {
		return result;
	}

	const FARPROC createDeviceProc = getProcAddressFn(d3d12Module, "D3D12CreateDevice");
	result.d3d12CreateDeviceFound = (createDeviceProc != nullptr);
	result.enabled = result.d3d12CreateDeviceFound;
	freeLibraryFn(d3d12Module);
	return result;
}

// UI で DX12 PoC が要求された場合に実行を有効化できるか判定する。
inline bool can_enable_dx12_poc(bool userRequestedDx12Poc, const Dx12PocProbeResult& probe)
{
	return userRequestedDx12Poc && probe.enabled;
}

#endif
