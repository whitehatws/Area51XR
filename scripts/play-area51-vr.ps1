param(
    [string]$RomPath = "D:\MAME\roms",
    [string]$MsysRoot = "C:\msys64",
    [string]$MameRoot = "",
    [string]$OpenXrManifest = "",
    [int]$OpenXrReadyTimeoutSeconds = 45,
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
if ($OpenXrReadyTimeoutSeconds -lt 5) {
    throw "OpenXrReadyTimeoutSeconds must be at least 5."
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

function Get-ActiveRuntime([string]$RegistryPath) {
    try {
        return [string](Get-ItemProperty -Path $RegistryPath -Name ActiveRuntime -ErrorAction Stop).ActiveRuntime
    }
    catch {
        return ""
    }
}

function Find-VirtualDesktopOpenXrManifest {
    $candidates = New-Object System.Collections.Generic.List[string]

    foreach ($active in @(
        (Get-ActiveRuntime "HKLM:\SOFTWARE\Khronos\OpenXR\1"),
        (Get-ActiveRuntime "HKCU:\SOFTWARE\Khronos\OpenXR\1")
    )) {
        if (-not [string]::IsNullOrWhiteSpace($active) -and $active -match '(?i)virtual.?desktop' -and -not $candidates.Contains($active)) {
            $candidates.Add($active)
        }
    }

    foreach ($programRoot in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if ([string]::IsNullOrWhiteSpace($programRoot)) { continue }
        $streamerRoot = Join-Path $programRoot "Virtual Desktop Streamer"
        foreach ($candidate in @(
            (Join-Path $streamerRoot "OpenXR\virtualdesktop-openxr.json"),
            (Join-Path $streamerRoot "virtualdesktop-openxr.json")
        )) {
            if (-not $candidates.Contains($candidate)) {
                $candidates.Add($candidate)
            }
        }
        if (Test-Path $streamerRoot) {
            foreach ($found in @(Get-ChildItem -Path $streamerRoot -Filter "*openxr*.json" -File -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -notmatch '(?i)-32\.json$' })) {
                if (-not $candidates.Contains($found.FullName)) {
                    $candidates.Add($found.FullName)
                }
            }
        }
    }

    return $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

function Resolve-RuntimeLibrary([string]$ManifestPath) {
    try {
        $manifest = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json
        $libraryPath = [string]$manifest.runtime.library_path
        if ([string]::IsNullOrWhiteSpace($libraryPath)) {
            return ""
        }
        if ([System.IO.Path]::IsPathRooted($libraryPath)) {
            return [System.IO.Path]::GetFullPath($libraryPath)
        }
        return [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $ManifestPath) $libraryPath))
    }
    catch {
        return ""
    }
}

$priorRuntimeJsonExists = Test-Path Env:XR_RUNTIME_JSON
$priorRuntimeJson = $env:XR_RUNTIME_JSON
$selectedRuntimeManifest = $OpenXrManifest
if ([string]::IsNullOrWhiteSpace($selectedRuntimeManifest)) {
    $selectedRuntimeManifest = Find-VirtualDesktopOpenXrManifest
}
if (-not [string]::IsNullOrWhiteSpace($selectedRuntimeManifest)) {
    $selectedRuntimeManifest = [System.IO.Path]::GetFullPath($selectedRuntimeManifest)
    if (-not (Test-Path $selectedRuntimeManifest)) {
        throw "Selected OpenXR runtime manifest does not exist: $selectedRuntimeManifest"
    }
    $env:XR_RUNTIME_JSON = $selectedRuntimeManifest
}

