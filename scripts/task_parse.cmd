@echo off
REM %1 = preset

set "PRESET=%~1"
set "TARGET="

if not "%PRESET:-x64=%"=="%PRESET%" set "TARGET=amd64"
if not "%PRESET:-x86=%"=="%PRESET%" set "TARGET=x86"
if not "%PRESET:-arm64=%"=="%PRESET%" set "TARGET=arm64"
if not "%PRESET:-arm=%"=="%PRESET%" set "TARGET=arm"

if "%TARGET%"=="" (
  echo ERROR: Could not infer target arch from preset "%PRESET%"
  exit /b 1
)

set "target_arch=%TARGET%"
exit /b 0
