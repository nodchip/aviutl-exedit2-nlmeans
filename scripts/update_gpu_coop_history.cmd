@echo off
setlocal

call "%~dp0generate_gpu_coop_report.cmd"
if errorlevel 1 exit /b 1

if not exist docs\reports (
  mkdir docs\reports
)

if not exist docs\reports\gpu-coop-history.csv (
  echo timestamp,single_ms,coop_ms,ratio > docs\reports\gpu-coop-history.csv
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/gpu-coop-benchmark.md';" ^
  "$single=0.0; $coop=0.0;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| Single GPU Full Frame \| ([0-9]+\.[0-9]+) \|$'){ $single=[double]$Matches[1] }" ^
  "  if($line -match '^\| Coop PoC \(2 tiles, sequential dispatch\) \| ([0-9]+\.[0-9]+) \|$'){ $coop=[double]$Matches[1] }" ^
  "}" ^
  "$ratio = if($single -gt 0){ $coop / $single } else { 0.0 };" ^
  "$timestamp = Get-Date -Format 'yyyy-MM-ddTHH:mm:ssK';" ^
  "Add-Content -Path 'docs/reports/gpu-coop-history.csv' -Value ($timestamp+','+$single.ToString('F3')+','+$coop.ToString('F3')+','+$ratio.ToString('F3'));"
if errorlevel 1 exit /b 1

exit /b 0
