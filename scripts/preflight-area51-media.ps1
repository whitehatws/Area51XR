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
    throw "MAME reports required Area 51 media as missing. Add the listed files to the area51 set before runtime acceptance."
}

if ($AllowModifiedMedia) {
    Write-Host ""
    Write-Warning "AREA 51 MEDIA PREFLIGHT: MODIFIED MEDIA ACCEPTED. MAME reported differences but no required files as NOT FOUND."
    exit 0
}

throw "MAME reported mismatched Area 51 media. If the differences are intentional for this project, rerun this preflight with -AllowModifiedMedia."
