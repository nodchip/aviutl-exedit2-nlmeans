@echo off
setlocal

echo [run_gpu_coop_decision_workflow] start.
set "WORKFLOW_RESULT=0"

call "%~dp0update_gpu_coop_history.cmd"
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] update_gpu_coop_history is FAIL.
  set "WORKFLOW_RESULT=1"
)

call "%~dp0check_gpu_coop_async_efficiency.cmd"
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] async efficiency check is FAIL.
  set "WORKFLOW_RESULT=1"
)

call "%~dp0check_gpu_coop_regression.cmd"
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] regression check is FAIL.
  set "WORKFLOW_RESULT=1"
)

call "%~dp0check_gpu_coop_adoption_gate.cmd" >nul 2>nul
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] adoption gate is FAIL.
) else (
  echo [run_gpu_coop_decision_workflow] adoption gate is PASS.
)

call "%~dp0check_exedit2_e2e_gate.cmd" >nul 2>nul
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] exedit2 e2e gate is FAIL.
) else (
  echo [run_gpu_coop_decision_workflow] exedit2 e2e gate is PASS.
)

call "%~dp0generate_exedit2_e2e_report.cmd" 30
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] e2e report generation is FAIL.
  set "WORKFLOW_RESULT=1"
)

call "%~dp0generate_gpu_coop_decision_report.cmd"
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] gpu coop decision report generation is FAIL.
  set "WORKFLOW_RESULT=1"
)

call "%~dp0check_gpu_coop_reevaluation_due.cmd"
if errorlevel 1 (
  echo [run_gpu_coop_decision_workflow] reevaluation due check is FAIL.
  set "WORKFLOW_RESULT=1"
)

if "%WORKFLOW_RESULT%"=="0" (
  echo [run_gpu_coop_decision_workflow] done.
  exit /b 0
)

echo [run_gpu_coop_decision_workflow] done with failure.
exit /b 1
