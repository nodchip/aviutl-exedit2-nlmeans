@echo off
setlocal

call scripts\setup_msvc_env.cmd
if errorlevel 1 exit /b 1

if not exist docs\reports (
  mkdir docs\reports
)

cl /nologo /utf-8 /EHsc /O2 /std:c++17 /wd4828 ^
  /I nlmeans_filter ^
  nlmeans_filter\tests\Dx12PocBenchmark.cpp ^
  /Fe:Dx12PocBenchmark.exe
if errorlevel 1 exit /b 1

Dx12PocBenchmark.exe > docs\reports\dx12-poc-benchmark.md
if errorlevel 1 exit /b 1

echo [generate_dx12_poc_benchmark] generated docs\reports\dx12-poc-benchmark.md
exit /b 0
