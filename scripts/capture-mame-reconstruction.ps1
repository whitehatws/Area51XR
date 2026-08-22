param(
    [Parameter(Mandatory = $true)]
    [string]$MameExe,

    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [string]$DepthModelPath = "",

    [string]$OutputDir = "",

    [int]$StartupSeconds = 12,

    [int]$CaptureTimeoutMs = 10000
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$hostExe = Join-Path $root "build-mingw\area51xr.exe"
$captureExe = Join-Path $root "build-mingw\area51xr-capture.exe"
$reconstructExe = Join-Path $root "build-mingw\area51xr-reconstruct.exe"

function Test-MameMediaPath([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return $false }
    foreach ($entry in ($Value -split ';')) {
        if ([string]::IsNullOrWhiteSpace($entry) -or -not (Test-Path $entry)) { return $false }
    }
    return $true
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $root "logs\capture-$stamp"
}

$required = @($hostExe, $captureExe, $reconstructExe, $MameExe)
if (-not [string]::IsNullOrWhiteSpace($DepthModelPath)) {
    $required += $DepthModelPath
}
foreach ($path in $required) {
    if (-not (Test-Path $path)) {
        throw "Required path not found: $path"
    }
}
if (-not (Test-MameMediaPath $RomPath)) {
    throw "MAME media path is invalid: $RomPath"
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
$capturePath = Join-Path $OutputDir "frame.a51cap"
$objPath = Join-Path $OutputDir "frame.obj"
$qualityPath = Join-Path $OutputDir "quality.json"
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
        -WorkingDirectory (Split-Path -Parent $hostExe) `
        -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

    Start-Sleep -Milliseconds 750
    if ($host.HasExited) {
        throw "Area51XR bridge exited during startup. See $hostErr"
    }

    Write-Host "Starting Area 51 in patched MAME..."
    $mameArgs = @("area51", "-rompath", $RomPath, "-window", "-verbose")
    $mame = Start-Process -FilePath $MameExe -ArgumentList $mameArgs -PassThru `
        -WorkingDirectory (Split-Path -Parent $MameExe) `
        -RedirectStandardOutput $mameOut -RedirectStandardError $mameErr

    Start-Sleep -Seconds $StartupSeconds
    if ($mame.HasExited) {
        throw "MAME exited before capture. See $mameErr"
    }
    if ($host.HasExited) {
        throw "Area51XR bridge exited before capture. See $hostErr"
    }

    Write-Host "Capturing shared framebuffer..."
    $captureArgs = @($capturePath, $CaptureTimeoutMs)
    if (-not [string]::IsNullOrWhiteSpace($DepthModelPath)) {
        $captureArgs += $DepthModelPath
    }
    & $captureExe @captureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Bridge capture failed with exit code $LASTEXITCODE."
    }

    Write-Host "Reconstructing captured frame..."
    & $reconstructExe $capturePath $objPath $qualityPath
    if ($LASTEXITCODE -ne 0) {
        throw "Captured reconstruction failed quality gates with exit code $LASTEXITCODE. See $qualityPath"
    }

    $result = @(
        "DESKTOP RECONSTRUCTION CAPTURE: PASS",
        "Capture: $capturePath",
        "Mesh:    $objPath",
        "Quality: $qualityPath",
        "Depth:   $depthLabel",
        "Logs:    $OutputDir"
    ) -join [Environment]::NewLine
    Set-Content -Path $resultPath -Value $result
    Write-Host $result
    return
}
finally {
    if ($mame -and -not $mame.HasExited) {
        Stop-Process -Id $mame.Id -Force -ErrorAction SilentlyContinue
    }
    if ($host -and -not $host.HasExited) {
        Stop-Process -Id $host.Id -Force -ErrorAction SilentlyContinue
    }
}
