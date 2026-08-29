param(
    [Parameter(Mandatory = $true)]
    [string]$MameRoot,

    [switch]$Revert
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$bridge = Join-Path $repoRoot "mame\area51xr_mame_bridge.h"
$mameAtari = Join-Path $MameRoot "src\mame\atari"
$jaguarVideo = Join-Path $mameAtari "jaguar_v.cpp"
$targetBridge = Join-Path $mameAtari "area51xr_mame_bridge.h"

if (-not (Test-Path $jaguarVideo)) {
    throw "MAME source tree not found at '$MameRoot'. Expected $jaguarVideo"
}

$raw = Get-Content -Raw -Path $jaguarVideo
$newline = if ($raw.Contains("`r`n")) { "`r`n" } else { "`n" }
$text = $raw.Replace("`r`n", "`n")

$includeOriginal = @'
#include "jaguar.h"
#include "jagblit.h"

#include "endianness.h"
'@.Replace("`r`n", "`n")
$includePatched = @'
#include "jaguar.h"
#include "jagblit.h"
#include "area51xr_mame_bridge.h"

#include "endianness.h"
'@.Replace("`r`n", "`n")

$gunOriginal = @'
		case 1:
			get_crosshair_xy(0, beamx, beamy);
			return (beamy << 16) | (beamx ^ 0x1ff);

		case 2:
			return ioport("IN3")->read();
'@.Replace("`r`n", "`n")
$gunPatchedOld = @'
		case 1:
			if (const auto *const xr_gun = area51xr_mame::gun_state())
			{
				const rectangle &visarea = m_screen->visible_area();
				const uint8_t rawx = area51xr_mame::normalized_to_mame_axis(xr_gun->aim_x);
				const uint8_t rawy = area51xr_mame::normalized_to_mame_axis(xr_gun->aim_y);
				beamx = visarea.left() + ((rawx * visarea.width()) >> 8);
				beamy = visarea.top() + ((rawy * visarea.height()) >> 8);
			}
			else
			{
				get_crosshair_xy(0, beamx, beamy);
			}
			return (beamy << 16) | (beamx ^ 0x1ff);

		case 2:
		{
			uint32_t input = ioport("IN3")->read();
			if (const auto *const xr_gun = area51xr_mame::gun_state())
			{
				input |= 0x00080000;
				if (xr_gun->trigger)
					input &= ~0x00080000;
			}
			return input;
		}
'@.Replace("`r`n", "`n")
$gunPatched = @'
		case 1:
			if (const auto *const xr_gun = area51xr_mame::gun_state())
			{
				const rectangle &visarea = m_screen->visible_area();
				if (xr_gun->offscreen)
				{
					beamx = 0;
					beamy = 0;
				}
				else
				{
					const uint8_t rawx = area51xr_mame::normalized_to_mame_axis(xr_gun->aim_x);
					const uint8_t rawy = area51xr_mame::normalized_to_mame_axis(xr_gun->aim_y);
					beamx = visarea.left() + ((rawx * visarea.width()) >> 8);
					beamy = visarea.top() + ((rawy * visarea.height()) >> 8);
				}
			}
			else
			{
				get_crosshair_xy(0, beamx, beamy);
			}
			return (beamy << 16) | (beamx ^ 0x1ff);

		case 2:
		{
			uint32_t input = ioport("IN3")->read();
			if (const auto *const xr_gun = area51xr_mame::gun_state())
			{
				input |= 0x00080000;
				if (xr_gun->trigger)
					input &= ~0x00080000;
			}
			return input;
		}
'@.Replace("`r`n", "`n")

$screenOriginal = @'
	/* render the object list */
	copybitmap(bitmap, m_screen_bitmap, 0, 0, 0, 0, cliprect);
	return 0;
'@.Replace("`r`n", "`n")
$screenPatchedOld = @'
	/* render the object list */
	copybitmap(bitmap, m_screen_bitmap, 0, 0, 0, 0, cliprect);
	area51xr_mame::publish_bitmap(bitmap, cliprect);
	return 0;
'@.Replace("`r`n", "`n")
$screenPatched = @'
	/* render the object list */
	copybitmap(bitmap, m_screen_bitmap, 0, 0, 0, 0, cliprect);
	area51xr_mame::publish_bitmap(bitmap, m_screen->visible_area());
	return 0;
'@.Replace("`r`n", "`n")

function Replace-Required([string]$Source, [string]$From, [string]$To, [string]$Label) {
    if (-not $Source.Contains($From)) {
        throw "Could not find expected MAME source block: $Label. The MAME source may have changed."
    }
    return $Source.Replace($From, $To)
}

function Write-PreservedNewlines([string]$Content) {
    if ($newline -eq "`r`n") {
        $Content = $Content.Replace("`n", "`r`n")
    }
    [System.IO.File]::WriteAllText($jaguarVideo, $Content)
}

if ($Revert) {
    if ($text.Contains($includeOriginal) -and $text.Contains($gunOriginal) -and $text.Contains($screenOriginal)) {
        Remove-Item $targetBridge -ErrorAction SilentlyContinue
        Write-Host "Area51XR MAME integration is already reverted."
        exit 0
    }

    $text = Replace-Required $text $includePatched $includeOriginal "Area51XR include"
    if ($text.Contains($gunPatched)) {
        $text = Replace-Required $text $gunPatched $gunOriginal "Area51XR gun hook"
    }
    elseif ($text.Contains($gunPatchedOld)) {
        $text = Replace-Required $text $gunPatchedOld $gunOriginal "Area51XR legacy gun hook"
    }
    else {
        throw "Could not find expected MAME source block: Area51XR gun hook. The MAME source may have changed."
    }
    if ($text.Contains($screenPatched)) {
        $text = Replace-Required $text $screenPatched $screenOriginal "Area51XR frame hook"
    }
    elseif ($text.Contains($screenPatchedOld)) {
        $text = Replace-Required $text $screenPatchedOld $screenOriginal "Area51XR legacy frame hook"
    }
    else {
        throw "Could not find expected MAME source block: Area51XR frame hook. The MAME source may have changed."
    }
    Write-PreservedNewlines $text
    Remove-Item $targetBridge -ErrorAction SilentlyContinue
    Write-Host "Area51XR MAME integration reverted."
    exit 0
}

$upgraded = $false
if ($text.Contains($gunPatchedOld)) {
    $text = Replace-Required $text $gunPatchedOld $gunPatched "Area51XR legacy gun hook"
    $upgraded = $true
}
if ($text.Contains($screenPatchedOld)) {
    $text = Replace-Required $text $screenPatchedOld $screenPatched "Area51XR legacy frame hook"
    $upgraded = $true
}

if ($text.Contains($includePatched) -and $text.Contains($gunPatched) -and $text.Contains($screenPatched)) {
    Copy-Item $bridge $targetBridge -Force
    if ($upgraded) {
        Write-PreservedNewlines $text
        Write-Host "Area51XR MAME integration upgraded."
    }
    else {
        Write-Host "Area51XR MAME integration is already applied."
    }
    exit 0
}

$text = Replace-Required $text $includeOriginal $includePatched "Jaguar include block"
$text = Replace-Required $text $gunOriginal $gunPatched "CoJag Player 1 gun input block"
$text = Replace-Required $text $screenOriginal $screenPatched "Jaguar screen update block"

Copy-Item $bridge $targetBridge -Force
Write-PreservedNewlines $text
Write-Host "Area51XR MAME integration applied."
