param(
    [string]$MsysRoot = "C:\msys64",

    [string]$MameRoot = "",

    [string]$OpenXrSdk = "",

    [string]$OnnxRuntimeDir = "",

    [string]$DepthModelPath = "",

    [string]$RomPath = "",

    [switch]$AllowModifiedMedia
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($MameRoot)) {
    $MameRoot = Join-Path $root "external\mame"
}
if ([string]::IsNullOrWhiteSpace($OpenXrSdk)) {
    $OpenXrSdk = Join-Path $MsysRoot "ucrt64"
}
if ([string]::IsNullOrWhiteSpace($OnnxRuntimeDir)) {
    $OnnxRuntimeDir = Join-Path $root "external\onnxruntime-1.28.0"
}
if ([string]::IsNullOrWhiteSpace($DepthModelPath)) {
    $DepthModelPath = Join-Path $root "models\depth_anything_v2_vits.onnx"
}

function Resolve-A51Path([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "Path value is empty."
    }
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-A51ParentDirectory([string]$Target) {
    $fullTarget = Resolve-A51Path $Target
    $parent = [System.IO.Path]::GetDirectoryName($fullTarget)
    if ([string]::IsNullOrWhiteSpace($parent)) {
        throw "Unable to determine parent directory for '$fullTarget'."
    }
    $rootPath = [System.IO.Path]::GetPathRoot($fullTarget)
    if ($parent.TrimEnd('\') -eq $rootPath.TrimEnd('\')) {
        if (-not (Test-Path $parent)) {
            throw "Drive root does not exist for '$fullTarget'."
        }
        return $fullTarget
    }
    if (-not (Test-Path $parent)) {
        Write-Host "Creating directory: $parent"
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    return $fullTarget
}

function Find-WindowsGit {
    $command = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($candidate in @(
        (Join-Path $env:ProgramFiles "Git\cmd\git.exe"),
        (Join-Path $env:ProgramFiles "Git\bin\git.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Git\cmd\git.exe")
    )) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return $candidate
        }
    }
    return ""
}

$MameRoot = Resolve-A51Path $MameRoot
$OpenXrSdk = Resolve-A51Path $OpenXrSdk
$OnnxRuntimeDir = Resolve-A51Path $OnnxRuntimeDir
$DepthModelPath = Resolve-A51Path $DepthModelPath
if (-not [string]::IsNullOrWhiteSpace($RomPath)) {
    $RomPath = Resolve-A51Path $RomPath
}

Write-Host "Bootstrap root: $root"
Write-Host "MAME root: $MameRoot"
Write-Host "OpenXR SDK root: $OpenXrSdk"
Write-Host "ONNX Runtime root: $OnnxRuntimeDir"
Write-Host "Depth model: $DepthModelPath"
if ($AllowModifiedMedia) {
    Write-Host "Modified Area 51 media mode: enabled"
}

function Get-MsysBashCandidatePaths([string]$PreferredRoot) {
    $paths = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($PreferredRoot)) {
        $paths.Add((Join-Path $PreferredRoot "usr\bin\bash.exe"))
    }
    foreach ($candidateRoot in @(
        "C:\msys64",
        "C:\msys2",
        (Join-Path $env:ProgramFiles "MSYS2"),
        (Join-Path ${env:ProgramFiles(x86)} "MSYS2"),
        (Join-Path $env:LOCALAPPDATA "Programs\MSYS2")
    )) {
        if (-not [string]::IsNullOrWhiteSpace($candidateRoot)) {
            $candidate = Join-Path $candidateRoot "usr\bin\bash.exe"
            if (-not $paths.Contains($candidate)) {
                $paths.Add($candidate)
            }
        }
    }
    return $paths.ToArray()
}

function Find-MsysBash([string]$PreferredRoot) {
    foreach ($candidate in (Get-MsysBashCandidatePaths $PreferredRoot)) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    return ""
}

function Wait-ForMsysBash([string]$PreferredRoot, [int]$TimeoutSeconds) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $found = Find-MsysBash $PreferredRoot
        if (-not [string]::IsNullOrWhiteSpace($found)) {
            return $found
        }
        Start-Sleep -Seconds 2
    } while ((Get-Date) -lt $deadline)
    return ""
}

