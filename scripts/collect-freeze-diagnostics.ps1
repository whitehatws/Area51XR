[CmdletBinding()]
param()

$ErrorActionPreference = "Continue"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$desktop = [Environment]::GetFolderPath("Desktop")
$work = Join-Path $desktop "Area51XR-freeze-diagnostics-$stamp"
$zip = "$work.zip"
New-Item -ItemType Directory -Path $work -Force | Out-Null

function Write-Section {
    param([string]$Name, [scriptblock]$Command)
    $path = Join-Path $work ($Name + ".txt")
    try {
        & $Command 2>&1 | Out-String -Width 320 | Set-Content -Path $path -Encoding UTF8
    }
    catch {
        ("Collection failed: " + $_.Exception.Message) | Set-Content -Path $path -Encoding UTF8
    }
}

function Get-VersionInfo {
    param([string]$Root)
    if (-not (Test-Path $Root)) { return }
    Get-ChildItem $Root -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @(".exe", ".dll") } |
        Select-Object -First 200 |
        ForEach-Object {
            $version = $_.VersionInfo
            [PSCustomObject]@{
                Path = $_.FullName
                Product = $version.ProductName
                ProductVersion = $version.ProductVersion
                FileVersion = $version.FileVersion
                Modified = $_.LastWriteTime
            }
        }
}

Write-Section "00-summary" {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    $isElevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    "Area51XR freeze diagnostic collection"
    "Collected: $(Get-Date -Format o)"
    "PowerShell: $($PSVersionTable.PSVersion)"
    "Elevated: $isElevated"
    "This archive intentionally excludes game media, ROMs, CHDs, NVRAM, settings files, browser data, and credentials."
}

Write-Section "01-windows" {
    Get-ComputerInfo | Select-Object WindowsProductName, WindowsVersion, OsBuildNumber, OsArchitecture, BiosFirmwareType, CsSystemType
    Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber, LastBootUpTime
}

Write-Section "02-gpu" {
    Get-CimInstance Win32_VideoController |
        Select-Object Name, AdapterCompatibility, DriverVersion, DriverDate, VideoProcessor, AdapterRAM, Status, PNPDeviceID
    if (Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue) {
        ""
        "nvidia-smi"
        & nvidia-smi.exe
    }
}

Write-Section "03-display-and-graphics-settings" {
    "DWM graphics settings"
    Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers" -ErrorAction SilentlyContinue |
        Select-Object HwSchMode, TdrDelay, TdrDdiDelay, TdrLevel
    ""
    "Per-user video settings"
    Get-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\VideoSettings" -ErrorAction SilentlyContinue
    ""
    "Display devices"
    Get-PnpDevice -Class Display -ErrorAction SilentlyContinue | Select-Object Status, Class, FriendlyName, InstanceId
}

Write-Section "04-openxr-registry" {
    foreach ($key in @(
        "HKLM:\SOFTWARE\Khronos\OpenXR\1",
        "HKCU:\SOFTWARE\Khronos\OpenXR\1",
        "HKLM:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit",
        "HKCU:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
    )) {
        "[$key]"
        if (Test-Path $key) { Get-ItemProperty $key } else { "<not present>" }
        ""
    }
    "Process OpenXR variables"
    "XR_RUNTIME_JSON=$env:XR_RUNTIME_JSON"
    "XR_API_LAYER_PATH=$env:XR_API_LAYER_PATH"
    "XR_ENABLE_API_LAYERS=$env:XR_ENABLE_API_LAYERS"
    "XR_LOADER_DEBUG=$env:XR_LOADER_DEBUG"
}

Write-Section "05-openxr-manifests" {
    $manifests = New-Object System.Collections.Generic.List[string]
    foreach ($key in @("HKLM:\SOFTWARE\Khronos\OpenXR\1", "HKCU:\SOFTWARE\Khronos\OpenXR\1")) {
        try {
            $candidate = [string](Get-ItemProperty $key -Name ActiveRuntime -ErrorAction Stop).ActiveRuntime
            if ($candidate) { $manifests.Add($candidate) }
        } catch {}
    }
    Get-ChildItem "C:\Program Files\Virtual Desktop Streamer\OpenXR" -Filter "*.json" -File -ErrorAction SilentlyContinue |
        ForEach-Object { $manifests.Add($_.FullName) }
    foreach ($candidate in @(
        "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json",
        "C:\Program Files\Steam\steamapps\common\SteamVR\steamxr_win64.json",
        "C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json"
    )) {
        if (Test-Path $candidate) { $manifests.Add($candidate) }
    }
    foreach ($manifest in ($manifests | Select-Object -Unique)) {
        "[$manifest]"
        if (Test-Path $manifest) { Get-Content $manifest -Raw } else { "<missing>" }
        ""
    }
}

Write-Section "06-virtual-desktop-versions" {
    Get-VersionInfo "C:\Program Files\Virtual Desktop Streamer"
    ""
    "Installed-app records"
    foreach ($root in @(
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*"
    )) {
        Get-ItemProperty $root -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -match "Virtual Desktop|SteamVR|OpenXR|Oculus|Meta Quest" } |
            Select-Object DisplayName, DisplayVersion, Publisher, InstallDate, InstallLocation
    }
}

Write-Section "07-steamvr-versions" {
    Get-VersionInfo "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64"
    Get-VersionInfo "C:\Program Files\Steam\steamapps\common\SteamVR\bin\win64"
}

