param(
    [string]$RomPath = "D:\MAME\roms",
    [string]$MsysRoot = "C:\msys64",
    [string]$MameRoot = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($MameRoot)) {
    $MameRoot = Join-Path $root "external\mame"
}

$bash = Join-Path $MsysRoot "usr\bin\bash.exe"
$ucrtBin = Join-Path $MsysRoot "ucrt64\bin"
$openXrSdk = Join-Path $MsysRoot "ucrt64"
$onnxRuntimeDir = Join-Path $root "external\onnxruntime-1.28.0"
$buildScript = Join-Path $PSScriptRoot "build-mame-area51xr.sh"
$preflight = Join-Path $PSScriptRoot "preflight-area51-media.ps1"

function ConvertTo-MsysPath([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $matches[1].ToLowerInvariant()
        $tail = $matches[2] -replace '\\', '/'
        return "/$drive/$tail"
    }
    return ($full -replace '\\', '/')
}

foreach ($required in @($RomPath, $MameRoot, $bash, $ucrtBin, $buildScript, $preflight)) {
    if (-not (Test-Path $required)) {
        throw "Required play-build path not found: $required"
    }
}

Write-Host "[1/4] Applying current Area51XR MAME patch..."
& (Join-Path $PSScriptRoot "apply-mame-patch.ps1") -MameRoot $MameRoot

$env:A51XR_ROOT_MSYS = ConvertTo-MsysPath $root
$env:A51XR_MAME_ROOT_MSYS = ConvertTo-MsysPath $MameRoot
$env:A51XR_OPENXR_SDK_MSYS = ConvertTo-MsysPath $openXrSdk
$env:A51XR_ONNXRUNTIME_DIR_MSYS = ConvertTo-MsysPath $onnxRuntimeDir
$env:A51XR_MAME_JOBS = [string][Math]::Max(2, [Math]::Min(8, [Environment]::ProcessorCount))

Write-Host "[2/4] Incrementally building Area51XR host..."
$hostBuild = 'export PATH=/ucrt64/bin:/usr/bin:$PATH; cmake -S "$A51XR_ROOT_MSYS" -B "$A51XR_ROOT_MSYS/build-mingw" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DA51XR_OPENXR_SDK="$A51XR_OPENXR_SDK_MSYS" -DA51XR_ONNXRUNTIME_DIR="$A51XR_ONNXRUNTIME_DIR_MSYS" && cmake --build "$A51XR_ROOT_MSYS/build-mingw" --parallel'
& $bash -lc $hostBuild
if ($LASTEXITCODE -ne 0) {
    throw "Area51XR host build failed with exit code $LASTEXITCODE."
}

$packagedLoader = Join-Path $ucrtBin "libopenxr_loader.dll"
$loaderDll = Join-Path $root "build-mingw\openxr_loader.dll"
if (-not (Test-Path $packagedLoader)) {
    throw "MSYS2 OpenXR loader not found: $packagedLoader"
}
Copy-Item $packagedLoader $loaderDll -Force

Write-Host "[3/4] Incrementally building patched MAME..."
$buildScriptMsys = ConvertTo-MsysPath $buildScript
& $bash $buildScriptMsys
if ($LASTEXITCODE -ne 0) {
    throw "MAME play build failed with exit code $LASTEXITCODE."
}

Write-Host "[4/4] Verifying Area 51 media..."
& $preflight -RomPath $RomPath -MameRoot $MameRoot
if ($LASTEXITCODE -ne 0) {
    throw "Area 51 media preflight failed with exit code $LASTEXITCODE."
}

Write-Host "Play build preparation passed."
