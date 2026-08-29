param(
    [string]$RomPath = "D:\MAME\roms",
    [string]$MsysRoot = "C:\msys64",
    [string]$MameRoot = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($MameRoot)) {
    $MameRoot = Join-Path $root "external\mame"
}

$hostExe = Join-Path $root "build-mingw\area51xr.exe"
$ucrtBin = Join-Path $MsysRoot "ucrt64\bin"
$resolver = Join-Path $PSScriptRoot "resolve-mame-media.ps1"
$preparePlay = Join-Path $PSScriptRoot "prepare-play-build.ps1"

if (-not (Test-Path $RomPath)) {
    throw "ROM path not found: $RomPath"
}
if (-not (Test-Path $MameRoot)) {
    throw "MAME source root not found: $MameRoot"
}
if (-not (Test-Path $resolver)) {
    throw "Media resolver not found: $resolver"
}
if (-not (Test-Path $preparePlay)) {
    throw "Play-build preparation script not found: $preparePlay"
}

if (-not $SkipBuild) {
    Write-Host "Preparing focused Area51XR VR play build..."
    & $preparePlay -MameRoot $MameRoot -MsysRoot $MsysRoot -RomPath $RomPath
}

if (-not (Test-Path $hostExe)) {
    throw "Area51XR host not found: $hostExe"
}
if (-not (Test-Path $ucrtBin)) {
    throw "MSYS2 UCRT64 runtime not found: $ucrtBin"
}
if (-not (($env:PATH -split ';') -contains $ucrtBin)) {
    $env:PATH = "$ucrtBin;$env:PATH"
}

$mameExe = @(
    (Join-Path $MameRoot "mamearea51xr.exe"),
    (Join-Path $MameRoot "area51xr.exe")
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $mameExe) {
    $mameExe = Get-ChildItem -Path $MameRoot -Filter "*area51xr*.exe" -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $mameExe) {
    throw "Patched Area51XR MAME executable was not found."
}

$resolvedOutput = @(& $resolver -RomPath $RomPath)
$mediaPath = $env:A51XR_MAME_MEDIA_PATH
if ([string]::IsNullOrWhiteSpace($mediaPath) -and $resolvedOutput.Count -gt 0) {
    $mediaPath = [string]$resolvedOutput[$resolvedOutput.Count - 1]
}
if ([string]::IsNullOrWhiteSpace($mediaPath)) {
    throw "Unable to resolve MAME media path."
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\play-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$hostOut = Join-Path $logDir "area51xr.log"
$hostErr = Join-Path $logDir "area51xr.err.log"
$mameOut = Join-Path $logDir "mame.log"
$mameErr = Join-Path $logDir "mame.err.log"
$runtimeLog = Join-Path $logDir "openxr-runtime.txt"

$runtimeLines = New-Object System.Collections.Generic.List[string]
$runtimeLines.Add("Area51XR play launch: $(Get-Date -Format o)")
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
$runtimeLines | Set-Content -Path $runtimeLog -Encoding UTF8
Write-Host ($runtimeLines -join [Environment]::NewLine)

$hostProcess = $null
$mameProcess = $null
try {
    Write-Host ""
    Write-Host "Starting Area51XR OpenXR host..."
    $hostProcess = Start-Process -FilePath $hostExe -ArgumentList @("--xr-bridge") -PassThru `
        -WorkingDirectory (Split-Path -Parent $hostExe) `
        -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

    Start-Sleep -Milliseconds 900
    if ($hostProcess.HasExited) {
        $errorText = if (Test-Path $hostErr) { Get-Content -Raw $hostErr } else { "" }
        throw "Area51XR OpenXR host exited during startup. $errorText"
    }

    Write-Host "Starting Area 51 in patched MAME..."
    $mameArgs = @(
        "area51",
        "-rompath", $mediaPath,
        "-window",
        "-skip_gameinfo",
        "-verbose"
    )
    $mameProcess = Start-Process -FilePath $mameExe -ArgumentList $mameArgs -PassThru `
        -WorkingDirectory (Split-Path -Parent $mameExe) `
        -RedirectStandardOutput $mameOut -RedirectStandardError $mameErr

    Write-Host ""
    Write-Host "AREA51XR PLAY MODE STARTED"
    Write-Host "Put on the Quest 3 and keep Virtual Desktop / VDXR connected."
    Write-Host "Right controller: aim + trigger."
    Write-Host "Keyboard fallback: 5 = coin, 1 = Player 1 start."
    Write-Host "Leave this PowerShell window open while playing."
    Write-Host "Press Ctrl+C here, or exit MAME, to stop the session."
    Write-Host "Logs: $logDir"

    $reportedSession = $false
    $reportedFrame = $false
    $reportedAim = $false
    while ($true) {
        Start-Sleep -Milliseconds 500

        if ($hostProcess.HasExited) {
            $errorText = if (Test-Path $hostErr) { Get-Content -Raw $hostErr } else { "" }
            throw "Area51XR OpenXR host exited. $errorText"
        }
        if ($mameProcess.HasExited) {
            Write-Host "MAME exited with code $($mameProcess.ExitCode)."
            if ($mameProcess.ExitCode -ne 0 -and (Test-Path $mameErr)) {
                Write-Host (Get-Content -Raw $mameErr)
            }
            break
        }

        $hostText = if (Test-Path $hostOut) { Get-Content -Raw $hostOut -ErrorAction SilentlyContinue } else { "" }
        if (-not $reportedSession -and $hostText -match "xr_session=running") {
            $reportedSession = $true
            Write-Host "OpenXR session is running."
        }
        if (-not $reportedFrame -and $hostText -match "frame=(\d+) size=(\d+)x(\d+)") {
            $reportedFrame = $true
            Write-Host "Area 51 framebuffer is reaching VR: $($Matches[0])"
        }
        if (-not $reportedAim -and $hostText -match "aim=([0-9.]+),([0-9.]+) aim_valid=1") {
            $reportedAim = $true
            Write-Host "Right-controller aiming is active."
        }
    }
}
finally {
    if ($mameProcess -and -not $mameProcess.HasExited) {
        Stop-Process -Id $mameProcess.Id -Force -ErrorAction SilentlyContinue
    }
    if ($hostProcess -and -not $hostProcess.HasExited) {
        Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
    }
    Remove-Item Env:A51XR_MAME_MEDIA_PATH -ErrorAction SilentlyContinue
}
