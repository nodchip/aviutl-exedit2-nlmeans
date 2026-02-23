@echo off
setlocal

set "REPORT=docs\reports\dx11-dx12-quality.md"
set "MAX_ABS_DIFF_LIMIT=32"
set "MEAN_ABS_DIFF_LIMIT=6.0"

if not exist "%REPORT%" (
  echo [check_dx11_dx12_quality_threshold] report file not found: %REPORT%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/dx11-dx12-quality.md';" ^
  "$maxDiff=-1; $meanDiff=-1.0;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| DX11 vs DX12 PoC \| ([0-9]+) \| ([0-9]+(\.[0-9]+)?) \|$'){" ^
  "    $maxDiff=[int]$Matches[1];" ^
  "    $meanDiff=[double]$Matches[2];" ^
  "  }" ^
  "}" ^
  "if($maxDiff -lt 0 -or $meanDiff -lt 0){" ^
  "  Write-Error '[check_dx11_dx12_quality_threshold] target row not found.';" ^
  "  exit 1" ^
  "}" ^
  "$maxLimit=%MAX_ABS_DIFF_LIMIT%;" ^
  "$meanLimit=%MEAN_ABS_DIFF_LIMIT%;" ^
  "if($maxDiff -gt $maxLimit -or $meanDiff -gt $meanLimit){" ^
  "  Write-Error ('[check_dx11_dx12_quality_threshold] regression detected. max=' + $maxDiff + ', mean=' + $meanDiff + ', limits=(' + $maxLimit + ',' + $meanLimit + ')');" ^
  "  exit 1" ^
  "}" ^
  "Write-Host ('[check_dx11_dx12_quality_threshold] ok. max=' + $maxDiff + ', mean=' + $meanDiff + ', limits=(' + $maxLimit + ',' + $meanLimit + ')');"
if errorlevel 1 exit /b 1

exit /b 0
