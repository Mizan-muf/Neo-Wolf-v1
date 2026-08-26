@echo off
rem Neo-Wolf DevKit Windows Launcher
set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%..\..\"
set "PYTHONPATH=%REPO_ROOT%;%PYTHONPATH%"
python "%SCRIPT_DIR%devkit.py" %*
