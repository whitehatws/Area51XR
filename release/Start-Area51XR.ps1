param(
    [string]$RomPath = "",
    [ValidateSet("Auto", "Active", "VDXR", "MetaLink", "SteamVR")]
    [string]$Runtime = "Auto"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$hostExe = Join-Path $root "bin\area51xr.exe"
$emulatorExe = Join-Path $root "emulator\area51xr.exe"
$binDir = Join-Path $root "bin"
$defaultMedia = Join-Path $root "media"
$logRoot = Join-Path $root "logs"

if ([string]::IsNullOrWhiteSpace($RomPath)) {
    $RomPath = $defaultMedia
}
$RomPath = [System.IO.Path]::GetFullPath($RomPath)

foreach ($required in @($hostExe, $emulatorExe, $binDir)) {
    if (-not (Test-Path $required)) {
        throw "Area51XR installation is incomplete. Missing: $required"
    }
}

if (-not (Test-Path $RomPath)) {
    New-Item -ItemType Directory -Force -Path $RomPath | Out-Null
}

function Test-Area51Media([string]$Path) {
    $roots = @($Path -split ';') | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    $romFound = $false
    $chdFound = $false
    foreach ($mediaRoot in $roots) {
        $archive = Join-Path $mediaRoot "area51.zip"
        $looseRom = Join-Path $mediaRoot "area51\2-c_area_51_hh.hh"
        $chd = Join-Path $mediaRoot "area51\area51.chd"
        if ((Test-Path $archive) -or (Test-Path $looseRom)) { $romFound = $true }
        if (Test-Path $chd) { $chdFound = $true }
    }
    return $romFound -and $chdFound
}

if (-not (Test-Area51Media $RomPath)) {
    Write-Host ""
    Write-Host "AREA51XR NEEDS YOUR LEGAL AREA 51 GAME MEDIA" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Place the Area 51 board ROMs and area51.chd under:"
    Write-Host "  $defaultMedia\area51"
    Write-Host ""
    Write-Host "Or launch Start-Area51XR.ps1 with -RomPath pointing to an existing compatible media folder."
    Write-Host "No ROMs, CHDs, or original game assets are included with Area51XR."
    if ($RomPath -eq $defaultMedia) {
        Start-Process explorer.exe $defaultMedia -ErrorAction SilentlyContinue
    }
    exit 2
}

if (-not (($env:PATH -split ';') -contains $binDir)) {
    $env:PATH = "$binDir;$env:PATH"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $logRoot "play-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$hostOut = Join-Path $logDir "area51xr.log"
$hostErr = Join-Path $logDir "area51xr.err.log"
$emulatorOut = Join-Path $logDir "emulator.log"
$emulatorErr = Join-Path $logDir "emulator.err.log"
$runtimeLog = Join-Path $logDir "openxr-runtime.txt"
$vdxrLogCopy = Join-Path $logDir "vdxr-openxr.log"

Write-Host "Verifying Area 51 media..."
$auditOutput = @(& $emulatorExe -rompath $RomPath -verifyroms area51 2>&1)
$auditExit = $LASTEXITCODE
$auditOutput | Write-Host
if ($auditExit -ne 0) {
    throw "Area 51 media verification failed. Check the files in '$RomPath'."
}

$runtimeLines = New-Object System.Collections.Generic.List[string]
$runtimeLines.Add("Area51XR release launch: $(Get-Date -Format o)")
$runtimeLines.Add("Host: $hostExe")
$runtimeLines.Add("Emulator: $emulatorExe")
$runtimeLines.Add("Media: $RomPath")
$runtimeLines.Add("Runtime preference: $Runtime")

$activeRuntimeManifests = New-Object System.Collections.Generic.List[string]
foreach ($registryPath in @(
    "HKLM:\SOFTWARE\Khronos\OpenXR\1",
    "HKCU:\SOFTWARE\Khronos\OpenXR\1"
)) {
    try {
        $activeRuntime = [string](Get-ItemProperty -Path $registryPath -Name ActiveRuntime -ErrorAction Stop).ActiveRuntime
        $runtimeLines.Add("$registryPath ActiveRuntime=$activeRuntime")
        if (-not [string]::IsNullOrWhiteSpace($activeRuntime)) {
            $activeRuntimeManifests.Add($activeRuntime)
        }
    }
    catch {
        $runtimeLines.Add("$registryPath ActiveRuntime=<not set>")
    }
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

function Get-RuntimeName([string]$ManifestPath) {
    if ($ManifestPath -match '(?i)virtual.?desktop') { return "VDXR" }
    if ($ManifestPath -match '(?i)oculus|meta') { return "MetaLink" }
    if ($ManifestPath -match '(?i)steamxr|steamvr') { return "SteamVR" }
    return "Active"
}

$runtimeCandidates = New-Object System.Collections.Generic.List[object]
$seenRuntimeManifests = @{}
function Add-RuntimeCandidate([string]$Name, [string]$ManifestPath, [int]$Attempts = 2) {
    if ([string]::IsNullOrWhiteSpace($ManifestPath) -or -not (Test-Path $ManifestPath)) { return }
    $full = [System.IO.Path]::GetFullPath($ManifestPath)
    $key = $full.ToLowerInvariant()
    if ($seenRuntimeManifests.ContainsKey($key)) { return }
    $library = Resolve-RuntimeLibrary $full
    if (-not $library -or -not (Test-Path $library)) { return }
    $seenRuntimeManifests[$key] = $true
    $runtimeCandidates.Add([PSCustomObject]@{
        Name = $Name
        Manifest = $full
        Library = $library
        Attempts = $Attempts
    })
}

function Add-ActiveRuntimes {
    foreach ($manifest in $activeRuntimeManifests) {
        $name = Get-RuntimeName $manifest
        $attempts = if ($name -eq "VDXR") { 3 } else { 2 }
        Add-RuntimeCandidate $name $manifest $attempts
    }
}

function Add-VdxrRuntimes {
    foreach ($dir in @(
        "C:\Program Files\Virtual Desktop Streamer\OpenXR",
        "C:\Program Files\VirtualDesktopXR"
    )) {
        if (-not (Test-Path $dir)) { continue }
        Get-ChildItem -Path $dir -Filter "virtualdesktop-openxr*.json" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -notmatch '(?i)-32\.json$' } |
            Sort-Object Name |
            ForEach-Object { Add-RuntimeCandidate "VDXR" $_.FullName 3 }
    }
}

function Add-MetaLinkRuntimes {
    Add-RuntimeCandidate "MetaLink" "C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json" 2
}

function Add-SteamVrRuntimes {
    Add-RuntimeCandidate "SteamVR" "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json" 2
    Add-RuntimeCandidate "SteamVR" "C:\Program Files\Steam\steamapps\common\SteamVR\steamxr_win64.json" 2
    foreach ($steamRegistry in @(
        "HKCU:\SOFTWARE\Valve\Steam",
        "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam"
    )) {
        try {
            $props = Get-ItemProperty -Path $steamRegistry -ErrorAction Stop
            foreach ($propertyName in @("SteamPath", "InstallPath")) {
                $steamRoot = [string]$props.$propertyName
                if (-not [string]::IsNullOrWhiteSpace($steamRoot)) {
                    Add-RuntimeCandidate "SteamVR" (Join-Path $steamRoot "steamapps\common\SteamVR\steamxr_win64.json") 2
                }
            }
        }
        catch {}
    }
}

switch ($Runtime) {
    "Active" { Add-ActiveRuntimes }
    "VDXR" { Add-VdxrRuntimes }
    "MetaLink" { Add-MetaLinkRuntimes }
    "SteamVR" { Add-SteamVrRuntimes }
    default {
        Add-ActiveRuntimes
        Add-MetaLinkRuntimes
        Add-SteamVrRuntimes
        Add-VdxrRuntimes
    }
}

if ($runtimeCandidates.Count -eq 0) {
    Write-Host ""
    Write-Host "NO OPENXR RUNTIME FOUND" -ForegroundColor Red
    Write-Host "Use one of these PCVR paths, connect the headset, then run Area51XR again:"
    Write-Host "  - Meta Horizon Link / Air Link (free)"
    Write-Host "  - Steam Link + SteamVR (free)"
    Write-Host "  - Virtual Desktop / VDXR (optional paid alternative)"
    throw "No usable 64-bit OpenXR runtime was found for '$Runtime'."
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

Set-ScopedEnvironment "XR_ENABLE_API_LAYERS" $null
Set-ScopedEnvironment "XR_API_LAYER_PATH" $null
Set-ScopedEnvironment "XR_LOADER_DEBUG" "warn"

$implicitLayers = New-Object System.Collections.Generic.List[object]
foreach ($implicitRoot in @(
    "HKLM:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit",
    "HKCU:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
)) {
    if (-not (Test-Path $implicitRoot)) { continue }
    try {
        $key = Get-Item $implicitRoot
        foreach ($manifestName in $key.GetValueNames()) {
            if ($key.GetValue($manifestName) -ne 0 -or -not (Test-Path $manifestName)) { continue }
            try {
                $layerJson = Get-Content -Raw -Path $manifestName | ConvertFrom-Json
                $implicitLayers.Add([PSCustomObject]@{
                    Name = [string]$layerJson.api_layer.name
                    Manifest = [string]$manifestName
                    DisableVariable = [string]$layerJson.api_layer.disable_environment
                })
            }
            catch {}
        }
    }
    catch {}
}

function Configure-ImplicitLayersForRuntime([string]$RuntimeName) {
    foreach ($layer in $implicitLayers) {
        if ([string]::IsNullOrWhiteSpace($layer.DisableVariable)) { continue }
        $owned =
            ($RuntimeName -eq "VDXR" -and ($layer.Name -match '(?i)virtual.?desktop' -or $layer.Manifest -match '(?i)virtual.?desktop')) -or
            ($RuntimeName -eq "MetaLink" -and ($layer.Name -match '(?i)oculus|meta' -or $layer.Manifest -match '(?i)oculus|meta')) -or
            ($RuntimeName -eq "SteamVR" -and ($layer.Name -match '(?i)steam' -or $layer.Manifest -match '(?i)steam'))
        if ($owned) { Set-ScopedEnvironment $layer.DisableVariable $null }
        else { Set-ScopedEnvironment $layer.DisableVariable "1" }
    }
}

$runtimeLines | Set-Content -Path $runtimeLog -Encoding UTF8

function Save-VdxrLog {
    $source = Join-Path $env:ProgramData "Virtual Desktop\OpenXR.log"
    if (Test-Path $source) {
        Copy-Item $source $vdxrLogCopy -Force -ErrorAction SilentlyContinue
    }
}

$hostProcess = $null
$emulatorProcess = $null
$selectedRuntime = $null
$lastOpenXrError = ""

try {
    Write-Host "Connecting Area51XR to OpenXR..."
    foreach ($candidate in $runtimeCandidates) {
        Configure-ImplicitLayersForRuntime $candidate.Name
        for ($attempt = 1; $attempt -le $candidate.Attempts; ++$attempt) {
            Set-ScopedEnvironment "XR_RUNTIME_JSON" $candidate.Manifest
            Remove-Item $hostOut, $hostErr -Force -ErrorAction SilentlyContinue
            Write-Host "Trying $($candidate.Name) ($attempt/$($candidate.Attempts))..."

            $hostProcess = Start-Process -FilePath $hostExe -ArgumentList @("--xr-bridge") -PassThru `
                -WorkingDirectory $binDir `
                -RedirectStandardOutput $hostOut -RedirectStandardError $hostErr

            $deadline = (Get-Date).AddSeconds(10)
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
                Write-Host "$($candidate.Name) connected."
                break
            }

            $lastOpenXrError = if (Test-Path $hostErr) { Get-Content -Raw $hostErr -ErrorAction SilentlyContinue } else { "OpenXR host did not initialize." }
            if ($hostProcess -and -not $hostProcess.HasExited) {
                Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
                $hostProcess.WaitForExit()
            }
            $hostProcess = $null
            if ($candidate.Name -eq "VDXR") { Save-VdxrLog }
            Start-Sleep -Seconds 1
        }
        if ($selectedRuntime) { break }
    }

    if (-not $selectedRuntime) {
        throw "Area51XR could not initialize an OpenXR headset. Connect Meta Horizon Link, Steam Link/SteamVR, or Virtual Desktop and try again. Last error: $lastOpenXrError Logs: $logDir"
    }

    Add-Content -Path $runtimeLog -Value "Selected runtime=$($selectedRuntime.Name)"
    Add-Content -Path $runtimeLog -Value "Selected manifest=$($selectedRuntime.Manifest)"
    Add-Content -Path $runtimeLog -Value "Selected library=$($selectedRuntime.Library)"

    $emulatorArgs = @(
        "area51",
        "-rompath", $RomPath,
        "-window",
        "-skip_gameinfo",
        "-lowlatency"
    )
    $emulatorProcess = Start-Process -FilePath $emulatorExe -ArgumentList $emulatorArgs -PassThru `
        -WorkingDirectory (Split-Path -Parent $emulatorExe) `
        -RedirectStandardOutput $emulatorOut -RedirectStandardError $emulatorErr

    Write-Host ""
    Write-Host "AREA51XR STARTED" -ForegroundColor Green
    Write-Host "Runtime: $($selectedRuntime.Name)"
    Write-Host "Trigger: fire"
    Write-Host "Aim outside screen + trigger: reload"
    Write-Host "B: insert coin"
    Write-Host "A: start / continue"
    Write-Host ""
    Write-Host "Close the game window to exit."

    while ($true) {
        Start-Sleep -Milliseconds 500
        if ($hostProcess.HasExited) {
            $hostProcess.Refresh()
            $exitCode = $hostProcess.ExitCode
            $errorText = if (Test-Path $hostErr) { Get-Content -Raw $hostErr } else { "" }
            throw "Area51XR VR host stopped unexpectedly. ExitCode=$exitCode $errorText Logs: $logDir"
        }
        if ($emulatorProcess.HasExited) {
            if ($emulatorProcess.ExitCode -ne 0) {
                $errorText = if (Test-Path $emulatorErr) { Get-Content -Raw $emulatorErr } else { "" }
                throw "Area 51 emulator stopped with code $($emulatorProcess.ExitCode). $errorText Logs: $logDir"
            }
            break
        }
    }
}
finally {
    if ($emulatorProcess -and -not $emulatorProcess.HasExited) {
        Stop-Process -Id $emulatorProcess.Id -Force -ErrorAction SilentlyContinue
    }
    if ($hostProcess -and -not $hostProcess.HasExited) {
        Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
    }
    if ($selectedRuntime -and $selectedRuntime.Name -eq "VDXR") { Save-VdxrLog }
    foreach ($name in $savedEnvironment.Keys) {
        $previous = $savedEnvironment[$name]
        if ($null -eq $previous) { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
        else { Set-Item "Env:$name" $previous }
    }
}
