param(
    [string]$MsysRoot = "C:\msys64",

    [string]$MameRoot = "",

    [string]$RomPath = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($MameRoot)) {
    $MameRoot = Join-Path $root "external\mame"
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

Write-Host "Installing/updating the local build packages..."
$packageCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
pacman -Sy --noconfirm
pacman -S --needed --noconfirm git make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-lld
'@
& $bash -lc $packageCommand
if ($LASTEXITCODE -ne 0) {
    throw "MSYS2 package installation failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path (Join-Path $MameRoot ".git"))) {
    Write-Host "Cloning MAME source..."
    $parent = Split-Path -Parent $MameRoot
    New-Item -ItemType Directory -Force -Path $parent | Out-Null

    $env:A51XR_MAME_ROOT = $MameRoot
    $cloneCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
target="$(cygpath -u "$A51XR_MAME_ROOT")"
git clone --depth 1 https://github.com/mamedev/mame.git "$target"
'@
    & $bash -lc $cloneCommand
    if ($LASTEXITCODE -ne 0) {
        throw "MAME source clone failed with exit code $LASTEXITCODE."
    }
}
else {
    Write-Host "Using existing MAME source at $MameRoot"
}

Write-Host "Building and validating Area51XR + targeted MAME..."
& (Join-Path $PSScriptRoot "smoke-test.ps1") -MameRoot $MameRoot -MsysRoot $MsysRoot

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
