@echo off
setlocal

set "OUT=docs\reports\gpu-runner-health.md"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$out='docs/reports/gpu-runner-health.md';" ^
  "$timestamp=(Get-Date).ToString('yyyy-MM-ddTHH:mm:ssK');" ^
  "$os=(Get-CimInstance Win32_OperatingSystem);" ^
  "$gpus=@(Get-CimInstance Win32_VideoController | Where-Object { $_.Name -ne $null -and $_.Name.Trim().Length -gt 0 });" ^
  "$msbuild=(Get-Command MSBuild.exe -ErrorAction SilentlyContinue);" ^
  "$cl=(Get-Command cl.exe -ErrorAction SilentlyContinue);" ^
  "$sdkHeader='nlmeans_filter/aviutl2_sdk/filter2.h';" ^
  "$sdkExists=Test-Path $sdkHeader;" ^
  "$dx12DllPath=Join-Path $env:WINDIR 'System32/d3d12.dll';" ^
  "$dx12DllExists=Test-Path $dx12DllPath;" ^
  "$lines=@();" ^
  "$lines += '# GPU Runner Health Report';" ^
  "$lines += '';" ^
  "$lines += ('- Timestamp: ' + $timestamp);" ^
  "$lines += ('- Computer: ' + $env:COMPUTERNAME);" ^
  "$lines += ('- OS: ' + $os.Caption + ' (' + $os.Version + ')');" ^
  "$lines += ('- ExEdit2 SDK header: ' + ($(if($sdkExists){'found'}else{'missing'})));" ^
  "$lines += ('- MSBuild.exe: ' + ($(if($msbuild){$msbuild.Source}else{'not found'})));" ^
  "$lines += ('- cl.exe: ' + ($(if($cl){$cl.Source}else{'not found'})));" ^
  "$lines += ('- d3d12.dll: ' + ($(if($dx12DllExists){'found (' + $dx12DllPath + ')'}else{'missing'})));" ^
  "$lines += ('- GPU count: ' + $gpus.Count);" ^
  "$lines += '';" ^
  "$lines += '## GPU Adapters';" ^
  "$lines += '';" ^
  "if($gpus.Count -eq 0){" ^
  "  $lines += '- none'" ^
  "} else {" ^
  "  for($i=0; $i -lt $gpus.Count; $i++){" ^
  "    $lines += ('- [' + $i + '] ' + $gpus[$i].Name)" ^
  "  }" ^
  "}" ^
  "$lines | Set-Content -Path $out -Encoding utf8;"
if errorlevel 1 exit /b 1

echo [generate_gpu_runner_health_report] generated %OUT%
exit /b 0
