@echo off
setlocal

if not exist docs\reports\dx11-dx12-benchmark.md (
  call "%~dp0generate_dx11_dx12_benchmark.cmd"
  if errorlevel 1 exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$path='docs/reports/dx11-dx12-benchmark.md';" ^
  "$dx11=-1.0; $copy=-1.0; $compute=-1.0;" ^
  "$lines=Get-Content $path;" ^
  "foreach($line in $lines){" ^
  "  if($line -match '^\| DX11 Runner \| ([0-9]+(\.[0-9]+)?) \|$'){ $dx11=[double]$Matches[1] }" ^
  "  if($line -match '^\| DX11 Runner \| N/A \|$'){ $dx11=-1.0 }" ^
  "  if($line -match '^\| DX12 PoC Single Frame \(copy path\) \| ([0-9]+(\.[0-9]+)?) \|$'){ $copy=[double]$Matches[1] }" ^
  "  if($line -match '^\| DX12 PoC Single Frame \(compute path\) \| ([0-9]+(\.[0-9]+)?) \|$'){ $compute=[double]$Matches[1] }" ^
  "}" ^
  "if($copy -lt 0 -or $compute -lt 0){ Write-Error '[check_dx11_dx12_benchmark_threshold] target rows not found.'; exit 1 }" ^
  "$copyThreshold=5.0;" ^
  "$computeThreshold=1000.0;" ^
  "$dx11Threshold=20.0;" ^
  "if($copy -gt $copyThreshold){ Write-Error ('[check_dx11_dx12_benchmark_threshold] copy threshold exceeded: ' + $copy + ' > ' + $copyThreshold); exit 1 }" ^
  "if($compute -gt $computeThreshold){ Write-Error ('[check_dx11_dx12_benchmark_threshold] compute threshold exceeded: ' + $compute + ' > ' + $computeThreshold); exit 1 }" ^
  "if($dx11 -ge 0 -and $dx11 -gt $dx11Threshold){ Write-Error ('[check_dx11_dx12_benchmark_threshold] dx11 threshold exceeded: ' + $dx11 + ' > ' + $dx11Threshold); exit 1 }" ^
  "Write-Host ('[check_dx11_dx12_benchmark_threshold] ok. dx11=' + $dx11.ToString('F3') + ' copy=' + $copy.ToString('F3') + ' compute=' + $compute.ToString('F3'));"
if errorlevel 1 exit /b 1

exit /b 0
