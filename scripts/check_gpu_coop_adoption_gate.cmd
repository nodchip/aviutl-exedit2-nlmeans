@echo off
setlocal

set "HISTORY=docs\reports\gpu-coop-history.csv"
set "REPORT=docs\reports\gpu-coop-benchmark.md"
set "MIN_SAMPLES=3"
set "MAX_RATIO_LIMIT=1.50"
set "MAX_ASYNC_OVER_SINGLE=1.20"

if not exist "%HISTORY%" (
  echo [check_gpu_coop_adoption_gate] history file not found: %HISTORY%
  exit /b 1
)

if not exist "%REPORT%" (
  echo [check_gpu_coop_adoption_gate] report file not found: %REPORT%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$history='docs/reports/gpu-coop-history.csv';" ^
  "$report='docs/reports/gpu-coop-benchmark.md';" ^
  "$minSamples=%MIN_SAMPLES%;" ^
  "$ratioLimit=%MAX_RATIO_LIMIT%;" ^
  "$asyncOverSingleLimit=%MAX_ASYNC_OVER_SINGLE%;" ^
  "$rows=@(Get-Content $history | Where-Object { $_ -and -not $_.StartsWith('timestamp,') });" ^
  "if($rows.Count -lt $minSamples){" ^
  "  Write-Error ('[check_gpu_coop_adoption_gate] history rows are not enough. required=' + $minSamples + ', actual=' + $rows.Count);" ^
  "  exit 1" ^
  "}" ^
  "$recent=$rows | Select-Object -Last $minSamples;" ^
  "$i=0;" ^
  "foreach($row in $recent){" ^
  "  $cols=$row.Split(',');" ^
  "  if($cols.Count -lt 4){ Write-Error '[check_gpu_coop_adoption_gate] invalid history row format.'; exit 1 }" ^
  "  $ratio=[double]$cols[3];" ^
  "  if($ratio -gt $ratioLimit){" ^
  "    Write-Error ('[check_gpu_coop_adoption_gate] ratio gate failed at recent[' + $i + ']. ratio=' + $ratio.ToString('F3') + ', limit=' + $ratioLimit.ToString('F3'));" ^
  "    exit 1" ^
  "  }" ^
  "  $i++;" ^
  "}" ^
  "$single=0.0; $async=0.0;" ^
  "$lines=Get-Content $report;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| Single GPU Full Frame \| ([0-9]+(\.[0-9]+)?) \|$'){ $single=[double]$Matches[1] }" ^
  "  if($line -match '^\| Coop PoC \(2 tiles, async dispatch\) \| ([0-9]+(\.[0-9]+)?) \|$'){ $async=[double]$Matches[1] }" ^
  "}" ^
  "if($single -le 0 -or $async -le 0){ Write-Error '[check_gpu_coop_adoption_gate] missing benchmark rows in report.'; exit 1 }" ^
  "$asyncOverSingle=$async / $single;" ^
  "if($asyncOverSingle -gt $asyncOverSingleLimit){" ^
  "  Write-Error ('[check_gpu_coop_adoption_gate] async/single gate failed. single=' + $single.ToString('F3') + ', async=' + $async.ToString('F3') + ', ratio=' + $asyncOverSingle.ToString('F3') + ', limit=' + $asyncOverSingleLimit.ToString('F3'));" ^
  "  exit 1" ^
  "}" ^
  "Write-Host ('[check_gpu_coop_adoption_gate] gate passed. samples=' + $minSamples + ', ratioLimit=' + $ratioLimit.ToString('F3') + ', asyncOverSingle=' + $asyncOverSingle.ToString('F3'));"
if errorlevel 1 exit /b 1

exit /b 0
