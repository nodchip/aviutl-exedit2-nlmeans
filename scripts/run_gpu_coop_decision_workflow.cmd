@echo off
setlocal

echo [run_gpu_coop_decision_workflow] start.

call "%~dp0update_gpu_coop_history.cmd"
if errorlevel 1 exit /b 1

call "%~dp0check_gpu_coop_async_efficiency.cmd"
if errorlevel 1 exit /b 1

call "%~dp0check_gpu_coop_regression.cmd"
if errorlevel 1 exit /b 1

call "%~dp0check_gpu_coop_adoption_gate.cmd" >nul 2>nul
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] adoption gate is FAIL.
) else (
  echo [run_gpu_coop_decision_workflow] adoption gate is PASS.
)

call "%~dp0generate_gpu_coop_decision_report.cmd"
if errorlevel 1 exit /b 1

call "%~dp0check_gpu_coop_reevaluation_due.cmd"
if errorlevel 1 exit /b 1

echo [run_gpu_coop_decision_workflow] done.
exit /b 0
