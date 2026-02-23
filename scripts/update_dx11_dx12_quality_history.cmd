@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

if not exist docs\reports\dx11-dx12-quality.md (
  call "%~dp0generate_dx11_dx12_quality_report.cmd"
  if errorlevel 1 exit /b 1
)

if not exist docs\reports\dx11-dx12-quality-history.csv (
  echo timestamp,copy_max,copy_mean,compute_max,compute_mean > docs\reports\dx11-dx12-quality-history.csv
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$history='docs/reports/dx11-dx12-quality-history.csv';" ^
  "$all=@(Get-Content $history);" ^
  "if($all.Count -gt 0 -and $all[0] -ne 'timestamp,copy_max,copy_mean,compute_max,compute_mean'){" ^
  "  $all[0]='timestamp,copy_max,copy_mean,compute_max,compute_mean';" ^
  "  Set-Content -Path $history -Value $all -Encoding utf8;" ^
  "}"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/dx11-dx12-quality.md';" ^
  "$copyMax=-1; $copyMean=-1.0; $computeMax=-1; $computeMean=-1.0;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| DX11 vs DX12 PoC \(copy path\) \| ([0-9]+) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $copyMax=[int]$Matches[1]; $copyMean=[double]$Matches[2];" ^
  "  }" ^
  "  if($line -match '^\| DX11 vs DX12 PoC \(compute path\) \| ([0-9]+) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $computeMax=[int]$Matches[1]; $computeMean=[double]$Matches[2];" ^
  "  }" ^
  "}" ^
  "if($copyMax -lt 0 -or $computeMax -lt 0){ Write-Error '[update_dx11_dx12_quality_history] target rows not found.'; exit 1 }" ^
  "$timestamp=Get-Date -Format 'yyyy-MM-ddTHH:mm:ssK';" ^
  "Add-Content -Path 'docs/reports/dx11-dx12-quality-history.csv' -Value ($timestamp+','+$copyMax+','+$copyMean.ToString('F5')+','+$computeMax+','+$computeMean.ToString('F5'));"
if errorlevel 1 exit /b 1

echo [update_dx11_dx12_quality_history] updated docs\reports\dx11-dx12-quality-history.csv
exit /b 0
