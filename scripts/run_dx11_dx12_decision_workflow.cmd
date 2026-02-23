@echo off
setlocal

echo [run_dx11_dx12_decision_workflow] start.

call "%~dp0generate_dx11_dx12_benchmark.cmd"
if errorlevel 1 exit /b 1

call "%~dp0check_dx11_dx12_benchmark_threshold.cmd"
if errorlevel 1 exit /b 1

if exist nlmeans_filter\aviutl2_sdk\filter2.h (
  call "%~dp0generate_dx11_dx12_quality_report.cmd"
  if errorlevel 1 exit /b 1
  call "%~dp0check_dx11_dx12_quality_threshold.cmd"
  if errorlevel 1 exit /b 1
) else (
  echo [run_dx11_dx12_decision_workflow] skip quality report/check because aviutl2_sdk\filter2.h is missing.
)

call "%~dp0check_dx11_dx12_adoption_gate.cmd" >nul 2>nul
if errorlevel 1 (
  echo [run_dx11_dx12_decision_workflow] adoption gate is FAIL.
 ) else (
  echo [run_dx11_dx12_decision_workflow] adoption gate is PASS.
)

call "%~dp0generate_dx11_dx12_decision_report.cmd"
if errorlevel 1 exit /b 1

call "%~dp0check_dx11_dx12_reevaluation_due.cmd"
if errorlevel 1 exit /b 1

echo [run_dx11_dx12_decision_workflow] done.
exit /b 0
