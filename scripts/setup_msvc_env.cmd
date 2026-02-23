@echo off

where cl >nul 2>&1
if not errorlevel 1 exit /b 0

set "VSVCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VSVCVARS%" (
  echo [setup_msvc_env] ERROR: cl.exe not found and vcvars64.bat not found: "%VSVCVARS%"
  exit /b 1
)

call "%VSVCVARS%"
if errorlevel 1 (
  echo [setup_msvc_env] ERROR: Failed to run vcvars64.bat.
  exit /b 1
)

exit /b 0
