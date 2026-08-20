param(
    [Parameter(Mandatory = $true)]
    [string]$MameExe,

    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [int]$DurationSeconds = 45
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$hostExe = Join-Path $root "build-mingw\area51xr.exe"
$loader = Join-Path $root "build-mingw\openxr_loader.dll"

if (-not (Test-Path $hostExe)) {
    throw "Area51XR host not found at '$hostExe'. Run bootstrap-windows.ps1 first."
}
if (-not (Test-Path $loader)) {
    throw "Khronos OpenXR loader not found at '$loader'. Run bootstrap-windows.ps1 first."
}
if (-not (Test-Path $MameExe)) {
    throw "Patched MAME executable not found at '$MameExe'."
}
if (-not (Test-Path $RomPath)) {
    throw "ROM path not found at '$RomPath'."
}
if ($DurationSeconds -lt 15) {
    throw "DurationSeconds must be at least 15."
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\vr-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$hostOut = Join-Path $logDir "area51xr.log"
$hostErr = Join-Path $logDir "area51xr.err.log"
$mameOut = Join-Path $logDir "mame.log"
$mameErr = Join-Path $logDir "mame.err.log"
$resultFile = Join-Path $logDir "result.txt"

$host = $null
$mame = $null
try {
    Write-Host "Starting Area51XR OpenXR host..."
    $host = Start-Process -FilePath $hostExe -ArgumentList @("--xr-bridge") -PassThru `
        -WorkingDirectory (Split-Path -Parent $hostExe) `
        -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

    Start-Sleep -Milliseconds 750
    if ($host.HasExited) {
        $errorText = if (Test-Path $hostErr) { Get-Content -Raw $hostErr } else { "" }
        throw "Area51XR OpenXR host exited during startup. $errorText"
    }

    Write-Host "Starting Area 51 in patched MAME..."
    $mameArgs = @("area51", "-rompath", $RomPath, "-window", "-verbose")
    $mame = Start-Process -FilePath $MameExe -ArgumentList $mameArgs -PassThru `
        -RedirectStandardOutput $mameOut -RedirectStandardError $mameErr

    $deadline = (Get-Date).AddSeconds($DurationSeconds)
    $sessionSeen = $false
    $frameSeen = $false
    $trackedAimSeen = $false

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 400

        if ($host.HasExited) {
            $errorText = if (Test-Path $hostErr) { Get-Content -Raw $hostErr } else { "" }
            throw "Area51XR OpenXR host exited early. $errorText"
        }
        if ($mame.HasExited) {
            $errorText = if (Test-Path $mameErr) { Get-Content -Raw $mameErr } else { "" }
            throw "MAME exited early. $errorText"
        }

        $hostText = if (Test-Path $hostOut) { Get-Content -Raw $hostOut -ErrorAction SilentlyContinue } else { "" }
        if ($hostText -match "xr_session=running") {
            $sessionSeen = $true
        }
        if ($hostText -match "frame=(\d+) size=(\d+)x(\d+)") {
            $frameSeen = $true
        }
        if ($hostText -match "aim=([0-9.]+),([0-9.]+) aim_valid=1") {
            $trackedAimSeen = $true
        }

        if ($sessionSeen -and $frameSeen -and $trackedAimSeen) {
            $result = @(
                "LIVE VR ACCEPTANCE TEST: PASS",
                "OpenXR session: running",
                "MAME framebuffer: detected",
                "Tracked controller aim: detected",
                "Logs: $logDir"
            ) -join [Environment]::NewLine
            Set-Content -Path $resultFile -Value $result
            Write-Host $result
            exit 0
        }
    }

    $missing = @()
    if (-not $sessionSeen) { $missing += "OpenXR session" }
    if (-not $frameSeen) { $missing += "MAME framebuffer" }
    if (-not $trackedAimSeen) { $missing += "tracked controller aim" }
    $result = "LIVE VR ACCEPTANCE TEST: INCOMPLETE`nMissing: $($missing -join ', ')`nLogs: $logDir"
    Set-Content -Path $resultFile -Value $result
    throw $result
}
finally {
    if ($mame -and -not $mame.HasExited) {
        Stop-Process -Id $mame.Id -Force -ErrorAction SilentlyContinue
    }
    if ($host -and -not $host.HasExited) {
        Stop-Process -Id $host.Id -Force -ErrorAction SilentlyContinue
    }
}
