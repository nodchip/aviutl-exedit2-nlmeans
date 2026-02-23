@echo off
setlocal

if not exist docs\reports\dx12-poc-benchmark-history.csv (
  echo [check_dx12_poc_regression] history file not found, skip.
  exit /b 0
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/dx12-poc-benchmark-history.csv';" ^
  "$rows=@(Get-Content $path | Where-Object { $_ -and -not $_.StartsWith('timestamp,') });" ^
  "if($rows.Count -lt 2){ Write-Host '[check_dx12_poc_regression] history has less than 2 rows, skip.'; exit 0 }" ^
  "$prev=$rows[$rows.Count-2].Split(',');" ^
  "$curr=$rows[$rows.Count-1].Split(',');" ^
  "$prevCompute=[double]$prev[2];" ^
  "$currCompute=[double]$curr[2];" ^
  "$threshold=1.20;" ^
  "if($prevCompute -le 0){ Write-Error '[check_dx12_poc_regression] invalid previous compute value.'; exit 1 }" ^
  "$ratio=$currCompute / $prevCompute;" ^
  "if($ratio -gt $threshold){" ^
  "  Write-Error ('[check_dx12_poc_regression] regression detected. prev='+$prevCompute.ToString('F3')+', curr='+$currCompute.ToString('F3')+', ratio='+$ratio.ToString('F3')+', threshold='+$threshold.ToString('F3')); exit 1" ^
  "} else {" ^
  "  Write-Host ('[check_dx12_poc_regression] ok. prev='+$prevCompute.ToString('F3')+', curr='+$currCompute.ToString('F3')+', ratio='+$ratio.ToString('F3')); exit 0" ^
  "}"
if errorlevel 1 exit /b 1

exit /b 0
