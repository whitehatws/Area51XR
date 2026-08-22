param(
    [Parameter(Mandatory = $true)]
    [string]$MameExe,

    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [int]$DurationSeconds = 20,

    [switch]$Fire
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$hostExe = Join-Path $root "build-mingw\area51xr.exe"

function Test-MameMediaPath([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return $false }
    foreach ($entry in ($Value -split ';')) {
        if ([string]::IsNullOrWhiteSpace($entry) -or -not (Test-Path $entry)) { return $false }
    }
    return $true
}

if (-not (Test-Path $hostExe)) {
    throw "Area51XR host not found at '$hostExe'. Run smoke-test.ps1 first."
}
if (-not (Test-Path $MameExe)) {
    throw "Patched MAME executable not found at '$MameExe'."
}
if (-not (Test-MameMediaPath $RomPath)) {
    throw "MAME media path is invalid: $RomPath"
}
if ($DurationSeconds -lt 5) {
    throw "DurationSeconds must be at least 5."
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\live-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$hostOut = Join-Path $logDir "area51xr.log"
$hostErr = Join-Path $logDir "area51xr.err.log"
$mameOut = Join-Path $logDir "mame.log"
$mameErr = Join-Path $logDir "mame.err.log"

$hostArgs = @("--bridge", "0.5", "0.5")
if ($Fire) {
    $hostArgs += "--fire"
}

$host = $null
$mame = $null
try {
    Write-Host "Starting Area51XR bridge..."
    $host = Start-Process -FilePath $hostExe -ArgumentList $hostArgs -PassThru `
        -WorkingDirectory (Split-Path -Parent $hostExe) `
        -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

    Start-Sleep -Milliseconds 500
    if ($host.HasExited) {
        throw "Area51XR bridge exited early. See $hostErr"
    }

    Write-Host "Starting Area 51 in patched MAME..."
    $mameArgs = @("area51", "-rompath", $RomPath, "-window", "-verbose")
    $mame = Start-Process -FilePath $MameExe -ArgumentList $mameArgs -PassThru `
        -WorkingDirectory (Split-Path -Parent $MameExe) `
        -RedirectStandardOutput $mameOut -RedirectStandardError $mameErr

    $deadline = (Get-Date).AddSeconds($DurationSeconds)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500

        if (Test-Path $hostOut) {
            $hostText = Get-Content -Raw -Path $hostOut -ErrorAction SilentlyContinue
            if ($hostText -match "frame=(\d+) size=(\d+)x(\d+) bytes=(\d+)") {
                Write-Host "Bridge traffic detected: $($Matches[0])"
                Write-Host "LIVE MAME BRIDGE TEST PASSED"
                Write-Host "Logs: $logDir"
                return
            }
        }

        if ($mame.HasExited) {
            throw "MAME exited before framebuffer traffic was detected. See $mameErr"
        }
        if ($host.HasExited) {
            throw "Area51XR bridge exited before framebuffer traffic was detected. See $hostErr"
        }
    }

    throw "No Area51XR framebuffer traffic was detected within $DurationSeconds seconds. Logs: $logDir"
}
finally {
    if ($mame -and -not $mame.HasExited) {
        Stop-Process -Id $mame.Id -Force -ErrorAction SilentlyContinue
    }
    if ($host -and -not $host.HasExited) {
        Stop-Process -Id $host.Id -Force -ErrorAction SilentlyContinue
    }
}
