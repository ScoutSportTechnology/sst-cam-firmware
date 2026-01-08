@echo off
setlocal EnableExtensions

REM ============================================================
REM  SST dev shell launcher (portable)
REM  - Project env vars: SST_CAM_FIRMWARE_*
REM  - Keeps console open when double-clicked
REM ============================================================

REM If double-clicked (no args), relaunch in a persistent window
if "%~1"=="" (
    start "SST Dev Shell" cmd.exe /c ""%~f0" __INVOKED__"
    exit /b 0
)

REM Resolve SST_CAM_FIRMWARE_ROOT
if defined SST_CAM_FIRMWARE_ROOT goto :ROOT_SET

REM Fallback: assume script lives in repo
for %%I in ("%~dp0.") do set "SST_CAM_FIRMWARE_ROOT=%%~fI"

:ROOT_SET

REM Locate vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :VSWHERE_MISSING

REM Query latest VS install that has MSVC tools
for /f "usebackq delims=" %%i in (`
  "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
`) do set "VSINSTALL=%%i"

if not defined VSINSTALL goto :VS_MISSING

set "SST_CAM_FIRMWARE_VSINSTALL=%VSINSTALL%"
set "SST_CAM_FIRMWARE_VCVARSALL=%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%SST_CAM_FIRMWARE_VCVARSALL%" goto :VCVARS_MISSING

echo.
echo ===============================================================
echo   ScoutSportTechnology - SST Cam Firmware Dev Shell
echo ===============================================================
echo   Repo  : %SST_CAM_FIRMWARE_ROOT%
echo   Time  : %DATE% %TIME%
echo ===============================================================
echo.

echo Toolchain:
echo   Visual Studio: %SST_CAM_FIRMWARE_VSINSTALL%
echo.
echo Pick your target. If you're unsure, choose x64.
echo.

echo.
echo Select toolchain architecture:
echo.
echo   Architecture      Type   Host    Target
echo   ------------      -------------------------------
echo   [1] x64           native:  x64   - x64    [default]
echo   [2] x86           native:  x86   - x86
echo   [3] arm64         native:  arm64 - arm64
echo   [4] x64_x86       cross :  x64   - x86
echo   [5] x64_arm64     cross :  x64   - arm64
echo   [6] x86_x64       cross :  x86   - x64
echo   [7] x86_arm64     cross :  x86   - arm64
echo   [8] arm64_x64     cross :  arm64 - x64
echo   [9] arm64_x86     cross :  arm64 - x86
echo.

choice /C 123456789 /N /T 15 /D 1 /M "Enter choice"

set "SST_CAM_FIRMWARE_ARCH=x64"
if errorlevel 9 set "SST_CAM_FIRMWARE_ARCH=arm64_x86"
if errorlevel 8 set "SST_CAM_FIRMWARE_ARCH=arm64_x64"
if errorlevel 7 set "SST_CAM_FIRMWARE_ARCH=x86_arm64"
if errorlevel 6 set "SST_CAM_FIRMWARE_ARCH=x86_x64"
if errorlevel 5 set "SST_CAM_FIRMWARE_ARCH=x64_arm64"
if errorlevel 4 set "SST_CAM_FIRMWARE_ARCH=x64_x86"
if errorlevel 3 set "SST_CAM_FIRMWARE_ARCH=arm64"
if errorlevel 2 set "SST_CAM_FIRMWARE_ARCH=x86"
if errorlevel 1 set "SST_CAM_FIRMWARE_ARCH=x64"

echo.
echo Using architecture: %SST_CAM_FIRMWARE_ARCH%
echo.

call "%SST_CAM_FIRMWARE_VCVARSALL%" %SST_CAM_FIRMWARE_ARCH%
if errorlevel 1 goto :VCVARS_FAILED

cd /d "%SST_CAM_FIRMWARE_ROOT%"

where code >nul 2>&1
if errorlevel 1 goto :CODE_MISSING

echo.
echo ===============================================================
echo   Environment ready.
echo.
echo   - MSVC toolchain initialized  : %SST_CAM_FIRMWARE_ARCH%
echo   - Project root                : %SST_CAM_FIRMWARE_ROOT%
echo   - VS Code                     : launched
echo.
echo   This window will close automatically after closing VS Code.
echo ===============================================================
echo.
code .
exit /b 0

:VSWHERE_MISSING
echo ERROR: vswhere not found at:
echo   %VSWHERE%
echo Install Visual Studio (or Build Tools). vswhere ships with it.
pause
exit /b 1

:VS_MISSING
echo ERROR: Visual Studio with C++ tools (MSVC x64/x86) not found.
echo Fix: Visual Studio Installer -> Modify -> enable "Desktop development with C++"
pause
exit /b 1

:VCVARS_MISSING
echo ERROR: vcvarsall.bat not found at:
echo   %SST_CAM_FIRMWARE_VCVARSALL%
pause
exit /b 1

:VCVARS_FAILED
echo ERROR: vcvarsall failed for arch "%SST_CAM_FIRMWARE_ARCH%".
pause
exit /b 1

:CODE_MISSING
echo ERROR: VS Code command 'code' not found in PATH.
echo Fix in VS Code: Command Palette -> "Shell Command: Install 'code' command in PATH"
pause
exit /b 1
