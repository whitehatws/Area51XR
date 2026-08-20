param(
    [string]$MsysRoot = "C:\msys64",
    [string]$OpenXrSdk = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$cmake = Join-Path $MsysRoot "ucrt64\bin\cmake.exe"
$ninja = Join-Path $MsysRoot "ucrt64\bin\ninja.exe"

if (-not (Test-Path $cmake)) {
    throw "MSYS2 UCRT64 CMake not found at '$cmake'. Run bootstrap-windows.ps1 first."
}
if (-not (Test-Path $ninja)) {
    throw "MSYS2 UCRT64 Ninja not found at '$ninja'. Run bootstrap-windows.ps1 first."
}

$args = @(
    "-S", $root,
    "-B", $build,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
    "-DCMAKE_MAKE_PROGRAM=$ninja"
)
if (-not [string]::IsNullOrWhiteSpace($OpenXrSdk)) {
    $args += "-DA51XR_OPENXR_SDK=$OpenXrSdk"
}

& $cmake @args
if ($LASTEXITCODE -ne 0) {
    throw "Area51XR CMake configure failed with exit code $LASTEXITCODE."
}

& $cmake --build $build
if ($LASTEXITCODE -ne 0) {
    throw "Area51XR build failed with exit code $LASTEXITCODE."
}
