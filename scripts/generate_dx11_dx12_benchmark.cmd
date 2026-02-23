@echo off
setlocal

call scripts\setup_msvc_env.cmd
if errorlevel 1 exit /b 1

if not exist docs\reports (
  mkdir docs\reports
)

cl /nologo /utf-8 /EHsc /O2 /std:c++17 /wd4828 ^
  /I nlmeans_filter ^
  nlmeans_filter\tests\Dx11Dx12Benchmark.cpp ^
  nlmeans_filter\exedit2\Exedit2GpuRunner.cpp ^
  /link d3d11.lib dxgi.lib d3dcompiler.lib ^
  /Fe:Dx11Dx12Benchmark.exe
if errorlevel 1 exit /b 1

Dx11Dx12Benchmark.exe > docs\reports\dx11-dx12-benchmark.md
if errorlevel 1 exit /b 1

echo [generate_dx11_dx12_benchmark] generated docs\reports\dx11-dx12-benchmark.md
exit /b 0
