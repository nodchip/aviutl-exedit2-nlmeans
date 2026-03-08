@echo off
setlocal

set "HISTORY=%GPU_COOP_HISTORY_PATH%"
if "%HISTORY%"=="" set "HISTORY=docs\reports\gpu-coop-history.csv"
set "MAX_RATIO_WORSE=1.15"
set "MAX_COOP_MS_WORSE=1.20"
set "MAX_SINGLE_MS_SCALE_DIFF=1.50"
set "TARGET_PROFILE=%GPU_COOP_PROFILE%"
if "%TARGET_PROFILE%"=="" set "TARGET_PROFILE=fhd"

if not exist "%HISTORY%" (
  echo [check_gpu_coop_regression] history file not found: %HISTORY%
  exit /b 0
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$target='%TARGET_PROFILE%';" ^
  "$rows=@();" ^
  "foreach($line in Get-Content '%HISTORY%'){" ^
  "  if(-not $line -or $line.StartsWith('timestamp,')){ continue }" ^
  "  $cols=$line.Split(',');" ^
  "  if($cols.Count -ge 5){" ^
  "    $rows += [pscustomobject]@{ timestamp=$cols[0]; profile=$cols[1]; single=[double]$cols[2]; coop=[double]$cols[3]; ratio=[double]$cols[4] };" ^
  "  } elseif($cols.Count -ge 4){" ^
  "    $rows += [pscustomobject]@{ timestamp=$cols[0]; profile='legacy'; single=[double]$cols[1]; coop=[double]$cols[2]; ratio=[double]$cols[3] };" ^
  "  }" ^
  "}" ^
  "$targetRows=@($rows | Where-Object { $_.profile -eq $target });" ^
  "if($targetRows.Count -lt 2){ Write-Host ('[check_gpu_coop_regression] skipped: need at least 2 samples for profile=' + $target + '.'); exit 0 }" ^
  "$prev=$targetRows[$targetRows.Count-2];" ^
  "$curr=$targetRows[$targetRows.Count-1];" ^
  "$prevSingle = [double]$prev.single;" ^
  "$currSingle = [double]$curr.single;" ^
  "$prevCoop = [double]$prev.coop;" ^
  "$currCoop = [double]$curr.coop;" ^
  "$prevRatio = [double]$prev.ratio;" ^
  "$currRatio = [double]$curr.ratio;" ^
  "$ratioLimit = $prevRatio * %MAX_RATIO_WORSE%;" ^
  "$coopLimit = $prevCoop * %MAX_COOP_MS_WORSE%;" ^
  "$singleScale = [double]::PositiveInfinity;" ^
  "if($prevSingle -gt 0 -and $currSingle -gt 0){" ^
  "  $singleScale = [Math]::Max($currSingle / $prevSingle, $prevSingle / $currSingle);" ^
  "}" ^
  "$compareAbsoluteCoop = $singleScale -le %MAX_SINGLE_MS_SCALE_DIFF%;" ^
  "if(($currRatio -gt $ratioLimit) -or ($compareAbsoluteCoop -and ($currCoop -gt $coopLimit))){" ^
  "  Write-Error ('[check_gpu_coop_regression] regression detected. profile=' + $target + ', prevSingle=' + $prevSingle + ', currSingle=' + $currSingle + ', prevRatio=' + $prevRatio + ', currRatio=' + $currRatio + ', prevCoop=' + $prevCoop + ', currCoop=' + $currCoop + ', absoluteCoopCheck=' + $compareAbsoluteCoop);" ^
  "  exit 1" ^
  "}" ^
  "$absoluteMode = if($compareAbsoluteCoop){ 'enabled' } else { 'skipped' };" ^
  "Write-Host ('[check_gpu_coop_regression] ok. profile=' + $target + ', prevSingle=' + $prevSingle + ', currSingle=' + $currSingle + ', prevRatio=' + $prevRatio + ', currRatio=' + $currRatio + ', prevCoop=' + $prevCoop + ', currCoop=' + $currCoop + ', absoluteCoopCheck=' + $absoluteMode);"
if errorlevel 1 exit /b 1

exit /b 0
