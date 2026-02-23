// DirectX 12 PoC のランタイム可否を Markdown で出力する。
#include <iostream>
#include "../exedit2/Dx12PocBackend.h"

int main()
{
	const Dx12PocProbeResult probe = probe_dx12_poc_runtime();

	std::cout << "# DirectX 12 PoC Readiness\n\n";
	std::cout << "| Item | Result |\n";
	std::cout << "|---|---|\n";
	std::cout << "| d3d12.dll | " << (probe.d3d12DllFound ? "found" : "missing") << " |\n";
	std::cout << "| D3D12CreateDevice | " << (probe.d3d12CreateDeviceFound ? "found" : "missing") << " |\n";
	std::cout << "| DX12 PoC runtime | " << (probe.enabled ? "enabled" : "disabled") << " |\n";
	return probe.enabled ? 0 : 1;
}
