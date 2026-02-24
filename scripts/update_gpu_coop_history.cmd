@echo off
setlocal

set "TARGET_PROFILE=%GPU_COOP_PROFILE%"
if "%TARGET_PROFILE%"=="" set "TARGET_PROFILE=fhd"

call "%~dp0generate_gpu_coop_report.cmd"
if errorlevel 1 exit /b 1

if not exist docs\reports (
  mkdir docs\reports
)

if not exist docs\reports\gpu-coop-history.csv (
  echo timestamp,profile,single_ms,coop_ms,ratio > docs\reports\gpu-coop-history.csv
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$history='docs/reports/gpu-coop-history.csv';" ^
  "$header='timestamp,profile,single_ms,coop_ms,ratio';" ^
  "$all=@();" ^
  "if(Test-Path $history){ $all=Get-Content $history }" ^
  "if($all.Count -eq 0){" ^
  "  Set-Content -Path $history -Value $header -Encoding utf8;" ^
  "} elseif($all[0] -ne $header){" ^
  "  $migrated=@($header);" ^
  "  foreach($line in $all){" ^
  "    if(-not $line -or $line.StartsWith('timestamp,')){ continue }" ^
  "    $cols=$line.Split(',');" ^
  "    if($cols.Count -ge 5){" ^
  "      $migrated += ($cols[0] + ',' + $cols[1] + ',' + $cols[2] + ',' + $cols[3] + ',' + $cols[4]);" ^
  "    } elseif($cols.Count -ge 4){" ^
  "      $migrated += ($cols[0] + ',legacy,' + $cols[1] + ',' + $cols[2] + ',' + $cols[3]);" ^
  "    }" ^
  "  }" ^
  "  $migrated | Set-Content -Path $history -Encoding utf8;" ^
  "}"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/gpu-coop-benchmark.md';" ^
  "$target='%TARGET_PROFILE%';" ^
  "$single=0.0; $coop=0.0; $found=$false;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  $trim=$line.Trim();" ^
  "  if(-not $trim.StartsWith('|')){ continue }" ^
  "  $cols=@($trim.Split('|') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' });" ^
  "  if($cols.Count -lt 10){ continue }" ^
  "  if($cols[0] -ne $target){ continue }" ^
  "  try{" ^
  "    $single=[double]$cols[3];" ^
  "    $coop=[double]$cols[4];" ^
  "    $found=$true;" ^
  "    break;" ^
  "  } catch { }" ^
  "}" ^
  "if(-not $found -or $single -le 0 -or $coop -le 0){ Write-Error ('[update_gpu_coop_history] target profile row not found or invalid: ' + $target); exit 1 }" ^
  "$ratio = if($single -gt 0){ $coop / $single } else { 0.0 };" ^
  "$timestamp = Get-Date -Format 'yyyy-MM-ddTHH:mm:ssK';" ^
  "Add-Content -Path 'docs/reports/gpu-coop-history.csv' -Value ($timestamp+','+$target+','+$single.ToString('F3')+','+$coop.ToString('F3')+','+$ratio.ToString('F3'));"
if errorlevel 1 exit /b 1

exit /b 0
