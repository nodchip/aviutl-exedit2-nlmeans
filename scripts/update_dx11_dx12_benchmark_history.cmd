@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

if not exist docs\reports\dx11-dx12-benchmark.md (
  call "%~dp0generate_dx11_dx12_benchmark.cmd"
  if errorlevel 1 exit /b 1
)

if not exist docs\reports\dx11-dx12-benchmark-history.csv (
  echo timestamp,dx11_ms,dx12_copy_ms,dx12_compute_ms > docs\reports\dx11-dx12-benchmark-history.csv
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$history='docs/reports/dx11-dx12-benchmark-history.csv';" ^
  "$all=@(Get-Content $history);" ^
  "if($all.Count -gt 0 -and $all[0] -ne 'timestamp,dx11_ms,dx12_copy_ms,dx12_compute_ms'){" ^
  "  $all[0]='timestamp,dx11_ms,dx12_copy_ms,dx12_compute_ms';" ^
  "  Set-Content -Path $history -Value $all -Encoding utf8;" ^
  "}"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/dx11-dx12-benchmark.md';" ^
  "$dx11='-1.000'; $copy='-1.000'; $compute='-1.000';" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| DX11 Runner \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $dx11=[double]$Matches[1];" ^
  "  }" ^
  "  if($line -match '^\| DX11 Runner \| N/A \|$'){" ^
  "    $dx11=-1.0;" ^
  "  }" ^
  "  if($line -match '^\| DX12 PoC Single Frame \(copy path\) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $copy=[double]$Matches[1];" ^
  "  }" ^
  "  if($line -match '^\| DX12 PoC Single Frame \(compute path\) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $compute=[double]$Matches[1];" ^
  "  }" ^
  "}" ^
  "if($copy -lt 0 -or $compute -lt 0){ Write-Error '[update_dx11_dx12_benchmark_history] target rows not found.'; exit 1 }" ^
  "$timestamp=Get-Date -Format 'yyyy-MM-ddTHH:mm:ssK';" ^
  "Add-Content -Path 'docs/reports/dx11-dx12-benchmark-history.csv' -Value ($timestamp+','+([double]$dx11).ToString('F3')+','+$copy.ToString('F3')+','+$compute.ToString('F3'));"
if errorlevel 1 exit /b 1

echo [update_dx11_dx12_benchmark_history] updated docs\reports\dx11-dx12-benchmark-history.csv
exit /b 0
