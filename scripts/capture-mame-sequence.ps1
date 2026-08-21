param(
    [Parameter(Mandatory = $true)]
    [string]$MameExe,

    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [string]$DepthModelPath = "",

    [string]$OutputDir = "",

    [int]$FrameCount = 8,

    [int]$IntervalMs = 500,

    [int]$StartupSeconds = 12,

    [int]$CaptureTimeoutMs = 10000
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$hostExe = Join-Path $root "build-mingw\area51xr.exe"
$captureExe = Join-Path $root "build-mingw\area51xr-capture.exe"
$reconstructExe = Join-Path $root "build-mingw\area51xr-reconstruct.exe"

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $root "logs\sequence-$stamp"
}

$required = @($hostExe, $captureExe, $reconstructExe, $MameExe, $RomPath)
if (-not [string]::IsNullOrWhiteSpace($DepthModelPath)) {
    $required += $DepthModelPath
}
foreach ($path in $required) {
    if (-not (Test-Path $path)) {
        throw "Required path not found: $path"
    }
}
if ($FrameCount -lt 1) {
    throw "FrameCount must be at least 1."
}
if ($IntervalMs -lt 0) {
    throw "IntervalMs must be non-negative."
}
if ($StartupSeconds -lt 3) {
    throw "StartupSeconds must be at least 3."
}
if ($CaptureTimeoutMs -lt 1000) {
    throw "CaptureTimeoutMs must be at least 1000."
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$hostOut = Join-Path $OutputDir "area51xr-bridge.log"
$hostErr = Join-Path $OutputDir "area51xr-bridge.err.log"
$mameOut = Join-Path $OutputDir "mame.log"
$mameErr = Join-Path $OutputDir "mame.err.log"
$manifestPath = Join-Path $OutputDir "manifest.jsonl"
$resultPath = Join-Path $OutputDir "result.txt"
$depthLabel = "synthetic"
if (-not [string]::IsNullOrWhiteSpace($DepthModelPath)) {
    $depthLabel = $DepthModelPath
}

$host = $null
$mame = $null
try {
    Write-Host "Starting Area51XR desktop bridge..."
    $host = Start-Process -FilePath $hostExe -ArgumentList @("--bridge", "0.5", "0.5") -PassThru `
        -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

    Start-Sleep -Milliseconds 750
    if ($host.HasExited) {
        throw "Area51XR bridge exited during startup. See $hostErr"
    }

    Write-Host "Starting Area 51 in patched MAME..."
    $mameArgs = @("area51", "-rompath", $RomPath, "-window", "-verbose")
    $mame = Start-Process -FilePath $MameExe -ArgumentList $mameArgs -PassThru `
        -RedirectStandardOutput $mameOut -RedirectStandardError $mameErr

    Start-Sleep -Seconds $StartupSeconds
    if ($mame.HasExited) {
        throw "MAME exited before capture. See $mameErr"
    }
    if ($host.HasExited) {
        throw "Area51XR bridge exited before capture. See $hostErr"
    }

    Remove-Item $manifestPath -Force -ErrorAction SilentlyContinue
    for ($i = 0; $i -lt $FrameCount; ++$i) {
        $index = "{0:D4}" -f $i
        $capturePath = Join-Path $OutputDir "frame-$index.a51cap"
        $objPath = Join-Path $OutputDir "frame-$index.obj"
        $qualityPath = Join-Path $OutputDir "frame-$index.quality.json"

        Write-Host "Capturing frame $($i + 1)/$FrameCount..."
        $captureArgs = @($capturePath, $CaptureTimeoutMs)
        if (-not [string]::IsNullOrWhiteSpace($DepthModelPath)) {
            $captureArgs += $DepthModelPath
        }
        & $captureExe @captureArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Bridge capture failed on frame $index with exit code $LASTEXITCODE."
        }

        & $reconstructExe $capturePath $objPath $qualityPath
        if ($LASTEXITCODE -ne 0) {
            throw "Reconstruction failed quality gates on frame $index with exit code $LASTEXITCODE. See $qualityPath"
        }

        $entry = "{`"index`":$i,`"capture`":`"$capturePath`",`"mesh`":`"$objPath`",`"quality`":`"$qualityPath`"}"
        Add-Content -Path $manifestPath -Value $entry

        if ($i + 1 -lt $FrameCount -and $IntervalMs -gt 0) {
            Start-Sleep -Milliseconds $IntervalMs
        }
    }

    $result = @(
        "DESKTOP RECONSTRUCTION SEQUENCE: PASS",
        "Frames:   $FrameCount",
        "Depth:    $depthLabel",
        "Manifest: $manifestPath",
        "Logs:     $OutputDir"
    ) -join [Environment]::NewLine
    Set-Content -Path $resultPath -Value $result
    Write-Host $result
    exit 0
}
finally {
    if ($mame -and -not $mame.HasExited) {
        Stop-Process -Id $mame.Id -Force -ErrorAction SilentlyContinue
    }
    if ($host -and -not $host.HasExited) {
        Stop-Process -Id $host.Id -Force -ErrorAction SilentlyContinue
    }
}
