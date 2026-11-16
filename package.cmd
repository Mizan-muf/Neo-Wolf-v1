@echo off
rem Neo-Wolf release package for Windows (Roadmap Phase 9, docs\RELEASE.md):
rem   release\neo-wolf-<version>-windows-x64.zip and release\SHA256SUMS
rem The version comes from project(VERSION) in CMakeLists.txt.
rem
rem Usage: package.cmd [DevKit package options]
rem   package.cmd                          configure + build the package preset, install, archive
rem   package.cmd --out-dir dist --clean   another folder, without older archives
rem   package.cmd --expect-version 1.0.0   fail unless CMakeLists.txt says 1.0.0 (release tags)
rem All options: package.cmd --help. Needs CMake and Visual Studio 2022+ (docs\BUILD_WINDOWS.md).
rem Uses "python" when it is a real Python 3.8+, otherwise the Python launcher "py -3" (as
rem tools\devkit\neo_devkit.cmd does). The DevKit's exit code is passed on.
setlocal
set "SCRIPT_DIR=%~dp0"

set "NEO_ARG1=%~1"
if not defined NEO_ARG1 goto :find_python
if "%NEO_ARG1:~0,1%"=="-" goto :find_python
echo package.cmd: no version argument any more: the version comes from project(VERSION) in CMakeLists.txt. 1>&2
echo Options: --out-dir DIR, --expect-version X.Y.Z (see docs\RELEASE.md). 1>&2
endlocal & exit /b 2

:find_python
set "NEO_PY="
python -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>&1
if not errorlevel 1 set "NEO_PY=python"
if not defined NEO_PY (
    py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>&1
    if not errorlevel 1 set "NEO_PY=py -3"
)
if not defined NEO_PY (
    echo package.cmd: Python 3.8 or newer was not found ^(neither "python" nor "py -3"^). 1>&2
    echo Install it with: winget install Python.Python.3.12 ^(or from https://www.python.org/downloads/^) 1>&2
    endlocal & exit /b 9009
)

%NEO_PY% "%SCRIPT_DIR%tools\devkit\devkit.py" package %*
set "NEO_RC=%ERRORLEVEL%"
if not "%NEO_RC%"=="0" echo package.cmd: packaging failed with exit code %NEO_RC%. 1>&2
endlocal & exit /b %NEO_RC%
