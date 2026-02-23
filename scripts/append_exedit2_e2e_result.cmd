@echo off
setlocal

if "%~8"=="" (
  echo [append_exedit2_e2e_result] usage:
  echo   %~nx0 ^<operator^> ^<build^> ^<gpu^> ^<cpu_naive^> ^<cpu_avx2^> ^<gpu_dx11^> ^<gpu_coop^> ^<fallback^> [notes...]
  echo [append_exedit2_e2e_result] status values: PASS ^| FAIL ^| SKIP
  exit /b 1
)

set "HISTORY_PATH=docs\reports\exedit2-e2e-history.csv"
if not exist "%HISTORY_PATH%" (
  echo [append_exedit2_e2e_result] ERROR: history file not found: %HISTORY_PATH%
  exit /b 1
)

set "OPERATOR=%~1"
set "BUILD_LABEL=%~2"
set "GPU_NAME=%~3"
set "CPU_NAIVE=%~4"
set "CPU_AVX2=%~5"
set "GPU_DX11=%~6"
set "GPU_COOP=%~7"
set "FALLBACK=%~8"

call :validate_status "%CPU_NAIVE%" cpu_naive
if errorlevel 1 exit /b 1
call :validate_status "%CPU_AVX2%" cpu_avx2
if errorlevel 1 exit /b 1
call :validate_status "%GPU_DX11%" gpu_dx11
if errorlevel 1 exit /b 1
call :validate_status "%GPU_COOP%" gpu_coop
if errorlevel 1 exit /b 1
call :validate_status "%FALLBACK%" fallback
if errorlevel 1 exit /b 1

shift
shift
shift
shift
shift
shift
shift
shift
set "NOTES=%*"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$timestamp=(Get-Date).ToString('yyyy-MM-ddTHH:mm:sszzz');" ^
  "$row=[pscustomobject]@{" ^
  "  timestamp=$timestamp;" ^
  "  operator=$env:OPERATOR;" ^
  "  build=$env:BUILD_LABEL;" ^
  "  gpu=$env:GPU_NAME;" ^
  "  cpu_naive=$env:CPU_NAIVE;" ^
  "  cpu_avx2=$env:CPU_AVX2;" ^
  "  gpu_dx11=$env:GPU_DX11;" ^
  "  gpu_coop=$env:GPU_COOP;" ^
  "  fallback=$env:FALLBACK;" ^
  "  notes=$env:NOTES" ^
  "};" ^
  "$row | Export-Csv -Path '%HISTORY_PATH%' -Append -NoTypeInformation -Encoding UTF8"
if errorlevel 1 exit /b 1

echo [append_exedit2_e2e_result] appended to %HISTORY_PATH%
exit /b 0

:validate_status
if /I "%~1"=="PASS" exit /b 0
if /I "%~1"=="FAIL" exit /b 0
if /I "%~1"=="SKIP" exit /b 0
echo [append_exedit2_e2e_result] ERROR: invalid %~2 value "%~1"
exit /b 1
