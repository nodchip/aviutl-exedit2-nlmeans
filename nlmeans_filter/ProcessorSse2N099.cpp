#include "stdafx.h"
#include <intrin.h>
#include <emmintrin.h>
#include "ProcessorSse2N099.h"

extern "C"
{
	EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTableSse2N099( void );
}

using namespace std;

ProcessorSse2N099::ProcessorSse2N099() : filterDll(*GetFilterTableSse2N099())
{
	prepared = filterDll.func_proc != NULL;
}

ProcessorSse2N099::~ProcessorSse2N099()
{
	filterDll.func_exit(NULL);
}

BOOL ProcessorSse2N099::proc(FILTER& fp, FILTER_PROC_INFO& fpip)
{
	return filterDll.func_proc(&fp, &fpip);
}

BOOL ProcessorSse2N099::wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	return filterDll.func_WndProc(hwnd, message, wparam, lparam, editp, fp);
}
