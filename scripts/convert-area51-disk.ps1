param(
    [string]$DiskImage = "D:\MAME\Area51.iso",
    [string]$MameRoot = "D:\MAME",
    [string]$OutputChd = "D:\MAME\roms\area51\area51.chd",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Get-PeMachine([string]$Path) {
    try {
        $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            $reader = [System.IO.BinaryReader]::new($stream)
            try {
                if ($stream.Length -lt 64) { return $null }
                if ($reader.ReadUInt16() -ne 0x5A4D) { return $null } # MZ
                $stream.Position = 0x3C
                $peOffset = $reader.ReadInt32()
                if ($peOffset -lt 0 -or $peOffset -gt ($stream.Length - 6)) { return $null }
                $stream.Position = $peOffset
                if ($reader.ReadUInt32() -ne 0x00004550) { return $null } # PE\0\0
                return $reader.ReadUInt16()
            }
            finally {
                $reader.Dispose()
            }
        }
        finally {
            $stream.Dispose()
        }
    }
    catch {
        Write-Host "PE inspection failed for '$Path': $($_.Exception.Message)"
        return $null
    }
}

function Get-FilePrefixHex([string]$Path, [int]$Count = 8) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($Path)
        $take = [Math]::Min($Count, $bytes.Length)
        if ($take -le 0) { return "<empty>" }
        return (($bytes[0..($take - 1)] | ForEach-Object { $_.ToString("X2") }) -join ' ')
    }
    catch {
        return "<unreadable>"
    }
}

if (-not (Test-Path $DiskImage)) {
    throw "Disk image not found: $DiskImage"
}
if (-not (Test-Path $MameRoot)) {
    throw "MAME root not found: $MameRoot"
}

Write-Host "Searching for a runnable Windows chdman.exe under $MameRoot..."
$candidates = @(Get-ChildItem -Path $MameRoot -Filter "chdman.exe" -File -Recurse -ErrorAction SilentlyContinue)
$valid = @()
foreach ($candidate in $candidates) {
    $machine = Get-PeMachine $candidate.FullName
    if ($machine -eq 0x8664 -or $machine -eq 0x014c) {
        $arch = if ($machine -eq 0x8664) { "x64" } else { "x86" }
        Write-Host "Valid Windows PE candidate ($arch): $($candidate.FullName)"
        $valid += $candidate
    }
    else {
        $prefix = Get-FilePrefixHex $candidate.FullName
        Write-Host "Skipping non-Windows/invalid candidate: $($candidate.FullName)"
        Write-Host "  Size: $($candidate.Length) bytes"
        Write-Host "  First bytes: $prefix"
    }
}

if ($valid.Count -eq 0) {
    throw "No runnable Windows chdman.exe was found under '$MameRoot'. A Windows executable should begin with bytes '4D 5A' (MZ). If the diagnostics above show something else, the extracted/downloaded file is not the Windows chdman binary."
}

$chdman = $valid[0].FullName
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
