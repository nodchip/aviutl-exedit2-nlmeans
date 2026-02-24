@echo off
setlocal

set "REPORT=docs\reports\gpu-coop-benchmark.md"
set "MAX_ASYNC_OVER_SEQ=1.05"
set "TARGET_PROFILE=%GPU_COOP_PROFILE%"
if "%TARGET_PROFILE%"=="" set "TARGET_PROFILE=fhd"

if not exist "%REPORT%" (
  echo [check_gpu_coop_async_efficiency] report file not found: %REPORT%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/gpu-coop-benchmark.md';" ^
  "$target='%TARGET_PROFILE%';" ^
  "$seq=0.0; $async=0.0; $found=$false;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  $trim=$line.Trim();" ^
  "  if(-not $trim.StartsWith('|')){ continue }" ^
  "  $cols=@($trim.Split('|') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' });" ^
  "  if($cols.Count -lt 10){ continue }" ^
  "  if($cols[0] -ne $target){ continue }" ^
  "  try{" ^
  "    $seq=[double]$cols[4];" ^
  "    $async=[double]$cols[5];" ^
  "    $found=$true;" ^
  "    break;" ^
  "  } catch { }" ^
  "}" ^
  "if(-not $found -or $seq -le 0 -or $async -le 0){ Write-Error ('[check_gpu_coop_async_efficiency] target profile row not found or invalid: ' + $target); exit 1 }" ^
  "$limit = $seq * %MAX_ASYNC_OVER_SEQ%;" ^
  "if($async -gt $limit){" ^
  "  Write-Error ('[check_gpu_coop_async_efficiency] regression detected. profile=' + $target + ', seq=' + $seq + ', async=' + $async + ', limit=' + $limit);" ^
  "  exit 1" ^
  "}" ^
  "Write-Host ('[check_gpu_coop_async_efficiency] ok. profile=' + $target + ', seq=' + $seq + ', async=' + $async + ', limit=' + $limit);"
if errorlevel 1 exit /b 1

exit /b 0
