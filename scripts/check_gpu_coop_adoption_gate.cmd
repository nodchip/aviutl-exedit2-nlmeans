@echo off
setlocal

set "HISTORY=docs\reports\gpu-coop-history.csv"
set "REPORT=docs\reports\gpu-coop-benchmark.md"
set "MIN_SAMPLES=3"
set "MAX_RATIO_LIMIT=1.50"
set "MAX_ASYNC_OVER_SINGLE=1.20"
set "TARGET_PROFILE=%GPU_COOP_PROFILE%"
if "%TARGET_PROFILE%"=="" set "TARGET_PROFILE=fhd"

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
  "$target='%TARGET_PROFILE%';" ^
  "$minSamples=%MIN_SAMPLES%;" ^
  "$ratioLimit=%MAX_RATIO_LIMIT%;" ^
  "$asyncOverSingleLimit=%MAX_ASYNC_OVER_SINGLE%;" ^
  "$rows=@();" ^
  "foreach($line in Get-Content $history){" ^
  "  if(-not $line -or $line.StartsWith('timestamp,')){ continue }" ^
  "  $cols=$line.Split(',');" ^
  "  if($cols.Count -ge 5){" ^
  "    $rows += [pscustomobject]@{ timestamp=$cols[0]; profile=$cols[1]; single=[double]$cols[2]; coop=[double]$cols[3]; ratio=[double]$cols[4] };" ^
  "  } elseif($cols.Count -ge 4){" ^
  "    $rows += [pscustomobject]@{ timestamp=$cols[0]; profile='legacy'; single=[double]$cols[1]; coop=[double]$cols[2]; ratio=[double]$cols[3] };" ^
  "  }" ^
  "}" ^
  "$targetRows=@($rows | Where-Object { $_.profile -eq $target });" ^
  "if($targetRows.Count -lt $minSamples){" ^
  "  Write-Error ('[check_gpu_coop_adoption_gate] history rows are not enough for profile=' + $target + '. required=' + $minSamples + ', actual=' + $targetRows.Count);" ^
  "  exit 1" ^
  "}" ^
  "$recent=$targetRows | Select-Object -Last $minSamples;" ^
  "$i=0;" ^
  "foreach($row in $recent){" ^
  "  $ratio=[double]$row.ratio;" ^
  "  if($ratio -gt $ratioLimit){" ^
  "    Write-Error ('[check_gpu_coop_adoption_gate] ratio gate failed at recent[' + $i + '] for profile=' + $target + '. ratio=' + $ratio.ToString('F3') + ', limit=' + $ratioLimit.ToString('F3'));" ^
  "    exit 1" ^
  "  }" ^
  "  $i++;" ^
  "}" ^
  "$single=0.0; $async=0.0; $found=$false;" ^
  "$lines=Get-Content $report;" ^
  "foreach($line in $lines){" ^
  "  $trim=$line.Trim();" ^
  "  if(-not $trim.StartsWith('|')){ continue }" ^
  "  $cols=@($trim.Split('|') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' });" ^
  "  if($cols.Count -lt 10){ continue }" ^
  "  if($cols[0] -ne $target){ continue }" ^
  "  try{" ^
  "    $single=[double]$cols[3];" ^
  "    $async=[double]$cols[5];" ^
  "    $found=$true;" ^
  "    break;" ^
  "  } catch { }" ^
  "}" ^
  "if(-not $found -or $single -le 0 -or $async -le 0){ Write-Error ('[check_gpu_coop_adoption_gate] target profile row not found or invalid in report: ' + $target); exit 1 }" ^
  "$asyncOverSingle=$async / $single;" ^
  "if($asyncOverSingle -gt $asyncOverSingleLimit){" ^
  "  Write-Error ('[check_gpu_coop_adoption_gate] async/single gate failed for profile=' + $target + '. single=' + $single.ToString('F3') + ', async=' + $async.ToString('F3') + ', ratio=' + $asyncOverSingle.ToString('F3') + ', limit=' + $asyncOverSingleLimit.ToString('F3'));" ^
  "  exit 1" ^
  "}" ^
  "Write-Host ('[check_gpu_coop_adoption_gate] gate passed. profile=' + $target + ', samples=' + $minSamples + ', ratioLimit=' + $ratioLimit.ToString('F3') + ', asyncOverSingle=' + $asyncOverSingle.ToString('F3'));"
if errorlevel 1 exit /b 1

exit /b 0
