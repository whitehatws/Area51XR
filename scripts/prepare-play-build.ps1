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
$onnxRuntimeBin = Join-Path $onnxRuntimeDir "runtimes\win-x64\native"
$hostBuildScript = Join-Path $PSScriptRoot "build-area51xr-host.sh"
$mameBuildScript = Join-Path $PSScriptRoot "build-mame-area51xr.sh"
$preflight = Join-Path $PSScriptRoot "preflight-area51-media.ps1"
$controlsPatch = Join-Path $PSScriptRoot "apply-mame-controls-patch.ps1"
$regressions = Join-Path $root "build-mingw\area51xr_tests.exe"

function ConvertTo-MsysPath([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $matches[1].ToLowerInvariant()
        $tail = $matches[2] -replace '\\', '/'
        return "/$drive/$tail"
    }
    return ($full -replace '\\', '/')
}

foreach ($required in @($RomPath, $MameRoot, $bash, $ucrtBin, $onnxRuntimeBin, $hostBuildScript, $mameBuildScript, $preflight, $controlsPatch)) {
    if (-not (Test-Path $required)) {
        throw "Required play-build path not found: $required"
    }
}

Write-Host "[1/5] Applying current Area51XR MAME patches..."
& (Join-Path $PSScriptRoot "apply-mame-patch.ps1") -MameRoot $MameRoot
& $controlsPatch -MameRoot $MameRoot

$env:A51XR_ROOT_MSYS = ConvertTo-MsysPath $root
$env:A51XR_MAME_ROOT_MSYS = ConvertTo-MsysPath $MameRoot
$env:A51XR_OPENXR_SDK_MSYS = ConvertTo-MsysPath $openXrSdk
$env:A51XR_ONNXRUNTIME_DIR_MSYS = ConvertTo-MsysPath $onnxRuntimeDir
$env:A51XR_MAME_JOBS = [string][Math]::Max(2, [Math]::Min(8, [Environment]::ProcessorCount))

# Native executables produced by the UCRT64 toolchain require the MinGW runtime
# DLLs to be visible to the Windows loader. Keep ONNX Runtime visible as well so
# the regression gate uses the same native environment as the full smoke test.
$runtimePaths = @($ucrtBin, $onnxRuntimeBin)
$currentPathParts = @($env:PATH -split ';')
foreach ($runtimePath in [array]::Reverse([object[]]$runtimePaths.Clone())) {
    # This loop body is intentionally empty; [array]::Reverse mutates in place.
}
foreach ($runtimePath in $runtimePaths) {
    if (-not ($currentPathParts -contains $runtimePath)) {
        $env:PATH = "$runtimePath;$env:PATH"
        $currentPathParts = @($env:PATH -split ';')
    }
}

Write-Host "[2/5] Incrementally building Area51XR host..."
& $bash (ConvertTo-MsysPath $hostBuildScript)
if ($LASTEXITCODE -ne 0) {
    throw "Area51XR host build failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path $regressions)) {
    throw "Area51XR regression runner not found after build: $regressions"
}
Write-Host "[3/5] Running Area51XR regression gate..."
& $regressions
$regressionExit = $LASTEXITCODE
if ($regressionExit -ne 0) {
    if ($regressionExit -eq -1073741515) {
        throw "Area51XR regression runner could not start because Windows could not load a required DLL (0xC0000135). UCRT64=$ucrtBin ONNX=$onnxRuntimeBin"
    }
    throw "Area51XR regressions failed with exit code $regressionExit."
}

$packagedLoader = Join-Path $ucrtBin "libopenxr_loader.dll"
$loaderDll = Join-Path $root "build-mingw\openxr_loader.dll"
if (-not (Test-Path $packagedLoader)) {
    throw "MSYS2 OpenXR loader not found: $packagedLoader"
}
Copy-Item $packagedLoader $loaderDll -Force

Write-Host "[4/5] Incrementally building patched MAME..."
& $bash (ConvertTo-MsysPath $mameBuildScript)
if ($LASTEXITCODE -ne 0) {
    throw "MAME play build failed with exit code $LASTEXITCODE."
}

Write-Host "[5/5] Verifying Area 51 media..."
& $preflight -RomPath $RomPath -MameRoot $MameRoot
if ($LASTEXITCODE -ne 0) {
    throw "Area 51 media preflight failed with exit code $LASTEXITCODE."
}

Write-Host "Play build preparation passed."
