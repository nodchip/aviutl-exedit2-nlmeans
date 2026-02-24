@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

set "REPORT=docs\reports\gpu-coop-decision.md"
set "GATE_RESULT=FAIL"
set "E2E_GATE_RESULT=FAIL"

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
  "$fixedStart='2026-02-23';" ^
  "$fixedEnd='2026-03-31';" ^
  "$reevalDate='2026-03-31';" ^
  "$rows=@();" ^
  "if(Test-Path $historyPath){ $rows=@(Get-Content $historyPath | Where-Object { $_ -and -not $_.StartsWith('timestamp,') }) }" ^
  "$recent=$rows | Select-Object -Last 3;" ^
  "$recentSummary='N/A';" ^
  "if($recent.Count -gt 0){" ^
  "  $parts=@();" ^
  "  foreach($row in $recent){ $cols=$row.Split(','); if($cols.Count -ge 4){ $parts += ($cols[0] + ': ratio=' + ([double]$cols[3]).ToString('F3')) } }" ^
  "  if($parts.Count -gt 0){ $recentSummary=($parts -join '; ') }" ^
  "}" ^
  "$single=0.0; $seq=0.0; $async=0.0; $fallback=0.0;" ^
  "if(Test-Path $benchPath){" ^
  "  $lines=Get-Content $benchPath;" ^
  "  foreach($line in $lines){" ^
  "    if($line -match '^\| Single GPU Full Frame \| ([0-9]+(\.[0-9]+)?) \|$'){ $single=[double]$Matches[1] }" ^
  "    if($line -match '^\| Coop PoC \(2 tiles, sequential dispatch\) \| ([0-9]+(\.[0-9]+)?) \|$'){ $seq=[double]$Matches[1] }" ^
  "    if($line -match '^\| Coop PoC \(2 tiles, async dispatch\) \| ([0-9]+(\.[0-9]+)?) \|$'){ $async=[double]$Matches[1] }" ^
  "    if($line -match '^\| Coop PoC \(2 tiles, async \+ single fallback\) \| ([0-9]+(\.[0-9]+)?) \|$'){ $fallback=[double]$Matches[1] }" ^
  "  }" ^
  "}" ^
  "$benchSummary='N/A';" ^
  "if($single -gt 0 -and $seq -gt 0 -and $async -gt 0){" ^
  "  $benchSummary=('single=' + $single.ToString('F3') + ', seq=' + $seq.ToString('F3') + ', async=' + $async.ToString('F3') + ', async_fallback=' + $fallback.ToString('F3'))" ^
  "}" ^
  "$today=(Get-Date).Date;" ^
  "$reeval=[datetime]::ParseExact($reevalDate,'yyyy-MM-dd',$null);" ^
  "$days=[int]($reeval - $today).TotalDays;" ^
  "$reevalStatus='due today';" ^
  "if($days -gt 0){ $reevalStatus=('in ' + $days + ' days') }" ^
  "if($days -lt 0){ $reevalStatus=('overdue by ' + (-1*$days) + ' days') }" ^
  "$lines=@();" ^
  "$lines += '# GPU Coop Adoption Decision Report';" ^
  "$lines += '';" ^
  "$lines += '- adoption_gate: ' + $gate;" ^
  "$lines += '- exedit2_e2e_gate: ' + $e2eGate;" ^
  "$lines += '- single_gpu_fixed_window: ' + $fixedStart + ' to ' + $fixedEnd;" ^
  "$lines += '- next_gpu_coop_reevaluation: ' + $reevalDate + ' (' + $reevalStatus + ')';" ^
  "$lines += '- policy: keep single-GPU default until adoption_gate becomes PASS.';" ^
  "$lines += '- adoption_triggers: last 3 history ratios <= 1.500, async/single <= 1.200, no regression check failure.';" ^
  "$lines += '- recent_ratio(3): ' + $recentSummary;" ^
  "$lines += '- latest_benchmark: ' + $benchSummary;" ^
  "$lines | Set-Content -Encoding utf8 $outPath;"
if errorlevel 1 exit /b 1

echo [generate_gpu_coop_decision_report] generated %REPORT%
exit /b 0