Write-Section "08-running-vr-processes" {
    Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match "VirtualDesktop|vrserver|vrcompositor|vrdashboard|steamvr|oculus|OVR|area51|mame" } |
        Select-Object ProcessName, Id, StartTime, Path, ProductVersion, Responding
}

Write-Section "09-recent-application-events" {
    $since = (Get-Date).AddHours(-8)
    Get-WinEvent -FilterHashtable @{LogName="Application"; StartTime=$since} -MaxEvents 1000 -ErrorAction SilentlyContinue |
        Where-Object { $_.LevelDisplayName -in @("Critical", "Error", "Warning") -or $_.Message -match "Virtual Desktop|OpenXR|SteamVR|area51|D3D|Direct3D|Display" } |
        Select-Object -First 250 TimeCreated, Id, LevelDisplayName, ProviderName, Message
}

Write-Section "10-recent-system-events" {
    $since = (Get-Date).AddHours(-8)
    Get-WinEvent -FilterHashtable @{LogName="System"; StartTime=$since} -MaxEvents 1500 -ErrorAction SilentlyContinue |
        Where-Object { $_.ProviderName -match "Display|nvlddmkm|amdkmdag|WHEA|Kernel-PnP" -or $_.Message -match "display driver|graphics|GPU|timeout|reset" } |
        Select-Object -First 250 TimeCreated, Id, LevelDisplayName, ProviderName, Message
}

Write-Section "11-area51xr-binaries" {
    $roots = @("C:\dev\Area51XR", "C:\dev\Area51XR-v1.0-control")
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        "[$root]"
        Get-ChildItem $root -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -in @("area51xr.exe", "Start-Area51XR.ps1", "Play Area51XR.cmd") } |
            ForEach-Object {
                [PSCustomObject]@{
                    Path = $_.FullName
                    Length = $_.Length
                    Modified = $_.LastWriteTime
                    SHA256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
                }
            }
    }
}

Write-Section "12-area51xr-git" {
    if ((Test-Path "C:\dev\Area51XR\.git") -and (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        & git.exe -C "C:\dev\Area51XR" status --short --branch
        & git.exe -C "C:\dev\Area51XR" log -5 --oneline --decorate
    } else {
        "C:\dev\Area51XR is not a Git working tree or git.exe was not found."
    }
}

$vdLog = "C:\ProgramData\Virtual Desktop\OpenXR.log"
if (Test-Path $vdLog) {
    Get-Content $vdLog -Tail 2000 -ErrorAction SilentlyContinue |
        Set-Content (Join-Path $work "13-vdxr-openxr-tail.log") -Encoding UTF8
}

$candidateLogRoots = New-Object System.Collections.Generic.List[string]
foreach ($root in @(
    "$env:LOCALAPPDATA\Area51XR\logs",
    "C:\dev\Area51XR\logs"
)) {
    if (Test-Path $root) { $candidateLogRoots.Add($root) }
}
$v1Launcher = Get-ChildItem "C:\dev\Area51XR-v1.0-control" -Filter "Start-Area51XR.ps1" -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($v1Launcher) {
    $v1Logs = Join-Path $v1Launcher.DirectoryName "logs"
    if (Test-Path $v1Logs) { $candidateLogRoots.Add($v1Logs) }
}

$logIndex = 0
foreach ($root in ($candidateLogRoots | Select-Object -Unique)) {
    $latestDirs = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 3
    foreach ($dir in $latestDirs) {
        $destination = Join-Path $work ("14-area51xr-logs-{0:D2}" -f $logIndex)
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        Get-ChildItem $dir.FullName -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in @(".log", ".txt") -and $_.Length -lt 20MB } |
            Copy-Item -Destination $destination -Force -ErrorAction SilentlyContinue
        "Source=$($dir.FullName)" | Set-Content (Join-Path $destination "source.txt") -Encoding UTF8
        $logIndex++
    }
}

$dxdiag = Join-Path $work "15-dxdiag.txt"
try {
    $process = Start-Process dxdiag.exe -ArgumentList @("/dontskip", "/t", $dxdiag) -PassThru -WindowStyle Hidden
    $null = $process.WaitForExit(60000)
} catch {
    "dxdiag collection failed: $($_.Exception.Message)" | Set-Content $dxdiag -Encoding UTF8
}

$replacements = @{}
if ($env:USERPROFILE) { $replacements[$env:USERPROFILE] = "<USERPROFILE>" }
if ($env:USERNAME) { $replacements[$env:USERNAME] = "<USERNAME>" }
if ($env:COMPUTERNAME) { $replacements[$env:COMPUTERNAME] = "<COMPUTER>" }

Get-ChildItem $work -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in @(".txt", ".log", ".json") } |
    ForEach-Object {
        try {
            $content = Get-Content $_.FullName -Raw -ErrorAction Stop
            foreach ($key in $replacements.Keys) {
                $content = $content.Replace($key, $replacements[$key])
            }
            $content | Set-Content $_.FullName -Encoding UTF8
        } catch {}
    }

Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $zip) { Remove-Item $zip -Force }
[System.IO.Compression.ZipFile]::CreateFromDirectory($work, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

Write-Host ""
Write-Host "Area51XR diagnostic collection complete." -ForegroundColor Green
Write-Host "Upload this ZIP:" -ForegroundColor Yellow
Write-Host $zip
