@echo off
setlocal

set "REEVALUATION_DATE=2026-03-31"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$target=[datetime]::ParseExact('%REEVALUATION_DATE%','yyyy-MM-dd',$null).Date;" ^
  "$today=(Get-Date).Date;" ^
  "$days=[int]($target - $today).TotalDays;" ^
  "if($days -gt 0){" ^
  "  Write-Host ('[check_gpu_coop_reevaluation_due] next reevaluation is in ' + $days + ' days (' + $target.ToString('yyyy-MM-dd') + ').'); exit 0" ^
  "} elseif($days -eq 0){" ^
  "  Write-Host ('[check_gpu_coop_reevaluation_due] reevaluation date is today (' + $target.ToString('yyyy-MM-dd') + ').'); exit 0" ^
  "} else {" ^
  "  Write-Host ('[check_gpu_coop_reevaluation_due] reevaluation is overdue by ' + (-1*$days) + ' days (' + $target.ToString('yyyy-MM-dd') + ').'); exit 0" ^
  "}"
if errorlevel 1 exit /b 1

exit /b 0
