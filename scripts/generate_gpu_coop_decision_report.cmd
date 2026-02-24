@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

set "REPORT=docs\reports\gpu-coop-decision.md"
set "GATE_RESULT=FAIL"
set "E2E_GATE_RESULT=FAIL"
set "TARGET_PROFILE=%GPU_COOP_PROFILE%"
if "%TARGET_PROFILE%"=="" set "TARGET_PROFILE=fhd"

call "%~dp0check_gpu_coop_adoption_gate.cmd" >nul 2>nul
if not errorlevel 1 (
  set "GATE_RESULT=PASS"
)

call "%~dp0check_exedit2_e2e_gate.cmd" >nul 2>nul
if not errorlevel 1 (
  set "E2E_GATE_RESULT=PASS"
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$historyPath='docs/reports/gpu-coop-history.csv';" ^
  "$benchPath='docs/reports/gpu-coop-benchmark.md';" ^
  "$outPath='docs/reports/gpu-coop-decision.md';" ^
  "$gate='%GATE_RESULT%';" ^
  "$e2eGate='%E2E_GATE_RESULT%';" ^
  "$target='%TARGET_PROFILE%';" ^
  "$decision='NOT_ADOPTED';" ^
  "$policy='Keep single-GPU as the default indefinitely. Multi-GPU coop remains non-adopted.';" ^
  "$fixedStart='2026-02-23';" ^
  "$fixedEnd='indefinite';" ^
  "$reevalDate='none';" ^
  "$reevalStatus='not scheduled (policy disabled)';" ^
  "$reevalTriggers='explicit project decision is required to reopen GPU coop adoption review';" ^
  "$rows=@();" ^
  "if(Test-Path $historyPath){" ^
  "  foreach($line in Get-Content $historyPath){" ^
  "    if(-not $line -or $line.StartsWith('timestamp,')){ continue }" ^
  "    $cols=$line.Split(',');" ^
  "    if($cols.Count -ge 5){" ^
  "      $rows += [pscustomobject]@{ timestamp=$cols[0]; profile=$cols[1]; single=[double]$cols[2]; coop=[double]$cols[3]; ratio=[double]$cols[4] };" ^
  "    } elseif($cols.Count -ge 4){" ^
  "      $rows += [pscustomobject]@{ timestamp=$cols[0]; profile='legacy'; single=[double]$cols[1]; coop=[double]$cols[2]; ratio=[double]$cols[3] };" ^
  "    }" ^
  "  }" ^
  "}" ^
  "$recent=@($rows | Where-Object { $_.profile -eq $target } | Select-Object -Last 3);" ^
  "$recentSummary='N/A';" ^
  "if($recent.Count -gt 0){" ^
  "  $parts=@();" ^
  "  foreach($row in $recent){ $parts += ($row.timestamp + ': ratio=' + $row.ratio.ToString('F3')) }" ^
  "  if($parts.Count -gt 0){ $recentSummary=($parts -join '; ') }" ^
  "}" ^
  "$frame='N/A'; $single=0.0; $seq=0.0; $async=0.0; $fallback=0.0; $foundBench=$false;" ^
  "if(Test-Path $benchPath){" ^
  "  $lines=Get-Content $benchPath;" ^
  "  foreach($line in $lines){" ^
  "    $trim=$line.Trim();" ^
  "    if(-not $trim.StartsWith('|')){ continue }" ^
  "    $cols=@($trim.Split('|') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' });" ^
  "    if($cols.Count -lt 10){ continue }" ^
  "    if($cols[0] -ne $target){ continue }" ^
  "    try{" ^
  "      $frame=$cols[1];" ^
  "      $single=[double]$cols[3];" ^
  "      $seq=[double]$cols[4];" ^
  "      $async=[double]$cols[5];" ^
  "      $fallback=[double]$cols[6];" ^
  "      $foundBench=$true;" ^
  "      break;" ^
  "    } catch { }" ^
  "  }" ^
  "}" ^
  "$benchSummary='N/A';" ^
  "if($foundBench -and $single -gt 0 -and $seq -gt 0 -and $async -gt 0){" ^
  "  $benchSummary=('frame=' + $frame + ', single=' + $single.ToString('F3') + ', seq=' + $seq.ToString('F3') + ', async=' + $async.ToString('F3') + ', async_fallback=' + $fallback.ToString('F3'))" ^
  "}" ^
  "$lines=@();" ^
  "$lines += '# GPU Coop Adoption Decision Report';" ^
  "$lines += '';" ^
  "$lines += '- decision: ' + $decision;" ^
  "$lines += '- policy: ' + $policy;" ^
  "$lines += '- adoption_gate: ' + $gate;" ^
  "$lines += '- exedit2_e2e_gate: ' + $e2eGate;" ^
  "$lines += '- benchmark_profile: ' + $target;" ^
  "$lines += '- single_gpu_fixed_window: ' + $fixedStart + ' to ' + $fixedEnd;" ^
  "$lines += '- next_gpu_coop_reevaluation: ' + $reevalDate + ' (' + $reevalStatus + ')';" ^
  "$lines += '- adoption_triggers: ' + $reevalTriggers;" ^
  "$lines += '- recent_ratio(3): ' + $recentSummary;" ^
  "$lines += '- latest_benchmark: ' + $benchSummary;" ^
  "$lines | Set-Content -Encoding utf8 $outPath;"
if errorlevel 1 exit /b 1

echo [generate_gpu_coop_decision_report] generated %REPORT%
exit /b 0
