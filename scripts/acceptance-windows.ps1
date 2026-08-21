param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [string]$MsysRoot = "C:\msys64",

    [string]$MameRoot = "",

    [string]$OpenXrSdk = "",

    [string]$OnnxRuntimeDir = "",

    [string]$DepthModelPath = "",

    [switch]$RunVr,

    [switch]$SkipVr
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

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$acceptanceDir = Join-Path $root "logs\acceptance-$stamp"
New-Item -ItemType Directory -Force -Path $acceptanceDir | Out-Null
$summaryPath = Join-Path $acceptanceDir "summary.txt"

function Write-Step([string]$Text) {
    $line = "[$(Get-Date -Format o)] $Text"
    Write-Host $line
    Add-Content -Path $summaryPath -Value $line
}

function Package-Diagnostics {
    try {
        & (Join-Path $PSScriptRoot "package-diagnostics.ps1") -InputDir $acceptanceDir | Out-Host
    }
    catch {
        Write-Host "Failed to package diagnostics: $_"
    }
}

try {
    Write-Step "Area51XR Windows acceptance started."
    Write-Step "ROM path: $RomPath"

    if ($RunVr -and $SkipVr) {
        throw "Use either -RunVr or -SkipVr, not both."
    }
    if ($RunVr) {
        Write-Step "VR acceptance explicitly enabled."
    }

    Write-Step "MAME root: $MameRoot"
    Write-Step "OpenXR SDK root: $OpenXrSdk"
    Write-Step "ONNX Runtime root: $OnnxRuntimeDir"
    Write-Step "Depth model: $DepthModelPath"
    Write-Step "Running full bootstrap/build/model/capture sequence."

    & (Join-Path $PSScriptRoot "bootstrap-windows.ps1") `
        -MsysRoot $MsysRoot `
        -MameRoot $MameRoot `
        -OpenXrSdk $OpenXrSdk `
        -OnnxRuntimeDir $OnnxRuntimeDir `
        -DepthModelPath $DepthModelPath `
        -RomPath $RomPath
    if ($LASTEXITCODE -ne 0) {
        throw "bootstrap-windows.ps1 failed with exit code $LASTEXITCODE"
    }

    if (-not $SkipVr) {
        $mameExe = Get-ChildItem -Path $MameRoot -Filter "*area51xr*.exe" -File -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
        if (-not $mameExe) {
            throw "Patched MAME executable was not found after bootstrap."
        }

        Write-Step "Running VR acceptance harness."
        & (Join-Path $PSScriptRoot "live-vr-test.ps1") -MameExe $mameExe -RomPath $RomPath
        if ($LASTEXITCODE -ne 0) {
            throw "live-vr-test.ps1 failed with exit code $LASTEXITCODE"
        }
    }
    else {
        Write-Step "VR acceptance skipped by request."
    }

    Write-Step "Area51XR Windows acceptance passed."
    Package-Diagnostics
    exit 0
}
catch {
    Write-Step "Area51XR Windows acceptance failed: $_"
    Package-Diagnostics
    throw
}
