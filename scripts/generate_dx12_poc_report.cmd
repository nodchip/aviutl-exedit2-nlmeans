@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

cl /nologo /utf-8 /EHsc /O2 /std:c++17 /wd4828 ^
  /I nlmeans_filter ^
  nlmeans_filter\tests\Dx12PocProbe.cpp ^
  /Fe:Dx12PocProbe.exe
if errorlevel 1 exit /b 1

Dx12PocProbe.exe > docs\reports\dx12-poc-readiness.md
set "EXITCODE=%ERRORLEVEL%"

if %EXITCODE%==0 (
  echo [generate_dx12_poc_report] dx12 runtime is available.
) else (
  echo [generate_dx12_poc_report] dx12 runtime is unavailable.
)

exit /b 0
