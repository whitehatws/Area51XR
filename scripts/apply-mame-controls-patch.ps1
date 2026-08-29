param(
    [Parameter(Mandatory = $true)]
    [string]$MameRoot,

    [switch]$Revert
)

$ErrorActionPreference = "Stop"

$jaguarMain = Join-Path $MameRoot "src\mame\atari\jaguar.cpp"
if (-not (Test-Path $jaguarMain)) {
    throw "MAME Jaguar source not found: $jaguarMain"
}

$raw = Get-Content -Raw -Path $jaguarMain
$newline = if ($raw.Contains("`r`n")) { "`r`n" } else { "`n" }
$text = $raw.Replace("`r`n", "`n")

# The bridge header uses Win32 shared-memory APIs. It must be included only
# after MAME's CPU/device headers have been parsed; otherwise Windows macros
# such as ERROR and EXCEPTION_ILLEGAL_INSTRUCTION collide with MAME symbols.
$legacyIncludeOriginal = @'
#include "jaguar.h"

#include "bus/ata/hdd.h"
'@.Replace("`r`n", "`n")
$legacyIncludePatched = @'
#include "jaguar.h"
#include "area51xr_mame_bridge.h"

#include "bus/ata/hdd.h"
'@.Replace("`r`n", "`n")
$safeIncludeOriginal = '#include "cdrom.h"'
$safeIncludePatched = @'
#include "cdrom.h"
#include "area51xr_mame_bridge.h"
'@.Replace("`r`n", "`n").TrimEnd("`n")

$gpioOriginal = @'
	map(0x04f17000, 0x04f17003).lr16(NAME([this] () { return uint16_t(m_system->read()); })); // GPIO3
	map(0x04f17800, 0x04f17803).w(FUNC(jaguar_state::latch_w));          // GPIO4
	map(0x04f17c00, 0x04f17c03).portr("P1_P2");      // GPIO5
'@.Replace("`r`n", "`n")
$gpioPatched = @'
	map(0x04f17000, 0x04f17003).lr16(NAME([this] () { return area51xr_mame::apply_system_input(uint16_t(m_system->read())); })); // GPIO3
	map(0x04f17800, 0x04f17803).w(FUNC(jaguar_state::latch_w));          // GPIO4
	map(0x04f17c00, 0x04f17c03).lr32(NAME([this] () { return area51xr_mame::apply_p1_p2_input(uint32_t(ioport("P1_P2")->read())); })); // GPIO5
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
    [System.IO.File]::WriteAllText($jaguarMain, $Content)
}

if ($Revert) {
    if ($text.Contains($gpioPatched)) {
        $text = Replace-Required $text $gpioPatched $gpioOriginal "Area51XR R3000 cabinet GPIO hook"
    }

    if ($text.Contains($legacyIncludePatched)) {
        $text = Replace-Required $text $legacyIncludePatched $legacyIncludeOriginal "Area51XR legacy Jaguar include"
    }
    if ($text.Contains($safeIncludePatched)) {
        $text = Replace-Required $text $safeIncludePatched $safeIncludeOriginal "Area51XR safe Jaguar include"
    }

    Write-PreservedNewlines $text
    Write-Host "Area51XR cabinet-control integration reverted."
    exit 0
}

$changed = $false

# Automatically repair the earlier patch that inserted windows.h transitively
# before the M68000/MIPS headers.
if ($text.Contains($legacyIncludePatched)) {
    $text = Replace-Required $text $legacyIncludePatched $legacyIncludeOriginal "Area51XR legacy Jaguar include"
    $changed = $true
}

if (-not $text.Contains($safeIncludePatched)) {
    $text = Replace-Required $text $safeIncludeOriginal $safeIncludePatched "safe post-device include point"
    $changed = $true
}

if (-not $text.Contains($gpioPatched)) {
    $text = Replace-Required $text $gpioOriginal $gpioPatched "R3000 cabinet GPIO block"
    $changed = $true
}

if ($changed) {
    Write-PreservedNewlines $text
    Write-Host "Area51XR cabinet-control integration applied/upgraded."
}
else {
    Write-Host "Area51XR cabinet-control integration is already applied."
}
