@echo off
setlocal

if not exist docs\reports (
  mkdir docs\reports
)

set "REPORT=docs\reports\dx11-dx12-decision.md"
set "GATE_RESULT=FAIL"

call "%~dp0check_dx11_dx12_adoption_gate.cmd" >nul 2>nul
if not errorlevel 1 (
  set "GATE_RESULT=PASS"
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$benchPath='docs/reports/dx11-dx12-benchmark-history.csv';" ^
  "$qualityPath='docs/reports/dx11-dx12-quality-history.csv';" ^
  "$outPath='docs/reports/dx11-dx12-decision.md';" ^
  "$gateResult='%GATE_RESULT%';" ^
  "$now=(Get-Date).ToString('yyyy-MM-ddTHH:mm:ssK');" ^
  "$benchRows=@();" ^
  "if(Test-Path $benchPath){ $benchRows=@(Get-Content $benchPath | Where-Object { $_ -and -not $_.StartsWith('timestamp,') }) }" ^
  "$qualityRows=@();" ^
  "if(Test-Path $qualityPath){ $qualityRows=@(Get-Content $qualityPath | Where-Object { $_ -and -not $_.StartsWith('timestamp,') }) }" ^
  "$recentBench=$benchRows | Select-Object -Last 3;" ^
  "$benchSummary='N/A';" ^
  "if($recentBench.Count -gt 0){" ^
  "  $parts=@();" ^
  "  foreach($row in $recentBench){ $cols=$row.Split(','); if($cols.Count -ge 4){ $dx11=[double]$cols[1]; $dx12=[double]$cols[3]; $ratio=0.0; if($dx11 -gt 0){ $ratio=$dx12/$dx11 }; $parts += ($cols[0] + ': dx11=' + $dx11.ToString('F3') + ', dx12=' + $dx12.ToString('F3') + ', ratio=' + $ratio.ToString('F3')) } }" ^
  "  if($parts.Count -gt 0){ $benchSummary=($parts -join '; ') }" ^
  "}" ^
  "$qualitySummary='N/A';" ^
  "if($qualityRows.Count -gt 0){" ^
  "  $q=$qualityRows[$qualityRows.Count-1].Split(',');" ^
  "  if($q.Count -ge 5){ $qualitySummary=('copy(max=' + $q[1] + ', mean=' + $q[2] + '), compute(max=' + $q[3] + ', mean=' + $q[4] + ')') }" ^
  "}" ^
  "$lines=@();" ^
  "$lines += '# DX11/DX12 Adoption Decision Report';" ^
  "$lines += '';" ^
  "$lines += '- generated_at: ' + $now;" ^
  "$lines += '- adoption_gate: ' + $gateResult;" ^
  "$lines += '- recent_benchmark(3): ' + $benchSummary;" ^
  "$lines += '- latest_quality: ' + $qualitySummary;" ^
  "$lines | Set-Content -Encoding utf8 $outPath;"
if errorlevel 1 exit /b 1

echo [generate_dx11_dx12_decision_report] generated %REPORT%
exit /b 0
