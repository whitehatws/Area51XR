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

foreach ($required in @($RomPath, $MameRoot, $resolver, $preparePlay)) {
    if (-not (Test-Path $required)) {
        throw "Required play path not found: $required"
    }
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
$vdxrLogCopy = Join-Path $logDir "vdxr-openxr.log"

$runtimeLines = New-Object System.Collections.Generic.List[string]
$runtimeLines.Add("Area51XR play launch: $(Get-Date -Format o)")
$runtimeLines.Add("Host: $hostExe")
$runtimeLines.Add("MAME: $mameExe")
$runtimeLines.Add("Media: $mediaPath")

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

$vdProcesses = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -match '(?i)virtual.?desktop'
})
if ($vdProcesses.Count -gt 0) {
    $runtimeLines.Add("Virtual Desktop processes: $((($vdProcesses | Select-Object -ExpandProperty ProcessName -Unique) -join ', '))")
}
else {
    $runtimeLines.Add("Virtual Desktop processes: <none detected>")
}

function Resolve-RuntimeLibrary([string]$ManifestPath) {
    try {
        $json = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json
        $library = [Environment]::ExpandEnvironmentVariables([string]$json.runtime.library_path)
        if ([string]::IsNullOrWhiteSpace($library)) { return $null }
        if (-not [System.IO.Path]::IsPathRooted($library)) {
            $library = Join-Path (Split-Path -Parent $ManifestPath) $library
        }
        return [System.IO.Path]::GetFullPath($library)
    }
    catch {
        return $null
    }
}

$runtimeCandidates = New-Object System.Collections.Generic.List[object]
$seenRuntimeManifests = @{}
function Add-RuntimeCandidate([string]$Name, [string]$ManifestPath, [int]$Attempts) {
    if ([string]::IsNullOrWhiteSpace($ManifestPath) -or -not (Test-Path $ManifestPath)) { return }
    $full = [System.IO.Path]::GetFullPath($ManifestPath)
    $key = $full.ToLowerInvariant()
    if ($seenRuntimeManifests.ContainsKey($key)) { return }
    $seenRuntimeManifests[$key] = $true
    $library = Resolve-RuntimeLibrary $full
    if (-not $library -or -not (Test-Path $library)) {
        $runtimeLines.Add("Skipping $Name runtime manifest because its library is missing: $full -> $library")
        return
    }
    $runtimeCandidates.Add([PSCustomObject]@{
        Name = $Name
        Manifest = $full
        Library = $library
        Attempts = $Attempts
    })
}

$vdxrDir = "C:\Program Files\Virtual Desktop Streamer\OpenXR"
if (Test-Path $vdxrDir) {
    Add-RuntimeCandidate "VDXR" (Join-Path $vdxrDir "virtualdesktop-openxr.json") 3
    Get-ChildItem -Path $vdxrDir -Filter "virtualdesktop-openxr*.json" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notmatch '(?i)-32\.json$' } |
        ForEach-Object { Add-RuntimeCandidate "VDXR" $_.FullName 3 }
}

Add-RuntimeCandidate "SteamVR" "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json" 1
Add-RuntimeCandidate "SteamVR" "C:\Program Files\Steam\steamapps\common\SteamVR\steamxr_win64.json" 1

if ($runtimeCandidates.Count -eq 0) {
    throw "No usable 64-bit VDXR or SteamVR OpenXR runtime manifest was found. Install/update Virtual Desktop Streamer or SteamVR."
}

$runtimeLines.Add("Runtime candidates:")
foreach ($candidate in $runtimeCandidates) {
    $runtimeLines.Add("  $($candidate.Name): $($candidate.Manifest)")
    $runtimeLines.Add("    library=$($candidate.Library)")
}

$savedEnvironment = @{}
function Set-ScopedEnvironment([string]$Name, [AllowNull()][string]$Value) {
    if (-not $savedEnvironment.ContainsKey($Name)) {
        $savedEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process')
    }
    if ($null -eq $Value) {
        Remove-Item "Env:$Name" -ErrorAction SilentlyContinue
    }
    else {
        Set-Item "Env:$Name" $Value
    }
}

# Remove externally forced layer overrides, but preserve Virtual Desktop's own
# implicit compatibility layer. Disable only unrelated third-party implicit layers.
Set-ScopedEnvironment "XR_ENABLE_API_LAYERS" $null
Set-ScopedEnvironment "XR_API_LAYER_PATH" $null
Set-ScopedEnvironment "XR_LOADER_DEBUG" "warn"

$implicitRoots = @(
    "HKLM:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit",
    "HKCU:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
)
foreach ($implicitRoot in $implicitRoots) {
    if (-not (Test-Path $implicitRoot)) { continue }
    try {
        $key = Get-Item $implicitRoot
        foreach ($manifestName in $key.GetValueNames()) {
            $enabledValue = $key.GetValue($manifestName)
            if ($enabledValue -ne 0 -or -not (Test-Path $manifestName)) { continue }
            try {
                $layerJson = Get-Content -Raw -Path $manifestName | ConvertFrom-Json
                $layerName = [string]$layerJson.api_layer.name
                $disableVariable = [string]$layerJson.api_layer.disable_environment
                if ($layerName -match '(?i)virtual.?desktop' -or $manifestName -match '(?i)virtual.?desktop') {
                    $runtimeLines.Add("Preserved Virtual Desktop implicit OpenXR layer: $layerName")
                    continue
                }
                if (-not [string]::IsNullOrWhiteSpace($disableVariable)) {
                    Set-ScopedEnvironment $disableVariable "1"
                    $runtimeLines.Add("Disabled third-party implicit OpenXR layer for Area51XR: $layerName ($disableVariable)")
                }
                else {
                    $runtimeLines.Add("Third-party implicit OpenXR layer has no disable variable: $layerName ($manifestName)")
                }
            }
            catch {
                $runtimeLines.Add("Could not inspect implicit OpenXR layer: $manifestName")
            }
        }
    }
    catch {
        $runtimeLines.Add("Could not inspect implicit OpenXR layer registry: $implicitRoot")
    }
}