function Get-MsysRootFromBash([string]$BashPath) {
    $bin = Split-Path -Parent $BashPath
    $usr = Split-Path -Parent $bin
    return Split-Path -Parent $usr
}

$bash = Find-MsysBash $MsysRoot

if ([string]::IsNullOrWhiteSpace($bash)) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "MSYS2 is not installed and winget is unavailable. Install MSYS2, then run this script again."
    }

    Write-Host "Installing MSYS2..."
    winget install --id MSYS2.MSYS2 -e --accept-source-agreements --accept-package-agreements
    $wingetExit = $LASTEXITCODE
    $bash = Wait-ForMsysBash $MsysRoot 180
    if ($wingetExit -ne 0 -and [string]::IsNullOrWhiteSpace($bash)) {
        throw "MSYS2 installer failed with exit code $wingetExit."
    }
    if ([string]::IsNullOrWhiteSpace($bash)) {
        $checked = (Get-MsysBashCandidatePaths $MsysRoot) -join ", "
        throw "MSYS2 installation finished, but bash.exe was not found. Checked: $checked"
    }
}

$MsysRoot = Get-MsysRootFromBash $bash
Write-Host "Using MSYS2 at $MsysRoot"

Write-Host "Installing the local build packages..."
$packageCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
pacman -S --needed --noconfirm git make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-lld mingw-w64-ucrt-x86_64-openxr-sdk
'@
& $bash -lc $packageCommand
if ($LASTEXITCODE -ne 0) {
    throw "MSYS2 package installation failed with exit code $LASTEXITCODE."
}

$openXrHeader = Join-Path $OpenXrSdk "include\openxr\openxr.h"
$openXrLoader = Join-Path $MsysRoot "ucrt64\bin\libopenxr_loader.dll"
if (-not (Test-Path $openXrHeader)) {
    throw "MSYS2 OpenXR headers were not found at '$openXrHeader'."
}
if (-not (Test-Path $openXrLoader)) {
    throw "MSYS2 OpenXR loader was not found at '$openXrLoader'."
}
Write-Host "Using packaged OpenXR SDK/loader from $OpenXrSdk"

$windowsGit = Find-WindowsGit
if ([string]::IsNullOrWhiteSpace($windowsGit)) {
    throw "Windows Git was not found. Install Git for Windows and rerun the acceptance script."
}
Write-Host "Using Git at $windowsGit"

function Clone-IfMissing([string]$Target, [string]$Url, [string]$Label) {
    $Target = Ensure-A51ParentDirectory $Target
    if (Test-Path (Join-Path $Target ".git")) {
        Write-Host "Using existing $Label source at $Target"
        return
    }

    if (Test-Path $Target) {
        $existing = @(Get-ChildItem -Force -Path $Target -ErrorAction SilentlyContinue)
        if ($existing.Count -eq 0) {
            Remove-Item -Force -Path $Target
        }
        else {
            throw "$Label target exists but is not a Git checkout: $Target"
        }
    }

    if (-not (Test-Path (Split-Path -Parent $Target))) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Target) | Out-Null
    }

    Write-Host "Cloning $Label source..."
    Write-Host "Clone target: $Target"
    & $windowsGit clone --depth 1 -- $Url $Target
    if ($LASTEXITCODE -ne 0) {
        throw "$Label source clone failed with exit code $LASTEXITCODE."
    }
    if (-not (Test-Path (Join-Path $Target ".git"))) {
        throw "$Label clone completed without a .git directory at $Target"
    }
}

function Download-File([string]$Url, [string]$Target, [string]$Label) {
    $Target = Ensure-A51ParentDirectory $Target
    if (Test-Path $Target) {
        Write-Host "Using existing $Label at $Target"
        return
    }
    Write-Host "Downloading $Label..."
    Write-Host "Download target: $Target"
    Invoke-WebRequest -Uri $Url -OutFile $Target -UseBasicParsing
}

Clone-IfMissing -Target $MameRoot -Url "https://github.com/mamedev/mame.git" -Label "MAME"

$mameCommit = (& $windowsGit -C $MameRoot rev-parse HEAD 2>$null).Trim()
if ($mameCommit) {
    Write-Host "MAME commit: $mameCommit"
}

