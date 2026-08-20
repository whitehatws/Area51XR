param(
    [Parameter(Mandatory = $true)]
    [string]$MameRoot,

    [string]$MsysRoot = "C:\msys64",

    [string]$OpenXrSdk = "",

    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$bash = Join-Path $MsysRoot "usr\bin\bash.exe"
$mameSource = Join-Path $MameRoot "src\mame\atari\jaguar.cpp"

if ([string]::IsNullOrWhiteSpace($OpenXrSdk)) {
    $OpenXrSdk = Join-Path $root "external\openxr-sdk"
}

if (-not (Test-Path $bash)) {
    throw "MSYS2 bash not found at '$bash'."
}
if (-not (Test-Path $mameSource)) {
    throw "MAME source tree not found at '$MameRoot'."
}
if (-not (Test-Path (Join-Path $OpenXrSdk "include\openxr\openxr.h"))) {
    throw "OpenXR SDK headers not found at '$OpenXrSdk'."
}

if ($Jobs -le 0) {
    $Jobs = [Math]::Max(2, [Environment]::ProcessorCount)
}

Write-Host "[1/5] Applying Area51XR MAME integration..."
& (Join-Path $PSScriptRoot "apply-mame-patch.ps1") -MameRoot $MameRoot

$env:A51XR_ROOT = $root
$env:A51XR_MAME_ROOT = $MameRoot
$env:A51XR_OPENXR_SDK = $OpenXrSdk

Write-Host "[2/5] Building Area51XR with MSYS2/UCRT and OpenXR..."
$hostBuildCommand = @"
export PATH=/ucrt64/bin:/usr/bin:`$PATH
root="`$(cygpath -u "`$A51XR_ROOT")"
sdk="`$(cygpath -u "`$A51XR_OPENXR_SDK")"
cmake -S "`$root" -B "`$root/build-mingw" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DA51XR_OPENXR_SDK="`$sdk"
cmake --build "`$root/build-mingw" -j$Jobs
"@
& $bash -lc $hostBuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "Area51XR build failed with exit code $LASTEXITCODE."
}

Write-Host "[3/5] Running Area51XR tests..."
$hostTestCommand = @"
export PATH=/ucrt64/bin:/usr/bin:`$PATH
root="`$(cygpath -u "`$A51XR_ROOT")"
ctest --test-dir "`$root/build-mingw" --output-on-failure
"@
& $bash -lc $hostTestCommand
if ($LASTEXITCODE -ne 0) {
    throw "Area51XR tests failed with exit code $LASTEXITCODE."
}

Write-Host "[4/5] Building targeted MAME CoJag subtarget..."
$mameBuildCommand = @"
export PATH=/ucrt64/bin:/usr/bin:`$PATH
cd "`$(cygpath -u "`$A51XR_MAME_ROOT")"
make SUBTARGET=area51xr SOURCES=src/mame/atari/jaguar.cpp REGENIE=1 -j$Jobs
"@
& $bash -lc $mameBuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "MAME build failed with exit code $LASTEXITCODE."
}

Write-Host "[5/5] Locating and validating the MAME executable..."
$candidates = @(
    (Join-Path $MameRoot "mamearea51xr.exe"),
    (Join-Path $MameRoot "area51xr.exe")
)
$mameExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $mameExe) {
    $mameExe = Get-ChildItem -Path $MameRoot -Filter "*area51xr*.exe" -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $mameExe) {
    throw "MAME build completed but the Area51XR subtarget executable was not found."
}

& $mameExe -listfull area51 | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "The patched MAME executable could not enumerate Area 51."
}

$hostExe = Join-Path $root "build-mingw\area51xr.exe"
if (-not (Test-Path $hostExe)) {
    throw "Area51XR diagnostic host was not found at '$hostExe'."
}

Write-Host ""
Write-Host "Smoke test passed."
Write-Host "Area51XR host: $hostExe"
Write-Host "Patched MAME:     $mameExe"
Write-Host "OpenXR backend:   compiled"
