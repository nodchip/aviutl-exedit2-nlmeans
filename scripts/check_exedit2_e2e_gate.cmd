@echo off
setlocal

set "MAX_AGE_DAYS=%~1"
if "%MAX_AGE_DAYS%"=="" set "MAX_AGE_DAYS=30"
set "HISTORY_PATH=docs\reports\exedit2-e2e-history.csv"

if not exist "%HISTORY_PATH%" (
  echo [check_exedit2_e2e_gate] ERROR: history file not found: %HISTORY_PATH%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$rows=Import-Csv '%HISTORY_PATH%';" ^
  "if($rows.Count -eq 0){" ^
  "  Write-Host '[check_exedit2_e2e_gate] ERROR: no E2E records found.'; exit 1" ^
  "}" ^
  "$latest=$rows[-1];" ^
  "$mustPass=@('cpu_naive','cpu_avx2','gpu_dx11','fallback');" ^
  "foreach($k in $mustPass){" ^
  "  if($latest.$k -ne 'PASS'){" ^
  "    Write-Host ('[check_exedit2_e2e_gate] FAIL: ' + $k + '=' + $latest.$k + ' (PASS required).'); exit 1" ^
  "  }" ^
  "}" ^
  "if($latest.gpu_coop -ne 'PASS' -and $latest.gpu_coop -ne 'SKIP'){" ^
  "  Write-Host ('[check_exedit2_e2e_gate] FAIL: gpu_coop=' + $latest.gpu_coop + ' (PASS or SKIP required).'); exit 1" ^
  "}" ^
  "try{ $ts=[datetimeoffset]::Parse($latest.timestamp) } catch {" ^
  "  Write-Host ('[check_exedit2_e2e_gate] FAIL: invalid timestamp=' + $latest.timestamp); exit 1" ^
  "}" ^
  "$age=[int]((Get-Date) - $ts.LocalDateTime).TotalDays;" ^
  "$limit=[int]$env:MAX_AGE_DAYS;" ^
  "if($age -gt $limit){" ^
  "  Write-Host ('[check_exedit2_e2e_gate] FAIL: latest record is stale. age=' + $age + ' days, limit=' + $limit + ' days.'); exit 1" ^
  "}" ^
  "Write-Host ('[check_exedit2_e2e_gate] ok. latest=' + $latest.timestamp + ', age=' + $age + ' days, coop=' + $latest.gpu_coop + '.'); exit 0"
if errorlevel 1 exit /b 1

exit /b 0
