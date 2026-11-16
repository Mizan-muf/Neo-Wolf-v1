@echo off
rem Neo-Wolf Windows build: configure, build and test with Visual Studio 2022 or newer.
rem Usage: build.cmd [Release^|Debug]
rem All the work is done by build.ps1 (vswhere lookup, CMake presets, CTest). This wrapper
rem lets the build be started from cmd.exe; run it from a console window.
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
if /I "%CONFIG%"=="release" set "CONFIG=Release"
if /I "%CONFIG%"=="debug" set "CONFIG=Debug"
if not "%CONFIG%"=="Release" if not "%CONFIG%"=="Debug" (
  echo error: unknown configuration "%CONFIG%" ^(expected Release or Debug^)
  exit /b 2
)

where powershell.exe >nul 2>nul
if errorlevel 1 (
  echo error: powershell.exe was not found on PATH.
  exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Config %CONFIG%
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
  echo error: build failed with exit code %RESULT%.
)
endlocal & exit /b %RESULT%
