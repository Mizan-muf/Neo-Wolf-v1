<#
.SYNOPSIS
    Builds Neo-Wolf on Windows with Visual Studio 2022 or newer and runs all tests.

.DESCRIPTION
    1. Uses vswhere to find the newest Visual Studio 2022+ (any edition, including Build
       Tools) with the C++ x64 tools.
    2. Uses the CMake bundled with that Visual Studio, or else cmake on PATH, and checks
       that it is 3.25+ and has the Visual Studio generator for that installation.
    3. Configures with the windows-msvc preset, pinned to that Visual Studio (explicit -G and
       CMAKE_GENERATOR_INSTANCE), then builds and tests with the windows-msvc (Release) or
       windows-msvc-debug (Debug) build/test presets.

    The presets use CMake's Visual Studio generator, so this works from a plain PowerShell
    or cmd window; a Developer prompt is not required. SDL2 is downloaded (source tarball)
    and built statically by CMake (internet needed on the first configure).
    See docs/BUILD_WINDOWS.md.

.PARAMETER Config
    Release (default) or Debug.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Config Debug
#>
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

# Visual Studio major version -> CMake generator name. Extend this map (and use a CMake that
# has the new generator) when a new Visual Studio major version ships.
$VsGenerators = @{
    17 = "Visual Studio 17 2022"
    18 = "Visual Studio 18 2026"
}

$script:ExitCode = 0

function Fail([string]$Message, [int]$Code = 1) {
    $script:ExitCode = $Code
    throw $Message
}

function Invoke-Step([string]$Name, [string]$Exe, [string[]]$Arguments) {
    Write-Host "==> $Name"
    & $Exe @Arguments
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Fail "$Name failed (exit code $code)." $code
    }
}

Push-Location -LiteralPath $PSScriptRoot
try {
    # --- Visual Studio (2022 = major version 17, or newer) ------------------------------
    $programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
    $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        Fail ("vswhere.exe not found at '$vswhere'. Install Visual Studio 2022 or newer " +
              "(or Build Tools for Visual Studio) with the 'Desktop development with C++' workload.")
    }

    $vswhereArgs = @(
        "-nologo", "-latest", "-products", "*", "-version", "[17.0,)",
        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
    )
    $vsPath = & $vswhere @vswhereArgs -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace("$vsPath")) {
        Fail ("No Visual Studio 2022+ installation with the C++ x64 tools was found. In the " +
              "Visual Studio Installer, add the 'Desktop development with C++' workload.")
    }
    $vsPath = "$($vsPath | Select-Object -First 1)".Trim()
    $vsVersion = "$(& $vswhere @vswhereArgs -property installationVersion | Select-Object -First 1)".Trim()
    if ($vsVersion -match '^(\d+)\.') {
        $vsMajor = [int]$Matches[1]
    } else {
        Fail "Could not read the version of the Visual Studio at '$vsPath' (vswhere returned '$vsVersion')."
    }
    if (-not $VsGenerators.ContainsKey($vsMajor)) {
        Fail ("Visual Studio $vsVersion at '$vsPath' (major version $vsMajor) is newer than this " +
              "script knows. Update build.ps1 (the VsGenerators map) to a version that supports " +
              "it, and use a CMake that has a generator for it.")
    }
    $generator = $VsGenerators[$vsMajor]
    Write-Host "==> Visual Studio: $vsPath ($vsVersion)"

    # --- CMake: prefer the one bundled with that Visual Studio -------------------------------
    $bundled = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path -LiteralPath $bundled) {
        $cmake = $bundled
    } else {
        $cmake = (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -First 1).Source
        if (-not $cmake) {
            Fail ("Visual Studio's bundled CMake is missing and cmake is not on PATH. Add the " +
                  "'C++ CMake tools for Windows' component, or install CMake 3.25+ from cmake.org.")
        }
    }
    $ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
    if (-not (Test-Path -LiteralPath $ctest)) {
        Fail "ctest.exe not found next to '$cmake'."
    }
    $cmakeVersion = "$(& $cmake --version | Select-Object -First 1)"
    if ($cmakeVersion -match '(\d+)\.(\d+)') {
        $major = [int]$Matches[1]
        $minor = [int]$Matches[2]
        if ($major -lt 3 -or ($major -eq 3 -and $minor -lt 25)) {
            Fail ("$cmakeVersion at '$cmake' is too old: CMakePresets.json needs CMake 3.25 or " +
                  "newer. Update Visual Studio or install a current CMake from cmake.org.")
        }
    }
    $capabilities = (& $cmake -E capabilities | Out-String) | ConvertFrom-Json
    $knownGenerators = @($capabilities.generators | ForEach-Object { $_.name })
    if ($knownGenerators -notcontains $generator) {
        Fail ("$cmakeVersion at '$cmake' has no '$generator' generator. Use a newer CMake " +
              "(Visual Studio 2026 needs CMake 4.2 or newer), or update Visual Studio.")
    }
    Write-Host "==> CMake: $cmake ($cmakeVersion)"

    # Pin the generator and the Visual Studio instance found above. An inherited
    # CMAKE_GENERATOR* environment (for example CMAKE_GENERATOR=Ninja) must not interfere.
    # This only affects this PowerShell process.
    foreach ($name in @("CMAKE_GENERATOR", "CMAKE_GENERATOR_PLATFORM", "CMAKE_GENERATOR_TOOLSET", "CMAKE_GENERATOR_INSTANCE")) {
        [Environment]::SetEnvironmentVariable($name, $null, "Process")
    }

    # One multi-config configure (windows-msvc); Debug or Release is chosen at build/test time.
    $configurePreset = "windows-msvc"
    $preset = if ($Config -eq "Debug") { "windows-msvc-debug" } else { "windows-msvc" }
    Write-Host "==> Neo-Wolf: configure preset '$configurePreset', build/test preset '$preset' ($Config), generator '$generator'"

    Invoke-Step "Configure" $cmake @("--preset", $configurePreset, "-G", $generator, "-DCMAKE_GENERATOR_INSTANCE=$vsPath")
    Invoke-Step "Build" $cmake @("--build", "--preset", $preset)
    Invoke-Step "Test" $ctest @("--preset", $preset)

    Write-Host "==> Build and tests succeeded. Run the game from the repository root:" -ForegroundColor Green
    Write-Host "    .\build\$configurePreset\$Config\neo_wolf.exe"
}
catch {
    Write-Host "error: $($_.Exception.Message)" -ForegroundColor Red
    if ($script:ExitCode -eq 0) {
        $script:ExitCode = 1
    }
}
finally {
    Pop-Location
}
exit $script:ExitCode
