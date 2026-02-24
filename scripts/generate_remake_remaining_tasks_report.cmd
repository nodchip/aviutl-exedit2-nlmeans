@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0generate_remake_remaining_tasks_report.ps1"
if errorlevel 1 exit /b 1

exit /b 0
