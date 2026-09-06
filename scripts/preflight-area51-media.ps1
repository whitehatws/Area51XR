param(
    [string]$RomPath = "D:\MAME\roms",
    [string]$MameRoot = "",
    [switch]$AllowModifiedMedia
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($MameRoot)) {
    $MameRoot = Join-Path $root "external\mame"
}

$resolver = Join-Path $PSScriptRoot "resolve-mame-media.ps1"
if (-not (Test-Path $resolver)) {
    throw "Media resolver not found: $resolver"
}
if (-not (Test-Path $RomPath)) {
    throw "ROM path not found: $RomPath"
}
if (-not (Test-Path $MameRoot)) {
    throw "MAME root not found: $MameRoot"
}

function Get-ArchiveEntryNames([string]$ArchivePath) {
    $entries = New-Object System.Collections.Generic.List[string]
    $lower = $ArchivePath.ToLowerInvariant()

    if ($lower.EndsWith('.zip')) {
        try {
            Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
            $zip = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
            try {
                foreach ($entry in $zip.Entries) {
                    if (-not [string]::IsNullOrWhiteSpace($entry.Name)) {
                        $entries.Add($entry.FullName)
                    }
                }
            }
            finally {
                $zip.Dispose()
            }
            return $entries.ToArray()
        }
        catch {
            Write-Host "  Warning: could not inspect ZIP '$ArchivePath': $_"
        }
    }

    $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
    if ($tar -and ($lower.EndsWith('.tar') -or $lower.EndsWith('.tgz') -or $lower.EndsWith('.tar.gz'))) {
        try {
            $listed = @(& $tar.Source -tf $ArchivePath 2>$null)
            foreach ($entry in $listed) {
                if (-not [string]::IsNullOrWhiteSpace([string]$entry)) {
                    $entries.Add([string]$entry)
                }
            }
        }
        catch {
            Write-Host "  Warning: could not inspect TAR '$ArchivePath': $_"
        }
    }

    return $entries.ToArray()
}

