@echo off
setlocal

set "SDK_HEADER=nlmeans_filter\aviutl2_sdk\filter2.h"
set "ALLOW_NO_GPU=%ALLOW_NO_GPU%"

echo [check_gpu_runner_prereq] checking Visual Studio tools...
where MSBuild.exe >nul 2>nul
if errorlevel 1 (
  echo [check_gpu_runner_prereq] ERROR: MSBuild.exe is not found in PATH.
  exit /b 1
)

where cl.exe >nul 2>nul
if errorlevel 1 (
  echo [check_gpu_runner_prereq] ERROR: cl.exe is not found in PATH.
  exit /b 1
)

echo [check_gpu_runner_prereq] checking ExEdit2 SDK header...
if not exist "%SDK_HEADER%" (
  echo [check_gpu_runner_prereq] ERROR: missing %SDK_HEADER%
  exit /b 1
)

echo [check_gpu_runner_prereq] checking GPU adapters...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$allowNoGpu = '%ALLOW_NO_GPU%' -eq '1';" ^
  "$gpus = @(Get-CimInstance Win32_VideoController | Where-Object { $_.Name -ne $null -and $_.Name.Trim().Length -gt 0 });" ^
  "$count = $gpus.Count;" ^
  "Write-Host ('[check_gpu_runner_prereq] detected GPU adapters: ' + $count);" ^
  "$i=0; foreach($gpu in $gpus){ Write-Host ('  [' + $i + '] ' + $gpu.Name); $i++ };" ^
  "if($count -lt 1 -and -not $allowNoGpu){ Write-Error '[check_gpu_runner_prereq] no GPU adapter found.'; exit 1 };" ^
  "if($count -lt 1 -and $allowNoGpu){ Write-Host '[check_gpu_runner_prereq] ALLOW_NO_GPU=1, zero GPU adapters is allowed.' }"
if errorlevel 1 exit /b 1

echo [check_gpu_runner_prereq] ok
exit /b 0
