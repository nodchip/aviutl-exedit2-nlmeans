@echo off
setlocal

call scripts\setup_msvc_env.cmd
if errorlevel 1 exit /b 1

if not exist docs\reports (
  mkdir docs\reports
)

if not exist nlmeans_filter\aviutl2_sdk\filter2.h (
  echo # GPU Variants Benchmark> docs\reports\gpu-variants-benchmark.md
  echo.>> docs\reports\gpu-variants-benchmark.md
  echo - SKIPPED: aviutl2_sdk/filter2.h is missing.>> docs\reports\gpu-variants-benchmark.md
  exit /b 0
)

cl /nologo /utf-8 /EHsc /O2 /std:c++17 /wd4828 ^
  /I nlmeans_filter ^
  /I nlmeans_filter\aviutl2_sdk ^
  nlmeans_filter\tests\GpuVariantsBenchmark.cpp ^
  nlmeans_filter\exedit2\Exedit2GpuRunner.cpp ^
  /Fe:GpuVariantsBenchmark.exe ^
  /link d3d11.lib d3dcompiler.lib dxgi.lib
if errorlevel 1 exit /b 1

set "GPU_VARIANTS_PROFILE_ARG="
if not "%GPU_VARIANTS_PROFILE%"=="" set "GPU_VARIANTS_PROFILE_ARG=--profile %GPU_VARIANTS_PROFILE%"

GpuVariantsBenchmark.exe %GPU_VARIANTS_PROFILE_ARG% > docs\reports\gpu-variants-benchmark.md
if errorlevel 1 exit /b 1

exit /b 0
