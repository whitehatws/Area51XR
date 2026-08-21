param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [string]$MsysRoot = "C:\msys64",

    [string]$MameRoot = "",

    [string]$OpenXrSdk = "",

    [string]$OnnxRuntimeDir = "",

    [string]$DepthModelPath = "",

    [switch]$SkipVr
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
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

    $bootstrapArgs = @(
        "-MsysRoot", $MsysRoot,
        "-RomPath", $RomPath
    )
    if (-not [string]::IsNullOrWhiteSpace($MameRoot)) { $bootstrapArgs += @("-MameRoot", $MameRoot) }
    if (-not [string]::IsNullOrWhiteSpace($OpenXrSdk)) { $bootstrapArgs += @("-OpenXrSdk", $OpenXrSdk) }
    if (-not [string]::IsNullOrWhiteSpace($OnnxRuntimeDir)) { $bootstrapArgs += @("-OnnxRuntimeDir", $OnnxRuntimeDir) }
    if (-not [string]::IsNullOrWhiteSpace($DepthModelPath)) { $bootstrapArgs += @("-DepthModelPath", $DepthModelPath) }

    Write-Step "Running full bootstrap/build/model/capture sequence."
    & (Join-Path $PSScriptRoot "bootstrap-windows.ps1") @bootstrapArgs
    if ($LASTEXITCODE -ne 0) {
        throw "bootstrap-windows.ps1 failed with exit code $LASTEXITCODE"
    }

    if (-not $SkipVr) {
        $resolvedMameRoot = $MameRoot
        if ([string]::IsNullOrWhiteSpace($resolvedMameRoot)) {
            $resolvedMameRoot = Join-Path $root "external\mame"
        }
        $mameExe = Get-ChildItem -Path $resolvedMameRoot -Filter "*area51xr*.exe" -File -ErrorAction SilentlyContinue |
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
