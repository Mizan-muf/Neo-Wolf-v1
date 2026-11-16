<#
.SYNOPSIS
    Builds the Neo-Wolf release package for Windows (Roadmap Phase 9, docs\RELEASE.md).

.DESCRIPTION
    Runs `tools\devkit\devkit.py package`, which configures and builds the package-windows-msvc
    preset, installs it into a staging folder, checks it (the executable must be there) and writes
    release\neo-wolf-<version>-windows-x64.zip and release\SHA256SUMS. The version comes from
    project(VERSION) in CMakeLists.txt. All arguments are passed to the DevKit; its exit code is
    passed on. Uses "python" when it is a real Python 3.8+, otherwise "py -3".

.EXAMPLE
    .\package.ps1
    .\package.ps1 --out-dir dist --clean
    .\package.ps1 --expect-version 1.0.0
#>

$devkit = Join-Path $PSScriptRoot "tools\devkit\devkit.py"
$versionCheck = "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)"

if ($args.Count -gt 0 -and -not "$($args[0])".StartsWith("-")) {
    [Console]::Error.WriteLine("package.ps1: no version argument any more: the version comes from " +
        "project(VERSION) in CMakeLists.txt. Options: --out-dir DIR, --expect-version X.Y.Z (docs\RELEASE.md).")
    exit 2
}

$python = $null
foreach ($candidate in @(@("python"), @("py", "-3"))) {
    if (-not (Get-Command $candidate[0] -ErrorAction SilentlyContinue)) { continue }
    $prefix = @($candidate | Select-Object -Skip 1)
    & $candidate[0] @prefix -c $versionCheck *> $null
    if ($LASTEXITCODE -eq 0) { $python = $candidate; break }
}
if ($null -eq $python) {
    [Console]::Error.WriteLine('package.ps1: Python 3.8 or newer was not found (neither "python" nor "py -3"). ' +
        'Install it with: winget install Python.Python.3.12')
    exit 9009
}

$prefix = @($python | Select-Object -Skip 1)
& $python[0] @prefix $devkit package @args
$code = $LASTEXITCODE
if ($code -ne 0) {
    [Console]::Error.WriteLine("package.ps1: packaging failed with exit code $code.")
}
exit $code
