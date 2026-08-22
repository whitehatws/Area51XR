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

function Show-AlternateArea51Sets([string]$SearchRoot) {
    $installRoot = Split-Path -Parent ([System.IO.Path]::GetFullPath($SearchRoot))
    Write-Host ""
    Write-Host "Scanning $installRoot for alternate Area 51 board-ROM revisions..."

    $allFiles = @(Get-ChildItem -Path $installRoot -File -Recurse -ErrorAction SilentlyContinue)
    $names = @{}
    foreach ($file in $allFiles) {
        $names[$file.Name.ToLowerInvariant()] = $file.FullName
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
            if ($names.ContainsKey($required.ToLowerInvariant())) { $found++ }
        }
        Write-Host "$($set.Name): $found/$($set.Files.Count) expected ROM files found"
        if ($found -gt 0) {
            foreach ($required in $set.Files) {
                $key = $required.ToLowerInvariant()
                if ($names.ContainsKey($key)) {
                    Write-Host "  FOUND   $required -> $($names[$key])"
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
    throw "MAME reports required Area 51 board ROMs as missing. The CHD alone is not sufficient; use the alternate-set scan above to see whether a different legal Area 51 ROM revision is already present."
}

if ($AllowModifiedMedia) {
    Write-Host ""
    Write-Warning "AREA 51 MEDIA PREFLIGHT: MODIFIED MEDIA ACCEPTED. MAME reported differences but no required files as NOT FOUND."
    exit 0
}

throw "MAME reported mismatched Area 51 media. If the differences are intentional for this project, rerun this preflight with -AllowModifiedMedia."
