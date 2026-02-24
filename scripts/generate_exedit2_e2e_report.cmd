@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

set "HISTORY_PATH=docs\reports\exedit2-e2e-history.csv"
set "REPORT_PATH=docs\reports\exedit2-e2e-status.md"
set "MAX_AGE_DAYS=%~1"
if "%MAX_AGE_DAYS%"=="" set "MAX_AGE_DAYS=30"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$historyPath='%HISTORY_PATH%';" ^
  "$reportPath='%REPORT_PATH%';" ^
  "$maxAge=[int]$env:MAX_AGE_DAYS;" ^
  "$rows=@();" ^
  "if(Test-Path $historyPath){ $rows=@(Import-Csv $historyPath) }" ^
  "$count=$rows.Count;" ^
  "$latest=$null;" ^
  "if($count -gt 0){ $latest=$rows[-1] }" ^
  "$gate='FAIL';" ^
  "$gateReason='no records';" ^
  "if($latest -ne $null){" ^
  "  $mustPass=@('cpu_naive','cpu_avx2','gpu_dx11');" ^
  "  $ok=$true;" ^
  "  foreach($k in $mustPass){ if($latest.$k -ne 'PASS'){ $ok=$false; $gateReason=($k + '=' + $latest.$k) } }" ^
  "  if($ok -and $latest.fallback -ne 'PASS' -and $latest.fallback -ne 'SKIP'){ $ok=$false; $gateReason=('fallback=' + $latest.fallback) }" ^
  "  if($ok -and $latest.gpu_coop -ne 'PASS' -and $latest.gpu_coop -ne 'SKIP'){ $ok=$false; $gateReason=('gpu_coop=' + $latest.gpu_coop) }" ^
  "  if($ok){" ^
  "    try { $ts=[datetimeoffset]::Parse($latest.timestamp) } catch { $ok=$false; $gateReason='invalid timestamp' }" ^
  "    if($ok){" ^
  "      $age=[int]((Get-Date) - $ts.LocalDateTime).TotalDays;" ^
  "      if($age -gt $maxAge){ $ok=$false; $gateReason=('stale record: age=' + $age + ' days') } else { $gateReason=('age=' + $age + ' days') }" ^
  "    }" ^
  "  }" ^
  "  if($ok){ $gate='PASS' }" ^
  "}" ^
  "$lines=@();" ^
  "$lines += '# ExEdit2 E2E Validation Status';" ^
  "$lines += '';" ^
  "$lines += '- records: ' + $count;" ^
  "$lines += '- gate: ' + $gate;" ^
  "$lines += '- gate_reason: ' + $gateReason;" ^
  "if($latest -ne $null){" ^
  "  $lines += '- latest_timestamp: ' + $latest.timestamp;" ^
  "  $lines += '- latest_operator: ' + $latest.operator;" ^
  "  $lines += '- latest_build: ' + $latest.build;" ^
  "  $lines += '- latest_gpu: ' + $latest.gpu;" ^
  "  $lines += '- latest_results: cpu_naive=' + $latest.cpu_naive + ', cpu_avx2=' + $latest.cpu_avx2 + ', gpu_dx11=' + $latest.gpu_dx11 + ', gpu_coop=' + $latest.gpu_coop + ', fallback=' + $latest.fallback;" ^
  "  if($latest.notes){ $lines += '- latest_notes: ' + $latest.notes }" ^
  "}" ^
  "$lines | Set-Content -Encoding utf8 $reportPath;"
if errorlevel 1 exit /b 1

echo [generate_exedit2_e2e_report] generated %REPORT_PATH%
exit /b 0
