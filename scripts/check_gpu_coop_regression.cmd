@echo off
setlocal

set "HISTORY=docs\reports\gpu-coop-history.csv"
set "MAX_RATIO_WORSE=1.15"
set "MAX_COOP_MS_WORSE=1.20"

if not exist "%HISTORY%" (
  echo [check_gpu_coop_regression] history file not found: %HISTORY%
  exit /b 0
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$lines = Get-Content '%HISTORY%';" ^
  "if($lines.Count -lt 3){ Write-Host '[check_gpu_coop_regression] skipped: need at least 2 samples.'; exit 0 }" ^
  "$prevCols = $lines[$lines.Count-2].Split(',');" ^
  "$currCols = $lines[$lines.Count-1].Split(',');" ^
  "$prevCoop = [double]$prevCols[2];" ^
  "$currCoop = [double]$currCols[2];" ^
  "$prevRatio = [double]$prevCols[3];" ^
  "$currRatio = [double]$currCols[3];" ^
  "$ratioLimit = $prevRatio * %MAX_RATIO_WORSE%;" ^
  "$coopLimit = $prevCoop * %MAX_COOP_MS_WORSE%;" ^
  "if(($currRatio -gt $ratioLimit) -or ($currCoop -gt $coopLimit)){" ^
  "  Write-Error ('[check_gpu_coop_regression] regression detected. prevRatio=' + $prevRatio + ', currRatio=' + $currRatio + ', prevCoop=' + $prevCoop + ', currCoop=' + $currCoop);" ^
  "  exit 1" ^
  "}" ^
  "Write-Host ('[check_gpu_coop_regression] ok. prevRatio=' + $prevRatio + ', currRatio=' + $currRatio + ', prevCoop=' + $prevCoop + ', currCoop=' + $currCoop);"
if errorlevel 1 exit /b 1

exit /b 0