$ortHeader = Join-Path $OnnxRuntimeDir "build\native\include\onnxruntime_cxx_api.h"
$ortDll = Join-Path $OnnxRuntimeDir "runtimes\win-x64\native\onnxruntime.dll"
if (-not (Test-Path $ortHeader) -or -not (Test-Path $ortDll)) {
    $ortPackage = Join-Path $root "external\Microsoft.ML.OnnxRuntime.1.28.0.nupkg"
    $ortZip = Join-Path $root "external\Microsoft.ML.OnnxRuntime.1.28.0.zip"
    Download-File -Url "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime/1.28.0" `
        -Target $ortPackage -Label "ONNX Runtime 1.28.0"

    Write-Host "Extracting ONNX Runtime..."
    Copy-Item $ortPackage $ortZip -Force
    if (Test-Path $OnnxRuntimeDir) {
        Remove-Item -Recurse -Force $OnnxRuntimeDir
    }
    New-Item -ItemType Directory -Force -Path $OnnxRuntimeDir | Out-Null
    Expand-Archive -Path $ortZip -DestinationPath $OnnxRuntimeDir -Force
    Remove-Item $ortZip -Force -ErrorAction SilentlyContinue
}
if (-not (Test-Path $ortHeader) -or -not (Test-Path $ortDll)) {
    throw "ONNX Runtime package did not contain the expected Windows x64 headers/DLL."
}

$depthModelUrl = "https://huggingface.co/AXERA-TECH/Depth-Anything-V2/resolve/main/depth_anything_v2_vits.onnx?download=true"
$depthModelSha256 = "443e95f17819f347f5f987384b8cb7d7d18ed6af6ac46dec9b0152748ba7dfd0"
Download-File -Url $depthModelUrl -Target $DepthModelPath -Label "Depth Anything V2 Small ONNX model"
$modelHash = (Get-FileHash -Path $DepthModelPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($modelHash -ne $depthModelSha256) {
    throw "Depth model checksum mismatch. Expected $depthModelSha256, got $modelHash. Delete '$DepthModelPath' before retrying."
}

Write-Host "Building and validating Area51XR + targeted MAME..."
$smokeArgs = @{
    MameRoot = $MameRoot
    MsysRoot = $MsysRoot
    OpenXrSdk = $OpenXrSdk
    OnnxRuntimeDir = $OnnxRuntimeDir
    DepthModelPath = $DepthModelPath
    RomPath = $RomPath
}
if ($AllowModifiedMedia) {
    $smokeArgs.AllowModifiedMedia = $true
}
& (Join-Path $PSScriptRoot "smoke-test.ps1") @smokeArgs

$effectiveRomPath = $RomPath
if (-not [string]::IsNullOrWhiteSpace($env:A51XR_MAME_MEDIA_PATH)) {
    $effectiveRomPath = $env:A51XR_MAME_MEDIA_PATH
    Write-Host "Using resolved MAME media path for live stages: $effectiveRomPath"
}

if (-not [string]::IsNullOrWhiteSpace($RomPath)) {
    $mameExe = Get-ChildItem -Path $MameRoot -Filter "*area51xr*.exe" -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
    if (-not $mameExe) {
        throw "Patched MAME executable was not found after the build."
    }

    Write-Host "Running desktop capture and reconstruction test..."
    & (Join-Path $PSScriptRoot "capture-mame-reconstruction.ps1") `
        -MameExe $mameExe `
        -RomPath $effectiveRomPath `
        -DepthModelPath $DepthModelPath

    Write-Host "Running desktop reconstruction sequence capture..."
    & (Join-Path $PSScriptRoot "capture-mame-sequence.ps1") `
        -MameExe $mameExe `
        -RomPath $effectiveRomPath `
        -DepthModelPath $DepthModelPath `
        -FrameCount 4

    Write-Host "Running live Area 51 bridge test..."
    & (Join-Path $PSScriptRoot "live-mame-test.ps1") -MameExe $mameExe -RomPath $effectiveRomPath
}
else {
    Write-Host ""
    Write-Host "Build setup is complete."
    Write-Host "Depth model: $DepthModelPath"
    Write-Host "When Area 51 game files are available, rerun with -RomPath <folder> for capture, reconstruction, sequence, and live bridge tests."
}
