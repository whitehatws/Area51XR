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

$text = Get-Content -Raw -Path $jaguarVideo

$includeOriginal = @'
#include "jaguar.h"
#include "jagblit.h"

#include "endianness.h"
'@
$includePatched = @'
#include "jaguar.h"
#include "jagblit.h"
#include "area51xr_mame_bridge.h"

#include "endianness.h"
'@

$gunOriginal = @'
		case 1:
			get_crosshair_xy(0, beamx, beamy);
			return (beamy << 16) | (beamx ^ 0x1ff);

		case 2:
			return ioport("IN3")->read();
'@
$gunPatched = @'
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
'@

$screenOriginal = @'
	/* render the object list */
	copybitmap(bitmap, m_screen_bitmap, 0, 0, 0, 0, cliprect);
	return 0;
'@
$screenPatched = @'
	/* render the object list */
	copybitmap(bitmap, m_screen_bitmap, 0, 0, 0, 0, cliprect);
	area51xr_mame::publish_bitmap(m_screen_bitmap);
	return 0;
'@

function Replace-Required([string]$Source, [string]$From, [string]$To, [string]$Label) {
    if (-not $Source.Contains($From)) {
        throw "Could not find expected MAME source block: $Label. The MAME source may have changed."
    }
    return $Source.Replace($From, $To)
}

if ($Revert) {
    if ($text.Contains($includeOriginal) -and $text.Contains($gunOriginal) -and $text.Contains($screenOriginal)) {
        Remove-Item $targetBridge -ErrorAction SilentlyContinue
        Write-Host "Area51XR MAME integration is already reverted."
        exit 0
    }

    $text = Replace-Required $text $includePatched $includeOriginal "Area51XR include"
    $text = Replace-Required $text $gunPatched $gunOriginal "Area51XR gun hook"
    $text = Replace-Required $text $screenPatched $screenOriginal "Area51XR frame hook"
    Set-Content -Path $jaguarVideo -Value $text -NoNewline
    Remove-Item $targetBridge -ErrorAction SilentlyContinue
    Write-Host "Area51XR MAME integration reverted."
    exit 0
}

if ($text.Contains($includePatched) -and $text.Contains($gunPatched) -and $text.Contains($screenPatched)) {
    Copy-Item $bridge $targetBridge -Force
    Write-Host "Area51XR MAME integration is already applied."
    exit 0
}

$text = Replace-Required $text $includeOriginal $includePatched "Jaguar include block"
$text = Replace-Required $text $gunOriginal $gunPatched "CoJag Player 1 gun input block"
$text = Replace-Required $text $screenOriginal $screenPatched "Jaguar screen update block"

Copy-Item $bridge $targetBridge -Force
Set-Content -Path $jaguarVideo -Value $text -NoNewline
Write-Host "Area51XR MAME integration applied."
