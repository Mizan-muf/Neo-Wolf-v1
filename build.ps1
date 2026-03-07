param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$vsVars = "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
$toolchain = Join-Path $PSScriptRoot "vcpkg\scripts\buildsystems\vcpkg.cmake"

if (-not (Test-Path $vsVars)) {
    throw "Missing vcvars64.bat at: $vsVars"
}

if (-not (Test-Path $toolchain)) {
    throw "Missing vcpkg toolchain file at: $toolchain"
}

$cmd = @(
    "`"$vsVars`"",
    "&& cmake -S . -B build -G `"NMake Makefiles`" -DCMAKE_TOOLCHAIN_FILE=`"$toolchain`" -DCMAKE_BUILD_TYPE=$Config",
    "&& cmake --build build"
) -join " "

cmd /c $cmd
