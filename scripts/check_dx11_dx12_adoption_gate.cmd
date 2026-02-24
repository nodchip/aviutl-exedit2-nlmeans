@echo off
setlocal

set "DX12_ADOPTION_POLICY=DISABLED"
if /I "%DX12_ADOPTION_POLICY%"=="DISABLED" (
  echo [check_dx11_dx12_adoption_gate] policy is DISABLED. DX12 adoption is not allowed.
  exit /b 1
)

set "BENCH_HISTORY=docs\reports\dx11-dx12-benchmark-history.csv"
set "QUALITY_HISTORY=docs\reports\dx11-dx12-quality-history.csv"
set "MIN_SAMPLES=3"
set "MAX_DX12_PER_DX11_RATIO=1.0"

if not exist "%BENCH_HISTORY%" (
  echo [check_dx11_dx12_adoption_gate] benchmark history not found: %BENCH_HISTORY%
  exit /b 1
)

if not exist "%QUALITY_HISTORY%" (
  echo [check_dx11_dx12_adoption_gate] quality history not found: %QUALITY_HISTORY%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$benchPath='docs/reports/dx11-dx12-benchmark-history.csv';" ^
  "$qualityPath='docs/reports/dx11-dx12-quality-history.csv';" ^
  "$minSamples=%MIN_SAMPLES%;" ^
  "$ratioLimit=%MAX_DX12_PER_DX11_RATIO%;" ^
  "$benchRows=@(Get-Content $benchPath | Where-Object { $_ -and -not $_.StartsWith('timestamp,') });" ^
  "if($benchRows.Count -lt $minSamples){" ^
  "  Write-Error ('[check_dx11_dx12_adoption_gate] benchmark rows are not enough. required=' + $minSamples + ', actual=' + $benchRows.Count);" ^
  "  exit 1" ^
  "}" ^
  "$recentBench=$benchRows | Select-Object -Last $minSamples;" ^
  "$index=0;" ^
  "foreach($row in $recentBench){" ^
  "  $cols=$row.Split(',');" ^
  "  if($cols.Count -lt 4){ Write-Error '[check_dx11_dx12_adoption_gate] invalid benchmark row format.'; exit 1 }" ^
  "  $dx11=[double]$cols[1];" ^
  "  $dx12=[double]$cols[3];" ^
  "  if($dx11 -le 0 -or $dx12 -le 0){ Write-Error '[check_dx11_dx12_adoption_gate] invalid benchmark values.'; exit 1 }" ^
  "  $ratio=$dx12 / $dx11;" ^
  "  if($ratio -gt $ratioLimit){" ^
  "    Write-Error ('[check_dx11_dx12_adoption_gate] speed gate failed at recent[' + $index + ']. dx11=' + $dx11.ToString('F3') + ', dx12=' + $dx12.ToString('F3') + ', ratio=' + $ratio.ToString('F3') + ', limit=' + $ratioLimit.ToString('F3'));" ^
  "    exit 1" ^
  "  }" ^
  "  $index++;" ^
  "}" ^
  "$qualityRows=@(Get-Content $qualityPath | Where-Object { $_ -and -not $_.StartsWith('timestamp,') });" ^
  "if($qualityRows.Count -lt 2){" ^
  "  Write-Error ('[check_dx11_dx12_adoption_gate] quality rows are not enough. required=2, actual=' + $qualityRows.Count);" ^
  "  exit 1" ^
  "}" ^
  "Write-Host ('[check_dx11_dx12_adoption_gate] gate passed. benchmark_recent=' + $minSamples + ', quality_rows=' + $qualityRows.Count);"
if errorlevel 1 exit /b 1

exit /b 0
