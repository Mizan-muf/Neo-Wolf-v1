@echo off
setlocal

set "VERSION=%~1"
if "%VERSION%"=="" set "VERSION=1.0.0"

set "OUT_DIR=%~2"
if "%OUT_DIR%"=="" set "OUT_DIR=release"

echo Packaging Neo-Wolf v%VERSION% to %OUT_DIR%...
python tools\devkit\devkit.py package --out-dir "%OUT_DIR%" --version "%VERSION%"

echo Package generation finished.
endlocal
