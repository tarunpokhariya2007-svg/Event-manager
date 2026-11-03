# Configure, build and test on Windows (PowerShell). Usage: scripts\build_and_test.ps1 [-BuildDir build]
# Requires $env:VCPKG_ROOT pointing at a vcpkg checkout (manifest mode installs the dependencies).
param([string]$BuildDir = "build")
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
cmake -S . -B $BuildDir "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build $BuildDir --config Debug --parallel
ctest --test-dir $BuildDir -C Debug --output-on-failure
