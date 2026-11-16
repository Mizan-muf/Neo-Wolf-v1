@echo off
rem Neo-Wolf DevKit launcher for Windows (Command Prompt and PowerShell).
rem   tools\devkit\neo_devkit.cmd <command> [options]      (docs\DEVKIT_GUIDE.md)
rem Uses "python" when it is a real Python 3.8+, otherwise the Python launcher "py -3". The
rem Microsoft Store "python" stub (WindowsApps) only opens the Store and fails the check below.
rem The DevKit's exit code is passed on to the caller.
setlocal
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
if defined PYTHONPATH (set "PYTHONPATH=%REPO_ROOT%;%PYTHONPATH%") else (set "PYTHONPATH=%REPO_ROOT%")

set "NEO_PY="
python -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>&1
if not errorlevel 1 set "NEO_PY=python"
if not defined NEO_PY (
    py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>&1
    if not errorlevel 1 set "NEO_PY=py -3"
)
if not defined NEO_PY (
    echo neo_devkit: Python 3.8 or newer was not found ^(neither "python" nor "py -3"^). 1>&2
    echo Install it with: winget install Python.Python.3.12 ^(or from https://www.python.org/downloads/^) 1>&2
    endlocal & exit /b 9009
)

%NEO_PY% "%SCRIPT_DIR%devkit.py" %*
set "NEO_RC=%ERRORLEVEL%"
endlocal & exit /b %NEO_RC%
