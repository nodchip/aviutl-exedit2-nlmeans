@echo off
setlocal

call "%~dp0setup_msvc_env.cmd"
if errorlevel 1 exit /b 1

set "INPUT_HLSL=nlmeans_filter\exedit2\nlmeans_exedit2_cs.hlsl"
set "OUTPUT_CSO=nlmeans_filter\exedit2\nlmeans_exedit2_cs.cso"

if not exist "%INPUT_HLSL%" (
  echo [generate_exedit2_shader_cso] ERROR: input shader not found: %INPUT_HLSL%
  exit /b 1
)

where fxc.exe >nul 2>nul
if errorlevel 1 (
  echo [generate_exedit2_shader_cso] ERROR: fxc.exe not found in PATH.
  exit /b 1
)

fxc.exe /nologo /T cs_5_0 /E main /Fo "%OUTPUT_CSO%" "%INPUT_HLSL%"
if errorlevel 1 exit /b 1

echo [generate_exedit2_shader_cso] generated %OUTPUT_CSO%
exit /b 0