$runtimeLines = New-Object System.Collections.Generic.List[string]
$runtimeLines.Add("Area51XR play launch: $(Get-Date -Format o)")
foreach ($registryPath in @(
    "HKLM:\SOFTWARE\Khronos\OpenXR\1",
    "HKCU:\SOFTWARE\Khronos\OpenXR\1"
)) {
    $runtime = Get-ActiveRuntime $registryPath
    if ([string]::IsNullOrWhiteSpace($runtime)) { $runtime = "<not set>" }
    $runtimeLines.Add("$registryPath ActiveRuntime=$runtime")
}
if (-not [string]::IsNullOrWhiteSpace($selectedRuntimeManifest)) {
    $runtimeLines.Add("Process XR_RUNTIME_JSON=$selectedRuntimeManifest")
    $runtimeLibrary = Resolve-RuntimeLibrary $selectedRuntimeManifest
    if ([string]::IsNullOrWhiteSpace($runtimeLibrary)) {
        $runtimeLines.Add("Selected runtime library=<could not resolve from manifest>")
    }
    else {
        $runtimeLines.Add("Selected runtime library=$runtimeLibrary")
        $runtimeLines.Add("Selected runtime library exists=$(Test-Path $runtimeLibrary)")
        if (-not (Test-Path $runtimeLibrary)) {
            throw "Virtual Desktop OpenXR manifest was found, but its runtime DLL is missing: $runtimeLibrary"
        }
    }
}
else {
    $runtimeLines.Add("Process XR_RUNTIME_JSON=<not set; using system ActiveRuntime>")
}
$runtimeLines | Set-Content -Path $runtimeLog -Encoding UTF8
Write-Host ($runtimeLines -join [Environment]::NewLine)

function Start-Area51XrHost {
    param([datetime]$Deadline)

    $attempt = 0
    $lastErrorText = ""
    do {
        $attempt++
        Remove-Item $hostOut, $hostErr -Force -ErrorAction SilentlyContinue
        Write-Host "Starting Area51XR OpenXR host (attempt $attempt)..."
        $process = Start-Process -FilePath $hostExe -ArgumentList @("--xr-bridge") -PassThru `
            -WorkingDirectory (Split-Path -Parent $hostExe) `
            -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

        $probeDeadline = (Get-Date).AddSeconds(2)
        while ((Get-Date) -lt $probeDeadline -and -not $process.HasExited) {
            Start-Sleep -Milliseconds 200
            $hostText = if (Test-Path $hostOut) { Get-Content -Raw $hostOut -ErrorAction SilentlyContinue } else { "" }
            if ($hostText -match "OpenXR bridge initialized") {
                return $process
            }
        }

        if (-not $process.HasExited) {
            return $process
        }

        $lastErrorText = if (Test-Path $hostErr) { Get-Content -Raw $hostErr -ErrorAction SilentlyContinue } else { "" }
        if ($lastErrorText -match '(?i)xrCreateInstance|no OpenXR HMD|xrGetSystem|HMD system') {
            Write-Host "VDXR is not ready yet. Connect the Quest in Virtual Desktop; retrying..."
        }
        else {
            Write-Host "OpenXR host exited during startup; retrying until the readiness timeout..."
        }
        Start-Sleep -Seconds 2
    } while ((Get-Date) -lt $Deadline)

    throw "Area51XR could not initialize OpenXR within $OpenXrReadyTimeoutSeconds seconds. Last error: $lastErrorText Runtime log: $runtimeLog"
}

$hostProcess = $null
$mameProcess = $null
try {
    Write-Host ""
    if (-not [string]::IsNullOrWhiteSpace($selectedRuntimeManifest)) {
        Write-Host "Using Virtual Desktop/VDXR runtime: $selectedRuntimeManifest"
    }
    else {
        Write-Host "Virtual Desktop OpenXR manifest was not found; using the system OpenXR runtime."
    }
    Write-Host "Connect the Quest in Virtual Desktop now. The launcher will wait up to $OpenXrReadyTimeoutSeconds seconds for OpenXR."

    $hostProcess = Start-Area51XrHost -Deadline (Get-Date).AddSeconds($OpenXrReadyTimeoutSeconds)
    Write-Host "OpenXR host initialized."

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
            $errorText = if (Test-Path $hostErr) { Get-Content -Raw $hostErr -ErrorAction SilentlyContinue } else { "" }
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
    if ($priorRuntimeJsonExists) {
        $env:XR_RUNTIME_JSON = $priorRuntimeJson
    }
    else {
        Remove-Item Env:XR_RUNTIME_JSON -ErrorAction SilentlyContinue
    }
}