function Show-AlternateArea51Sets([string]$SearchRoot) {
    $installRoot = Split-Path -Parent ([System.IO.Path]::GetFullPath($SearchRoot))
    Write-Host ""
    Write-Host "Scanning $installRoot for Area 51 board-ROM revisions (loose files and archives)..."

    $allFiles = @(Get-ChildItem -Path $installRoot -File -Recurse -ErrorAction SilentlyContinue)
    $locations = @{}

    function Add-RomLocation([string]$Name, [string]$Location) {
        if ([string]::IsNullOrWhiteSpace($Name)) { return }
        $key = [System.IO.Path]::GetFileName($Name).ToLowerInvariant()
        if (-not $locations.ContainsKey($key)) {
            $locations[$key] = New-Object System.Collections.Generic.List[string]
        }
        if (-not $locations[$key].Contains($Location)) {
            $locations[$key].Add($Location)
        }
    }

    foreach ($file in $allFiles) {
        Add-RomLocation -Name $file.Name -Location $file.FullName
    }

    $archives = @($allFiles | Where-Object {
        $n = $_.Name.ToLowerInvariant()
        $n.EndsWith('.zip') -or $n.EndsWith('.tar') -or $n.EndsWith('.tgz') -or $n.EndsWith('.tar.gz')
    })

    if ($archives.Count -gt 0) {
        Write-Host "Inspecting $($archives.Count) archive(s) for known Area 51 ROM filenames..."
    }
    foreach ($archive in $archives) {
        foreach ($entry in @(Get-ArchiveEntryNames -ArchivePath $archive.FullName)) {
            $leaf = [System.IO.Path]::GetFileName(([string]$entry -replace '/', '\'))
            if (-not [string]::IsNullOrWhiteSpace($leaf)) {
                Add-RomLocation -Name $leaf -Location "$($archive.FullName) :: $entry"
            }
        }
    }

    $sets = @(
        [ordered]@{
            Name = "area51 (R3000)"
            Files = @("2-c_area_51_hh.hh", "2-c_area_51_hl.hl", "2-c_area_51_lh.lh", "2-c_area_51_ll.ll", "jagwave.rom")
        },
        [ordered]@{
            Name = "area51t (68020 Time Warner)"
            Files = @("136105-0003-q_h.3h", "136105-0002-q_p.3p", "136105-0001-q_m.3m", "136105-0000-q_k.3k", "jagwave.rom")
        },
        [ordered]@{
            Name = "area51ta (older 68020 Time Warner)"
            Files = @("136105-0003c.3h", "136105-0002c.3p", "136105-0001c.3m", "136105-0000c.3k", "jagwave.rom")
        }
    )

    foreach ($set in $sets) {
        $found = 0
        foreach ($required in $set.Files) {
            if ($locations.ContainsKey($required.ToLowerInvariant())) { $found++ }
        }
        Write-Host "$($set.Name): $found/$($set.Files.Count) expected ROM files found"
        if ($found -gt 0) {
            foreach ($required in $set.Files) {
                $key = $required.ToLowerInvariant()
                if ($locations.ContainsKey($key)) {
                    foreach ($location in $locations[$key]) {
                        Write-Host "  FOUND   $required -> $location"
                    }
                }
                else {
                    Write-Host "  MISSING $required"
                }
            }
        }
    }
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
    throw "Patched Area51XR MAME executable not found under '$MameRoot'. Run the Windows bootstrap once first."
}

Write-Host "MAME: $mameExe"
$resolvedOutput = @(& $resolver -RomPath $RomPath)
$mameMediaPath = $env:A51XR_MAME_MEDIA_PATH
if ([string]::IsNullOrWhiteSpace($mameMediaPath) -and $resolvedOutput.Count -gt 0) {
    $mameMediaPath = [string]$resolvedOutput[$resolvedOutput.Count - 1]
}
if ([string]::IsNullOrWhiteSpace($mameMediaPath)) {
    throw "Unable to resolve MAME media path."
}

Write-Host ""
Write-Host "Running MAME Area 51 media audit..."
$auditDir = Join-Path $root "logs\media-preflight"
New-Item -ItemType Directory -Force -Path $auditDir | Out-Null
$auditOut = Join-Path $auditDir "mame-verify-area51.out.txt"
$auditErr = Join-Path $auditDir "mame-verify-area51.err.txt"
Remove-Item $auditOut, $auditErr -Force -ErrorAction SilentlyContinue

$verify = Start-Process -FilePath $mameExe `
    -ArgumentList @("-rompath", $mameMediaPath, "-verifyroms", "area51") `
    -WorkingDirectory (Split-Path -Parent $mameExe) `
    -PassThru -Wait -NoNewWindow `
    -RedirectStandardOutput $auditOut -RedirectStandardError $auditErr
$exitCode = $verify.ExitCode

$output = @()
if (Test-Path $auditOut) { $output += Get-Content $auditOut }
if (Test-Path $auditErr) { $output += Get-Content $auditErr }
$output | Write-Host
Write-Host "MAME audit exit code: $exitCode"
Write-Host "Audit files: $auditDir"

if ($exitCode -eq 0) {
    Write-Host ""
    Write-Host "AREA 51 MEDIA PREFLIGHT: PASS (stock audit)"
    exit 0
}

$missingLines = @($output | Where-Object {
    $_ -match '(?i)\bNOT FOUND\b|required files are missing|not found in (the )?rompath|is missing|missing required'
})
if ($missingLines.Count -gt 0) {
    Write-Host ""
    Write-Host "AREA 51 MEDIA PREFLIGHT: MISSING REQUIRED FILES"
    $missingLines | ForEach-Object { Write-Host "  $_" }
    Show-AlternateArea51Sets -SearchRoot $RomPath
    throw "MAME reports required Area 51 board ROMs as missing. The CHD alone is not sufficient; the scan above also checks inside ZIP/TAR archives for a legal Area 51 ROM revision already present."
}

if ($AllowModifiedMedia) {
    Write-Host ""
    Write-Warning "AREA 51 MEDIA PREFLIGHT: MODIFIED MEDIA ACCEPTED. MAME reported differences but no required files as NOT FOUND."
    exit 0
}

throw "MAME reported mismatched Area 51 media. If the differences are intentional for this project, rerun this preflight with -AllowModifiedMedia."
