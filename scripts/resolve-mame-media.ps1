param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$support = Join-Path $root "release\Support\Area51XR.PlayerSupport.psm1"
if (-not (Test-Path $support)) { throw "Area51XR player-support module not found: $support" }
Import-Module $support -Force

$primary = [System.IO.Path]::GetFullPath($RomPath)
if (-not (Test-Path $primary)) { throw "ROM path not found: $primary" }
$playerData = Initialize-Area51XRPlayerData
$mediaPath = Resolve-Area51Media -SearchRoot $primary -DataRoot $playerData.Root

$env:A51XR_DATA_ROOT = $playerData.Root
$env:A51XR_MAME_MEDIA_PATH = $mediaPath
Write-Host "Resolved verified MAME media path: $mediaPath"
Write-Output $mediaPath
