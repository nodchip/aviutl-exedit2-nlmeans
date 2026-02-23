@echo off
setlocal

set "REPORT=docs\reports\dx11-dx12-quality.md"
set "COPY_MAX_ABS_DIFF_LIMIT=32"
set "COPY_MEAN_ABS_DIFF_LIMIT=6.0"
set "COMPUTE_MAX_ABS_DIFF_LIMIT=128"
set "COMPUTE_MEAN_ABS_DIFF_LIMIT=64.0"

if not exist "%REPORT%" (
  echo [check_dx11_dx12_quality_threshold] report file not found: %REPORT%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/dx11-dx12-quality.md';" ^
  "$copyMax=-1; $copyMean=-1.0; $computeMax=-1; $computeMean=-1.0;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| DX11 vs DX12 PoC \(copy path\) \| ([0-9]+) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $copyMax=[int]$Matches[1];" ^
  "    $copyMean=[double]$Matches[2];" ^
  "  }" ^
  "  if($line -match '^\| DX11 vs DX12 PoC \(compute path\) \| ([0-9]+) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $computeMax=[int]$Matches[1];" ^
  "    $computeMean=[double]$Matches[2];" ^
  "  }" ^
  "}" ^
  "if($copyMax -lt 0 -or $copyMean -lt 0 -or $computeMax -lt 0 -or $computeMean -lt 0){" ^
  "  Write-Error '[check_dx11_dx12_quality_threshold] target row not found.';" ^
  "  exit 1" ^
  "}" ^
  "$copyMaxLimit=%COPY_MAX_ABS_DIFF_LIMIT%;" ^
  "$copyMeanLimit=%COPY_MEAN_ABS_DIFF_LIMIT%;" ^
  "$computeMaxLimit=%COMPUTE_MAX_ABS_DIFF_LIMIT%;" ^
  "$computeMeanLimit=%COMPUTE_MEAN_ABS_DIFF_LIMIT%;" ^
  "if($copyMax -gt $copyMaxLimit -or $copyMean -gt $copyMeanLimit){" ^
  "  Write-Error ('[check_dx11_dx12_quality_threshold] copy-path regression detected. max=' + $copyMax + ', mean=' + $copyMean + ', limits=(' + $copyMaxLimit + ',' + $copyMeanLimit + ')');" ^
  "  exit 1" ^
  "}" ^
  "if($computeMax -gt $computeMaxLimit -or $computeMean -gt $computeMeanLimit){" ^
  "  Write-Error ('[check_dx11_dx12_quality_threshold] compute-path regression detected. max=' + $computeMax + ', mean=' + $computeMean + ', limits=(' + $computeMaxLimit + ',' + $computeMeanLimit + ')');" ^
  "  exit 1" ^
  "}" ^
  "Write-Host ('[check_dx11_dx12_quality_threshold] ok. copy=(' + $copyMax + ',' + $copyMean + ') compute=(' + $computeMax + ',' + $computeMean + ')');"
if errorlevel 1 exit /b 1

exit /b 0
