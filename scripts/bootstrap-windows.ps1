param(
    [string]$MsysRoot = "C:\msys64",

    [string]$MameRoot = "",

    [string]$OpenXrSdk = "",

    [string]$RomPath = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($MameRoot)) {
    $MameRoot = Join-Path $root "external\mame"
}
if ([string]::IsNullOrWhiteSpace($OpenXrSdk)) {
    $OpenXrSdk = Join-Path $root "external\openxr-sdk"
}

$bash = Join-Path $MsysRoot "usr\bin\bash.exe"

if (-not (Test-Path $bash)) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "MSYS2 is not installed and winget is unavailable. Install MSYS2, then run this script again."
    }

    Write-Host "Installing MSYS2..."
    winget install --id MSYS2.MSYS2 -e --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $bash)) {
        throw "MSYS2 installation did not complete successfully."
    }
}

Write-Host "Installing the local build packages..."
$packageCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
pacman -S --needed --noconfirm git make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-lld
'@
& $bash -lc $packageCommand
if ($LASTEXITCODE -ne 0) {
    throw "MSYS2 package installation failed with exit code $LASTEXITCODE."
}

function Clone-IfMissing([string]$Target, [string]$Url, [string]$Label) {
    if (Test-Path (Join-Path $Target ".git")) {
        Write-Host "Using existing $Label source at $Target"
        return
    }

    Write-Host "Cloning $Label source..."
    $parent = Split-Path -Parent $Target
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $env:A51XR_CLONE_TARGET = $Target
    $env:A51XR_CLONE_URL = $Url
    $cloneCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
target="$(cygpath -u "$A51XR_CLONE_TARGET")"
git clone --depth 1 "$A51XR_CLONE_URL" "$target"
'@
    & $bash -lc $cloneCommand
    if ($LASTEXITCODE -ne 0) {
        throw "$Label source clone failed with exit code $LASTEXITCODE."
    }
}

Clone-IfMissing -Target $MameRoot -Url "https://github.com/mamedev/mame.git" -Label "MAME"
Clone-IfMissing -Target $OpenXrSdk -Url "https://github.com/KhronosGroup/OpenXR-SDK.git" -Label "OpenXR SDK"

Write-Host "Building and validating Area51XR + targeted MAME..."
& (Join-Path $PSScriptRoot "smoke-test.ps1") -MameRoot $MameRoot -MsysRoot $MsysRoot -OpenXrSdk $OpenXrSdk

if (-not [string]::IsNullOrWhiteSpace($RomPath)) {
    $mameExe = Get-ChildItem -Path $MameRoot -Filter "*area51xr*.exe" -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
    if (-not $mameExe) {
        throw "Patched MAME executable was not found after the build."
    }

    Write-Host "Running live Area 51 bridge test..."
    & (Join-Path $PSScriptRoot "live-mame-test.ps1") -MameExe $mameExe -RomPath $RomPath
}
else {
    Write-Host ""
    Write-Host "Build setup is complete."
    Write-Host "When your Area 51 game files are available, rerun with -RomPath <folder> to execute the live bridge test."
}
