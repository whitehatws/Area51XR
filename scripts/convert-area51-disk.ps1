param(
    [string]$DiskImage = "D:\MAME\Area51.iso",
    [string]$MameRoot = "D:\MAME",
    [string]$OutputChd = "D:\MAME\roms\area51\area51.chd",
    [string]$MsysRoot = "C:\msys64",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$sourceMameRoot = Join-Path $root "external\mame"
$bash = Join-Path $MsysRoot "usr\bin\bash.exe"

function ConvertTo-MsysPath([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $matches[1].ToLowerInvariant()
        $tail = $matches[2] -replace '\\', '/'
        return "/$drive/$tail"
    }
    return ($full -replace '\\', '/')
}

function Get-PeMachine([string]$Path) {
    $stream = $null
    $reader = $null
    try {
        $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        $reader = [System.IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { return $null }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or $peOffset -gt ($stream.Length - 6)) { return $null }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { return $null }
        return $reader.ReadUInt16()
    }
    catch {
        return $null
    }
    finally {
        if ($reader) { $reader.Dispose() }
        elseif ($stream) { $stream.Dispose() }
    }
}

function Get-FirstBytes([string]$Path, [int]$Count = 8) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($Path)
        $take = [Math]::Min($Count, $bytes.Length)
        if ($take -eq 0) { return "<empty>" }
        return (($bytes[0..($take - 1)] | ForEach-Object { $_.ToString('X2') }) -join ' ')
    }
    catch {
        return "<unreadable>"
    }
}

function Find-ValidChdman([string[]]$SearchRoots) {
    foreach ($searchRoot in $SearchRoots) {
        if ([string]::IsNullOrWhiteSpace($searchRoot) -or -not (Test-Path $searchRoot)) { continue }
        foreach ($candidate in @(Get-ChildItem -Path $searchRoot -Filter "chdman.exe" -File -Recurse -ErrorAction SilentlyContinue)) {
            $machine = Get-PeMachine $candidate.FullName
            if ($machine -eq 0x8664 -or $machine -eq 0x014c) {
                $arch = if ($machine -eq 0x8664) { "x64" } else { "x86" }
                Write-Host "Valid Windows PE candidate ($arch): $($candidate.FullName)"
                try {
                    $probe = Start-Process -FilePath $candidate.FullName -ArgumentList @("info") -PassThru -Wait -NoNewWindow -ErrorAction Stop
                    return $candidate.FullName
                }
                catch {
                    Write-Host "Candidate is Windows PE but cannot execute here: $($candidate.FullName)"
                }
            }
            else {
                $size = $candidate.Length
                $prefix = Get-FirstBytes $candidate.FullName
                Write-Host "Skipping non-Windows/invalid candidate: $($candidate.FullName) (size=$size bytes, first=$prefix)"
            }
        }
    }
    return ""
}

function Build-LocalChdman {
    if (-not (Test-Path (Join-Path $sourceMameRoot "makefile"))) {
        throw "Area51XR MAME source tree not found at '$sourceMameRoot'. Run the Windows bootstrap first."
    }
    if (-not (Test-Path $bash)) {
        throw "MSYS2 bash not found at '$bash'. Run the Windows bootstrap first."
    }

    Write-Host "Building chdman from the Area51XR MAME source tree using the working UCRT64 toolchain..."
    $env:A51XR_MAME_ROOT_MSYS = ConvertTo-MsysPath $sourceMameRoot
    $env:A51XR_MAME_JOBS = [string][Math]::Max(2, [Math]::Min(8, [Environment]::ProcessorCount))
    $buildCommand = @'
export OS=Windows_NT
export MSYSTEM=UCRT64
export MINGW_PREFIX=/ucrt64
export MINGW_CHOST=x86_64-w64-mingw32
export MINGW_PACKAGE_PREFIX=mingw-w64-ucrt-x86_64
export PATH=/ucrt64/bin:/usr/bin:$PATH
cd "$A51XR_MAME_ROOT_MSYS"
make SUBTARGET=area51xr SOURCES=src/mame/atari/jaguar.cpp TOOLS=1 REGENIE=1 -j"$A51XR_MAME_JOBS"
'@
    & $bash -lc $buildCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Local MAME tools build failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path $DiskImage)) {
    throw "Disk image not found: $DiskImage"
}
if (-not (Test-Path $MameRoot)) {
    throw "MAME root not found: $MameRoot"
}

Write-Host "Searching for a runnable Windows chdman.exe..."
$chdman = Find-ValidChdman @($MameRoot, $sourceMameRoot)
if ([string]::IsNullOrWhiteSpace($chdman)) {
    Write-Host "No usable prebuilt chdman found."
    Build-LocalChdman
    $chdman = Find-ValidChdman @($sourceMameRoot)
}
if ([string]::IsNullOrWhiteSpace($chdman)) {
    throw "A compatible chdman.exe could not be produced from the local MAME source tree."
}

Write-Host "Using chdman: $chdman"

$outputDir = Split-Path -Parent $OutputChd
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
if (Test-Path $OutputChd) {
    if (-not $Force) {
        throw "Output already exists: $OutputChd. Rerun with -Force to replace it."
    }
    Remove-Item -Force $OutputChd
}

Write-Host "Creating hard-disk CHD from: $DiskImage"
& $chdman createhd -i $DiskImage -o $OutputChd
if ($LASTEXITCODE -ne 0) {
    throw "chdman createhd failed with exit code $LASTEXITCODE. If the input is a normal CD/DVD ISO rather than a raw hard-disk image, it is not the correct source for Area 51."
}

Write-Host "Inspecting generated CHD..."
& $chdman info -i $OutputChd
if ($LASTEXITCODE -ne 0) {
    throw "chdman info failed with exit code $LASTEXITCODE."
}

Write-Host ""
Write-Host "CHD created: $OutputChd"
Write-Host "Next: run scripts\resolve-mame-media.ps1 and then the Windows acceptance harness."
