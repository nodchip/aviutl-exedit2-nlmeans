@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

if not exist docs\reports\dx12-poc-benchmark.md (
  call "%~dp0generate_dx12_poc_benchmark.cmd"
  if errorlevel 1 exit /b 1
)

if not exist docs\reports\dx12-poc-benchmark-history.csv (
  echo timestamp,copy_ms,compute_ms > docs\reports\dx12-poc-benchmark-history.csv
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$history='docs/reports/dx12-poc-benchmark-history.csv';" ^
  "$all=@(Get-Content $history);" ^
  "if($all.Count -gt 0 -and $all[0] -ne 'timestamp,copy_ms,compute_ms'){" ^
  "  $all[0]='timestamp,copy_ms,compute_ms';" ^
  "  Set-Content -Path $history -Value $all -Encoding utf8;" ^
  "}"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/dx12-poc-benchmark.md';" ^
  "$copy=-1.0; $compute=-1.0;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| DX12 PoC Single Frame \(copy path\) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $copy=[double]$Matches[1];" ^
  "  }" ^
  "  if($line -match '^\| DX12 PoC Single Frame \(compute path\) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $compute=[double]$Matches[1];" ^
  "  }" ^
  "}" ^
  "if($copy -lt 0 -or $compute -lt 0){ Write-Error '[update_dx12_poc_benchmark_history] target rows not found.'; exit 1 }" ^
  "$timestamp=Get-Date -Format 'yyyy-MM-ddTHH:mm:ssK';" ^
  "Add-Content -Path 'docs/reports/dx12-poc-benchmark-history.csv' -Value ($timestamp+','+$copy.ToString('F3')+','+$compute.ToString('F3'));"
if errorlevel 1 exit /b 1

echo [update_dx12_poc_benchmark_history] updated docs\reports\dx12-poc-benchmark-history.csv
exit /b 0
