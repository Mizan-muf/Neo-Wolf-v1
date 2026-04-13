@echo off
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

set "VS_VARS="
for %%E in (Professional Community Enterprise BuildTools) do (
  if not defined VS_VARS (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
      set "VS_VARS=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)
set "TOOLCHAIN=%~dp0vcpkg\scripts\buildsystems\vcpkg.cmake"

if not exist "%VS_VARS%" (
  echo Missing vcvars64.bat at: %VS_VARS%
  exit /b 1
)

if not exist "%TOOLCHAIN%" (
  echo Missing vcpkg toolchain at: %TOOLCHAIN%
  exit /b 1
)

call "%VS_VARS%" || exit /b 1

cmake -S . -B build -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 (
  if exist build\CMakeCache.txt (
    echo Detected incompatible CMake cache in build\. Removing and retrying configure...
    rmdir /s /q build
    cmake -S . -B build -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1
  ) else (
    exit /b 1
  )
)
cmake --build build || exit /b 1

echo Build completed: %CONFIG%
endlocal
