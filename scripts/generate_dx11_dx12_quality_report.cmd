@echo off
setlocal

call scripts\setup_msvc_env.cmd
if errorlevel 1 exit /b 1

if not exist docs\reports (
  mkdir docs\reports
)

if not exist nlmeans_filter\aviutl2_sdk\filter2.h (
  echo # DX11 vs DX12 PoC Quality Comparison> docs\reports\dx11-dx12-quality.md
  echo.>> docs\reports\dx11-dx12-quality.md
  echo - SKIPPED: aviutl2_sdk/filter2.h is missing.>> docs\reports\dx11-dx12-quality.md
  exit /b 0
)

cl /nologo /utf-8 /EHsc /O2 /std:c++17 /wd4828 ^
  /I nlmeans_filter ^
  /I nlmeans_filter\aviutl2_sdk ^
  nlmeans_filter\tests\Dx11Dx12QualityReport.cpp ^
  nlmeans_filter\exedit2\Exedit2GpuRunner.cpp ^
  /Fe:Dx11Dx12QualityReport.exe ^
  /link d3d11.lib d3dcompiler.lib dxgi.lib
if errorlevel 1 exit /b 1

Dx11Dx12QualityReport.exe > docs\reports\dx11-dx12-quality.md
if errorlevel 1 exit /b 1

echo [generate_dx11_dx12_quality_report] generated docs\reports\dx11-dx12-quality.md
exit /b 0
