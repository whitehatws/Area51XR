param(
    [Parameter(Mandatory = $true)]
    [string]$MameRoot,

    [switch]$Revert
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$patch = Join-Path $repoRoot "mame\area51xr-mame.patch"
$bridge = Join-Path $repoRoot "mame\area51xr_mame_bridge.h"
$mameAtari = Join-Path $MameRoot "src\mame\atari"
$jaguarVideo = Join-Path $mameAtari "jaguar_v.cpp"
$targetBridge = Join-Path $mameAtari "area51xr_mame_bridge.h"

if (-not (Test-Path $jaguarVideo)) {
    throw "MAME source tree not found at '$MameRoot'. Expected $jaguarVideo"
}

if ($Revert) {
    git -C $MameRoot apply --reverse --check $patch
    git -C $MameRoot apply --reverse $patch
    if (Test-Path $targetBridge) {
        Remove-Item $targetBridge
    }
    Write-Host "Area51XR MAME patch reverted."
    exit 0
}

Copy-Item $bridge $targetBridge -Force
try {
    git -C $MameRoot apply --check $patch
    git -C $MameRoot apply $patch
}
catch {
    Remove-Item $targetBridge -ErrorAction SilentlyContinue
    throw
}

Write-Host "Area51XR MAME patch applied."
