param (
    [string]$Version = "1.0.0",
    [string]$OutDir = "release"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Host "Packaging Neo-Wolf v$Version to $OutDir..." -ForegroundColor Cyan
python "$ScriptDir\tools\devkit\devkit.py" package --out-dir $OutDir --version $Version
Write-Host "Packaging complete." -ForegroundColor Green
