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

$includeOriginal = @'
#include "jaguar.h"

#include "bus/ata/hdd.h"
'@.Replace("`r`n", "`n")
$includePatched = @'
#include "jaguar.h"
#include "area51xr_mame_bridge.h"

#include "bus/ata/hdd.h"
'@.Replace("`r`n", "`n")

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
    if ($text.Contains($gpioOriginal) -and $text.Contains($includeOriginal)) {
        Write-Host "Area51XR cabinet-control integration is already reverted."
        exit 0
    }
    $text = Replace-Required $text $gpioPatched $gpioOriginal "Area51XR R3000 cabinet GPIO hook"
    $text = Replace-Required $text $includePatched $includeOriginal "Area51XR Jaguar main include"
    Write-PreservedNewlines $text
    Write-Host "Area51XR cabinet-control integration reverted."
    exit 0
}

if ($text.Contains($gpioPatched) -and $text.Contains($includePatched)) {
    Write-Host "Area51XR cabinet-control integration is already applied."
    exit 0
}

if (-not $text.Contains($includePatched)) {
    $text = Replace-Required $text $includeOriginal $includePatched "Jaguar main include block"
}
if (-not $text.Contains($gpioPatched)) {
    $text = Replace-Required $text $gpioOriginal $gpioPatched "R3000 cabinet GPIO block"
}

Write-PreservedNewlines $text
Write-Host "Area51XR cabinet-control integration applied."
