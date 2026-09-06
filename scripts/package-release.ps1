param(
    [string]$Version = "1.0.0",
    [string]$MameRoot = "",
    [string]$MsysRoot = "C:\msys64",
    [string]$OutputDir = "",
    [switch]$SkipSourceArchive,
    [switch]$AllowDirty
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($MameRoot)) {
    $MameRoot = Join-Path $root "external\mame"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "artifacts"
}

$buildDir = Join-Path $root "build-mingw"
$releaseTemplate = Join-Path $root "release"
$ucrtBin = Join-Path $MsysRoot "ucrt64\bin"
$ucrtLicenses = Join-Path $MsysRoot "ucrt64\share\licenses"
$hostExe = Join-Path $buildDir "area51xr.exe"
$loaderDll = Join-Path $buildDir "openxr_loader.dll"
$objdump = Join-Path $ucrtBin "objdump.exe"

$emulatorExe = @(
    (Join-Path $MameRoot "mamearea51xr.exe"),
    (Join-Path $MameRoot "area51xr.exe")
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $emulatorExe) {
    $emulatorExe = Get-ChildItem -Path $MameRoot -Filter "*area51xr*.exe" -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

foreach ($required in @($MameRoot, $buildDir, $releaseTemplate, $ucrtBin, $hostExe, $loaderDll, $objdump, $emulatorExe)) {
    if (-not $required -or -not (Test-Path $required)) {
        throw "Release prerequisite not found: $required"
    }
}

$area51xrCommit = (& git -C $root rev-parse HEAD).Trim()
$mameCommit = (& git -C $MameRoot rev-parse HEAD).Trim()
if ([string]::IsNullOrWhiteSpace($area51xrCommit) -or [string]::IsNullOrWhiteSpace($mameCommit)) {
    throw "Unable to resolve source revisions for release packaging."
}

$area51xrStatus = @(& git -C $root status --porcelain)
if ($area51xrStatus.Count -gt 0 -and -not $AllowDirty) {
    throw "Area51XR working tree is dirty. Commit/stash changes before packaging, or use -AllowDirty for a non-publishable local package."
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$stageName = "Area51XR-$Version-win64"
$stage = Join-Path $OutputDir $stageName
$playerZip = Join-Path $OutputDir "$stageName.zip"
$sourceZip = Join-Path $OutputDir "Area51XR-$Version-mame-source.zip"
$tempRoot = Join-Path $OutputDir ".package-$Version"

Remove-Item $stage, $playerZip, $sourceZip, $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stage | Out-Null
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

Write-Host "[1/8] Staging release template..."
Copy-Item (Join-Path $releaseTemplate "*") $stage -Recurse -Force
$binStage = Join-Path $stage "bin"
$emulatorStage = Join-Path $stage "emulator"
$licenseStage = Join-Path $stage "licenses"
New-Item -ItemType Directory -Force -Path $binStage, $emulatorStage, $licenseStage | Out-Null

Copy-Item $hostExe (Join-Path $binStage "area51xr.exe") -Force
Copy-Item $loaderDll (Join-Path $binStage "openxr_loader.dll") -Force
Copy-Item $emulatorExe (Join-Path $emulatorStage "area51xr.exe") -Force

function Get-ImportedDllNames([string]$ExePath) {
    $lines = & $objdump -p $ExePath 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inspect DLL dependencies for $ExePath"
    }
    return @($lines | ForEach-Object {
        if ($_ -match '^\s*DLL Name:\s*(.+?)\s*$') { $Matches[1] }
    } | Where-Object { $_ } | Sort-Object -Unique)
}

$systemDllNames = @(
    'ADVAPI32.dll','bcrypt.dll','COMDLG32.dll','COMCTL32.dll','d3d11.dll','dbghelp.dll',
    'dxgi.dll','GDI32.dll','IMM32.dll','IPHLPAPI.DLL','KERNEL32.dll','MSVCRT.dll','ntdll.dll',
    'OLE32.dll','OLEAUT32.dll','POWRPROF.dll','PSAPI.DLL','RPCRT4.dll','SHELL32.dll','SHLWAPI.dll',
    'USER32.dll','USERENV.dll','UXTHEME.dll','VERSION.dll','WINMM.dll','WS2_32.dll','WTSAPI32.dll'
)

function Stage-NativeDependencies([string]$ExePath, [string]$Destination) {
    foreach ($dllName in Get-ImportedDllNames $ExePath) {
        if ($systemDllNames -contains $dllName) { continue }
        if ($dllName -ieq 'openxr_loader.dll') { continue }
        $candidate = Join-Path $ucrtBin $dllName
        if (Test-Path $candidate) {
            Copy-Item $candidate (Join-Path $Destination $dllName) -Force
        }
        else {
            Write-Host "  Note: dependency '$dllName' was not found in UCRT64; assuming it is provided by Windows or the application package."
        }
    }
}

Write-Host "[2/8] Staging native runtime dependencies..."
Stage-NativeDependencies $hostExe $binStage
Stage-NativeDependencies $emulatorExe $emulatorStage

Write-Host "[3/8] Staging toolchain/OpenXR license files..."
if (Test-Path $ucrtLicenses) {
    $licenseDirs = @(Get-ChildItem -Path $ucrtLicenses -Directory -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match '(?i)openxr|gcc|winpthread|mingw'
    })
    foreach ($licenseDir in $licenseDirs) {
        Copy-Item $licenseDir.FullName (Join-Path $licenseStage $licenseDir.Name) -Recurse -Force
    }
}

