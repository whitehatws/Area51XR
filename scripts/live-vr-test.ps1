param(
    [Parameter(Mandatory = $true)]
    [string]$MameExe,

    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [string]$MsysRoot = "C:\msys64",

    [int]$DurationSeconds = 45
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$hostExe = Join-Path $root "build-mingw\area51xr.exe"
$loader = Join-Path $root "build-mingw\openxr_loader.dll"
$ucrtBin = Join-Path $MsysRoot "ucrt64\bin"

function Test-MameMediaPath([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return $false }
    foreach ($entry in ($Value -split ';')) {
        if ([string]::IsNullOrWhiteSpace($entry) -or -not (Test-Path $entry)) { return $false }
    }
    return $true
}

if (-not (Test-Path $hostExe)) {
    throw "Area51XR host not found at '$hostExe'. Run bootstrap-windows.ps1 first."
}
if (-not (Test-Path $loader)) {
    throw "Khronos OpenXR loader not found at '$loader'. Run bootstrap-windows.ps1 first."
}
if (-not (Test-Path $MameExe)) {
    throw "Patched MAME executable not found at '$MameExe'."
}
if (-not (Test-MameMediaPath $RomPath)) {
    throw "MAME media path is invalid: $RomPath"
}
if (-not (Test-Path $ucrtBin)) {
    throw "MSYS2 UCRT64 runtime directory not found at '$ucrtBin'."
}
if ($DurationSeconds -lt 15) {
    throw "DurationSeconds must be at least 15."
}

if (-not (($env:PATH -split ';') -contains $ucrtBin)) {
    $env:PATH = "$ucrtBin;$env:PATH"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\vr-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$hostOut = Join-Path $logDir "area51xr.log"
$hostErr = Join-Path $logDir "area51xr.err.log"
$mameOut = Join-Path $logDir "mame.log"
$mameErr = Join-Path $logDir "mame.err.log"
$resultFile = Join-Path $logDir "result.txt"
$runtimeFile = Join-Path $logDir "openxr-runtime.txt"

$runtimeLines = New-Object System.Collections.Generic.List[string]
$runtimeLines.Add("OpenXR runtime inspection: $(Get-Date -Format o)")
foreach ($registryPath in @(
    "HKLM:\SOFTWARE\Khronos\OpenXR\1",
    "HKCU:\SOFTWARE\Khronos\OpenXR\1"
)) {
    try {
        $runtime = (Get-ItemProperty -Path $registryPath -Name ActiveRuntime -ErrorAction Stop).ActiveRuntime
        $runtimeLines.Add("$registryPath ActiveRuntime=$runtime")
    }
    catch {
        $runtimeLines.Add("$registryPath ActiveRuntime=<not set>")
    }
}
$runtimeLines | Set-Content -Path $runtimeFile -Encoding UTF8
Write-Host ($runtimeLines -join [Environment]::NewLine)

$hostProcess = $null
$mame = $null
try {
    Write-Host "Starting Area51XR OpenXR host..."
    $hostProcess = Start-Process -FilePath $hostExe -ArgumentList @("--xr-bridge") -PassThru `
        -WorkingDirectory (Split-Path -Parent $hostExe) `
        -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

    Start-Sleep -Milliseconds 750
    if ($hostProcess.HasExited) {
        $errorText = if (Test-Path $hostErr) { Get-Content -Raw $hostErr } else { "" }
        throw "Area51XR OpenXR host exited during startup. $errorText"
    }

    Write-Host "Starting Area 51 in patched MAME..."
    $mameArgs = @("area51", "-rompath", $RomPath, "-window", "-verbose")
    $mame = Start-Process -FilePath $MameExe -ArgumentList $mameArgs -PassThru `
        -WorkingDirectory (Split-Path -Parent $MameExe) `
        -RedirectStandardOutput $mameOut -RedirectStandardError $mameErr

    $deadline = (Get-Date).AddSeconds($DurationSeconds)
    $sessionSeen = $false
    $frameSeen = $false
    $trackedAimSeen = $false

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 400

        if ($hostProcess.HasExited) {
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
            return
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
    if ($hostProcess -and -not $hostProcess.HasExited) {
        Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
    }
}
