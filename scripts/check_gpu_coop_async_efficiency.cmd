@echo off
setlocal

set "REPORT=docs\reports\gpu-coop-benchmark.md"
set "MAX_ASYNC_OVER_SEQ=1.05"

if not exist "%REPORT%" (
  echo [check_gpu_coop_async_efficiency] report file not found: %REPORT%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/gpu-coop-benchmark.md';" ^
  "$seq=0.0; $async=0.0;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| Coop PoC \(2 tiles, sequential dispatch\) \| ([0-9]+\.[0-9]+) \|$'){ $seq=[double]$Matches[1] }" ^
  "  if($line -match '^\| Coop PoC \(2 tiles, async dispatch\) \| ([0-9]+\.[0-9]+) \|$'){ $async=[double]$Matches[1] }" ^
  "}" ^
  "if($seq -le 0 -or $async -le 0){ Write-Error '[check_gpu_coop_async_efficiency] missing benchmark rows.'; exit 1 }" ^
  "$limit = $seq * %MAX_ASYNC_OVER_SEQ%;" ^
  "if($async -gt $limit){" ^
  "  Write-Error ('[check_gpu_coop_async_efficiency] regression detected. seq=' + $seq + ', async=' + $async + ', limit=' + $limit);" ^
  "  exit 1" ^
  "}" ^
  "Write-Host ('[check_gpu_coop_async_efficiency] ok. seq=' + $seq + ', async=' + $async + ', limit=' + $limit);"
if errorlevel 1 exit /b 1

exit /b 0
