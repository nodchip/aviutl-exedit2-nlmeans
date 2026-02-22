@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

cl /nologo /utf-8 /EHsc /O2 /std:c++17 /I nlmeans_filter ^
  nlmeans_filter\tests\NlmVariantsBenchmark.cpp ^
  /Fe:NlmVariantsBenchmark.exe
if errorlevel 1 exit /b 1

NlmVariantsBenchmark.exe > docs\reports\nlm-variants-benchmark.md
if errorlevel 1 exit /b 1

exit /b 0
