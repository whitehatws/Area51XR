param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath
)

$ErrorActionPreference = "Stop"

function Add-UniquePath([System.Collections.Generic.List[string]]$List, [string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return
    }
    $full = [System.IO.Path]::GetFullPath($Path)
    foreach ($existing in $List) {
        if ([string]::Equals($existing, $full, [System.StringComparison]::OrdinalIgnoreCase)) {
            return
        }
    }
    $List.Add($full)
}

$primary = [System.IO.Path]::GetFullPath($RomPath)
if (-not (Test-Path $primary)) {
    throw "ROM path not found: $primary"
}

$paths = New-Object 'System.Collections.Generic.List[string]'
Add-UniquePath $paths $primary

$mameHome = Split-Path -Parent $primary
foreach ($candidate in @(
    (Join-Path $mameHome "chds"),
    (Join-Path $mameHome "chd"),
    (Join-Path $mameHome "CHDs"),
    (Join-Path $mameHome "CHD")
)) {
    Add-UniquePath $paths $candidate
}

# Search the MAME installation root for the Area 51 parent ROM archive and CHD.
# MAME's rompath must point at the directory that CONTAINS the game-named
# directory.  For the standard layout
#   <media-root>\area51\area51.chd
# adding <media-root>\area51 itself would make MAME look one level too deep.
$area51Zip = Get-ChildItem -Path $mameHome -Filter "area51.zip" -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($area51Zip) {
    Add-UniquePath $paths $area51Zip.Directory.FullName
}

$area51Chd = Get-ChildItem -Path $mameHome -Filter "area51.chd" -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($area51Chd) {
    $chdDir = $area51Chd.Directory
    if ($chdDir.Name -ieq "area51" -and $chdDir.Parent) {
        # Standard MAME layout: <media-root>\area51\area51.chd
        Add-UniquePath $paths $chdDir.Parent.FullName
    }
    else {
        # Nonstandard direct placement: include the containing directory.
        Add-UniquePath $paths $chdDir.FullName
    }
}

# Loose/unpacked ROM sets are also valid MAME media.  If one of the known
# Area 51 program ROM filenames is found in an area51 directory, add its parent
# as the media root just as we do for the CHD directory.
$looseRom = Get-ChildItem -Path $mameHome -Filter "2-c_area_51_hh.hh" -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($looseRom) {
    $romDir = $looseRom.Directory
    if ($romDir.Name -ieq "area51" -and $romDir.Parent) {
        Add-UniquePath $paths $romDir.Parent.FullName
    }
    else {
        Add-UniquePath $paths $romDir.FullName
    }
}

$mediaPath = ($paths.ToArray() -join ';')
Write-Host "Resolved MAME media path: $mediaPath"
Write-Host "Area 51 ROM archive found: $([bool]$area51Zip)"
if ($area51Zip) { Write-Host "Area 51 ROM archive: $($area51Zip.FullName)" }
Write-Host "Area 51 loose ROM found: $([bool]$looseRom)"
if ($looseRom) { Write-Host "Area 51 loose ROM directory: $($looseRom.Directory.FullName)" }
Write-Host "Area 51 CHD found: $([bool]$area51Chd)"
if ($area51Chd) { Write-Host "Area 51 CHD: $($area51Chd.FullName)" }

# Keep the resolved value available to parent PowerShell scripts invoked with &.
$env:A51XR_MAME_MEDIA_PATH = $mediaPath
Write-Output $mediaPath