$runtimeLines | Set-Content -Path $runtimeLog -Encoding UTF8
Write-Host ($runtimeLines -join [Environment]::NewLine)

function Save-VdxrLog {
    $source = Join-Path $env:ProgramData "Virtual Desktop\OpenXR.log"
    if (Test-Path $source) {
        Copy-Item $source $vdxrLogCopy -Force -ErrorAction SilentlyContinue
    }
}

function Show-VdxrLogTail {
    Save-VdxrLog
    if (Test-Path $vdxrLogCopy) {
        Write-Host ""
        Write-Host "VDXR runtime log tail:"
        Get-Content -Path $vdxrLogCopy -Tail 80 -ErrorAction SilentlyContinue | Write-Host
    }
}

$hostProcess = $null
$mameProcess = $null
$selectedRuntime = $null
$lastOpenXrError = ""

try {
    Write-Host ""
    Write-Host "Starting clean OpenXR runtime negotiation..."

    foreach ($candidate in $runtimeCandidates) {
        for ($attempt = 1; $attempt -le $candidate.Attempts; ++$attempt) {
            Set-ScopedEnvironment "XR_RUNTIME_JSON" $candidate.Manifest
            Remove-Item $hostOut, $hostErr -Force -ErrorAction SilentlyContinue

            Write-Host "Trying $($candidate.Name) OpenXR ($attempt/$($candidate.Attempts))..."
            Write-Host "  Manifest: $($candidate.Manifest)"
            Write-Host "  Runtime:  $($candidate.Library)"

            $hostProcess = Start-Process -FilePath $hostExe -ArgumentList @("--xr-bridge") -PassThru `
                -WorkingDirectory (Split-Path -Parent $hostExe) `
                -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

            $deadline = (Get-Date).AddSeconds(8)
            $initialized = $false
            while ((Get-Date) -lt $deadline) {
                Start-Sleep -Milliseconds 250
                if ($hostProcess.HasExited) { break }
                $hostText = if (Test-Path $hostOut) { Get-Content -Raw $hostOut -ErrorAction SilentlyContinue } else { "" }
                if ($hostText -match "OpenXR bridge initialized") {
                    $initialized = $true
                    break
                }
            }

            if ($initialized -and -not $hostProcess.HasExited) {
                $selectedRuntime = $candidate
                Write-Host "$($candidate.Name) OpenXR initialized successfully."
                break
            }

            $lastOpenXrError = if (Test-Path $hostErr) { Get-Content -Raw $hostErr -ErrorAction SilentlyContinue } else { "OpenXR host did not initialize." }
            if ($hostProcess -and -not $hostProcess.HasExited) {
                Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
                $hostProcess.WaitForExit()
            }
            $hostProcess = $null

            if ($candidate.Name -eq "VDXR") {
                Save-VdxrLog
                Write-Host "VDXR did not initialize. Retrying/falling back without launching MAME."
                Start-Sleep -Seconds 1
            }
        }
        if ($selectedRuntime) { break }
    }

    if (-not $selectedRuntime) {
        Show-VdxrLogTail
        throw "No OpenXR runtime could initialize Area51XR. Last host error: $lastOpenXrError Runtime diagnostics: $runtimeLog VDXR diagnostics: $vdxrLogCopy"
    }

    Add-Content -Path $runtimeLog -Value "Selected runtime=$($selectedRuntime.Name)"
    Add-Content -Path $runtimeLog -Value "Selected manifest=$($selectedRuntime.Manifest)"
    Add-Content -Path $runtimeLog -Value "Selected library=$($selectedRuntime.Library)"

    Write-Host "Starting Area 51 in patched MAME only after OpenXR initialized..."
    $mameArgs = @(
        "area51",
        "-rompath", $mediaPath,
        "-window",
        "-skip_gameinfo",
        "-lowlatency",
        "-verbose"
    )
    $mameProcess = Start-Process -FilePath $mameExe -ArgumentList $mameArgs -PassThru `
        -WorkingDirectory (Split-Path -Parent $mameExe) `
        -RedirectStandardOutput $mameOut -RedirectStandardError $mameErr

    Write-Host ""
    Write-Host "AREA51XR PLAY MODE STARTED"
    Write-Host "OpenXR runtime: $($selectedRuntime.Name)"
    Write-Host "MAME low-latency mode: enabled"
    Write-Host "Right controller: aim + trigger. Point outside the game screen and pull trigger to reload."
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
            Save-VdxrLog
            throw "Area51XR OpenXR host exited after initialization. $errorText"
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
    Save-VdxrLog
    foreach ($name in $savedEnvironment.Keys) {
        $previous = $savedEnvironment[$name]
        if ($null -eq $previous) {
            Remove-Item "Env:$name" -ErrorAction SilentlyContinue
        }
        else {
            Set-Item "Env:$name" $previous
        }
    }
    Remove-Item Env:A51XR_MAME_MEDIA_PATH -ErrorAction SilentlyContinue
}