Write-Host "[4/8] Staging emulator licensing and source notice..."
$copying = Join-Path $MameRoot "COPYING"
$legalDir = Join-Path $MameRoot "docs\legal"
if (-not (Test-Path $copying) -or -not (Test-Path $legalDir)) {
    throw "MAME legal files are missing from the source tree."
}
Copy-Item $copying (Join-Path $emulatorStage "COPYING") -Force
New-Item -ItemType Directory -Force -Path (Join-Path $emulatorStage "docs") | Out-Null
Copy-Item $legalDir (Join-Path $emulatorStage "docs\legal") -Recurse -Force

$sourceNotice = @"
AREA51XR MODIFIED EMULATOR SOURCE
=================================

The emulator executable in this directory is built from MAME source code modified for Area51XR.

Upstream source revision:
$mameCommit

The exact corresponding source used for this Area51XR release is distributed alongside the player ZIP as:

Area51XR-$Version-mame-source.zip

The source archive includes the full upstream source snapshot for the revision above with the Area51XR modifications overlaid. No original Area 51 game ROMs, CHDs, or other copyrighted game media are included.

MAME is distributed under the GNU General Public License version 2 or later. See COPYING and docs\legal in this directory.
"@
$sourceNotice | Set-Content -Path (Join-Path $emulatorStage "SOURCE.txt") -Encoding UTF8

$metadata = @"
AREA51XR BUILD METADATA
=======================
Version: $Version
Area51XR Git revision: $area51xrCommit
MAME upstream Git revision: $mameCommit
Package generated: $(Get-Date -Format o)
Architecture: Windows x64
Launch scope: OpenXR flat-screen VR/light-gun v1 gameplay path
Game media included: NO
"@
$metadata | Set-Content -Path (Join-Path $stage "BUILD-METADATA.txt") -Encoding UTF8

Write-Host "[5/8] Enforcing no-game-media release boundary..."
$bannedNames = @(
    'area51.zip',
    'area51.chd',
    '2-c_area_51_hh.hh',
    '2-c_area_51_hl.hl',
    '2-c_area_51_lh.lh',
    '2-c_area_51_ll.ll',
    'jagwave.rom'
)
$forbidden = @(Get-ChildItem -Path $stage -Recurse -File | Where-Object {
    $_.Extension -ieq '.chd' -or $bannedNames -contains $_.Name.ToLowerInvariant()
})
if ($forbidden.Count -gt 0) {
    $names = ($forbidden.FullName -join [Environment]::NewLine)
    throw "Release packaging refused because game media was found in the player stage:`n$names"
}

Write-Host "[6/8] Creating player hashes and ZIP..."
$hashLines = Get-ChildItem -Path $stage -Recurse -File |
    Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($stage.Length + 1).Replace('\\','/')
        $hash = (Get-FileHash -Algorithm SHA256 -Path $_.FullName).Hash.ToLowerInvariant()
        "$hash  $relative"
    }
$hashLines | Set-Content -Path (Join-Path $stage "SHA256SUMS.txt") -Encoding ASCII
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $playerZip -CompressionLevel Optimal

if (-not $SkipSourceArchive) {
    Write-Host "[7/8] Building full corresponding-source ZIP..."
    $upstreamZip = Join-Path $tempRoot "mame-upstream.zip"
    $sourceStage = Join-Path $tempRoot "mame-source"
    & git -C $MameRoot archive --format=zip --output=$upstreamZip HEAD
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $upstreamZip)) {
        throw "Could not archive upstream MAME source revision $mameCommit."
    }
    Expand-Archive -Path $upstreamZip -DestinationPath $sourceStage -Force

    foreach ($relativePath in @(
        "src\mame\atari\jaguar.cpp",
        "src\mame\atari\jaguar_v.cpp",
        "src\mame\atari\area51xr_mame_bridge.h"
    )) {
        $sourceFile = Join-Path $MameRoot $relativePath
        if (-not (Test-Path $sourceFile)) {
            throw "Modified emulator source file is missing: $sourceFile"
        }
        $destinationFile = Join-Path $sourceStage $relativePath
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destinationFile) | Out-Null
        Copy-Item $sourceFile $destinationFile -Force
    }

    $mameStatus = (& git -C $MameRoot status --short) -join [Environment]::NewLine
    $buildNote = @"
AREA51XR MAME CORRESPONDING SOURCE
=================================

Area51XR release: $Version
Area51XR Git revision: $area51xrCommit
Upstream MAME Git revision: $mameCommit

This archive is the full MAME source snapshot for the revision above with the Area51XR source modifications used to build the bundled emulator overlaid in place.

Relevant Area51XR-modified files:
- src\mame\atari\jaguar.cpp
- src\mame\atari\jaguar_v.cpp
- src\mame\atari\area51xr_mame_bridge.h

Representative Area51XR subtarget build command from the source root:

  make SUBTARGET=area51xr SOURCES=src/mame/atari/jaguar.cpp

Local MAME source-tree status when this package was created:
$mameStatus

No Area 51 ROMs, CHDs, or other original game media are included.
"@
    $buildNote | Set-Content -Path (Join-Path $sourceStage "AREA51XR-SOURCE-NOTE.txt") -Encoding UTF8
    Compress-Archive -Path (Join-Path $sourceStage "*") -DestinationPath $sourceZip -CompressionLevel Optimal
}
else {
    Write-Host "[7/8] Corresponding-source ZIP skipped by request. Do not publish the player ZIP without publishing matching corresponding source."
}

Write-Host "[8/8] Release package complete."
Write-Host "Player: $playerZip"
if (-not $SkipSourceArchive) { Write-Host "Source: $sourceZip" }
Write-Host "Area51XR revision: $area51xrCommit"
Write-Host "MAME revision: $mameCommit"
Write-Host "No Area 51 game media was included."

Remove-Item $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
