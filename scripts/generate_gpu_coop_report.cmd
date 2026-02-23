@echo off
setlocal

call scripts\setup_msvc_env.cmd
if errorlevel 1 exit /b 1

if not exist docs\reports (
  mkdir docs\reports
)

if not exist nlmeans_filter\aviutl2_sdk\filter2.h (
  echo # GPU Coop PoC Benchmark> docs\reports\gpu-coop-benchmark.md
  echo.>> docs\reports\gpu-coop-benchmark.md
  echo - SKIPPED: aviutl2_sdk/filter2.h is missing.>> docs\reports\gpu-coop-benchmark.md
  exit /b 0
)

cl /nologo /utf-8 /EHsc /O2 /std:c++17 /wd4828 ^
  /I nlmeans_filter ^
  /I nlmeans_filter\aviutl2_sdk ^
  nlmeans_filter\tests\GpuCoopBenchmark.cpp ^
  nlmeans_filter\exedit2\Exedit2GpuRunner.cpp ^
  /Fe:GpuCoopBenchmark.exe ^
  /link d3d11.lib d3dcompiler.lib dxgi.lib
if errorlevel 1 exit /b 1

GpuCoopBenchmark.exe > docs\reports\gpu-coop-benchmark.md
if errorlevel 1 exit /b 1

exit /b 0
