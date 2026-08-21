param(
    [string]$MsysRoot = "C:\msys64",

    [string]$MameRoot = "",

    [string]$OpenXrSdk = "",

    [string]$OnnxRuntimeDir = "",

    [string]$DepthModelPath = "",

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
if ([string]::IsNullOrWhiteSpace($OnnxRuntimeDir)) {
    $OnnxRuntimeDir = Join-Path $root "external\onnxruntime-1.28.0"
}
if ([string]::IsNullOrWhiteSpace($DepthModelPath)) {
    $DepthModelPath = Join-Path $root "models\depth_anything_v2_vits.onnx"
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

function Download-File([string]$Url, [string]$Target, [string]$Label) {
    if (Test-Path $Target) {
        Write-Host "Using existing $Label at $Target"
        return
    }
    Write-Host "Downloading $Label..."
    $parent = Split-Path -Parent $Target
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Invoke-WebRequest -Uri $Url -OutFile $Target -UseBasicParsing
}

Clone-IfMissing -Target $MameRoot -Url "https://github.com/mamedev/mame.git" -Label "MAME"
Clone-IfMissing -Target $OpenXrSdk -Url "https://github.com/KhronosGroup/OpenXR-SDK.git" -Label "OpenXR SDK"

$ortHeader = Join-Path $OnnxRuntimeDir "build\native\include\onnxruntime_cxx_api.h"
$ortLib = Join-Path $OnnxRuntimeDir "runtimes\win-x64\native\onnxruntime.lib"
if (-not (Test-Path $ortHeader) -or -not (Test-Path $ortLib)) {
    $ortPackage = Join-Path $root "external\Microsoft.ML.OnnxRuntime.1.28.0.nupkg"
    $ortZip = Join-Path $root "external\Microsoft.ML.OnnxRuntime.1.28.0.zip"
    Download-File -Url "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime/1.28.0" `
        -Target $ortPackage -Label "ONNX Runtime 1.28.0"

    Write-Host "Extracting ONNX Runtime..."
    Copy-Item $ortPackage $ortZip -Force
    if (Test-Path $OnnxRuntimeDir) {
        Remove-Item -Recurse -Force $OnnxRuntimeDir
    }
    New-Item -ItemType Directory -Force -Path $OnnxRuntimeDir | Out-Null
    Expand-Archive -Path $ortZip -DestinationPath $OnnxRuntimeDir -Force
    Remove-Item $ortZip -Force -ErrorAction SilentlyContinue
}
if (-not (Test-Path $ortHeader) -or -not (Test-Path $ortLib)) {
    throw "ONNX Runtime package did not contain the expected Windows x64 C++ files."
}

$depthModelUrl = "https://huggingface.co/AXERA-TECH/Depth-Anything-V2/resolve/main/depth_anything_v2_vits.onnx?download=true"
$depthModelSha256 = "443e95f17819f347f5f987384b8cb7d7d18ed6af6ac46dec9b0152748ba7dfd0"
Download-File -Url $depthModelUrl -Target $DepthModelPath -Label "Depth Anything V2 Small ONNX model"
$modelHash = (Get-FileHash -Path $DepthModelPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($modelHash -ne $depthModelSha256) {
    throw "Depth model checksum mismatch. Expected $depthModelSha256, got $modelHash. Delete '$DepthModelPath' before retrying."
}

Write-Host "Building and validating Area51XR + targeted MAME..."
& (Join-Path $PSScriptRoot "smoke-test.ps1") `
    -MameRoot $MameRoot `
    -MsysRoot $MsysRoot `
    -OpenXrSdk $OpenXrSdk `
    -OnnxRuntimeDir $OnnxRuntimeDir `
    -DepthModelPath $DepthModelPath

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
    Write-Host "Depth model: $DepthModelPath"
    Write-Host "When Area 51 game files are available, rerun with -RomPath <folder> for the live bridge test."
}
