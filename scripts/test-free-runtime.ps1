param(
    [string]$RomPath = "D:\MAME\roms",
    [ValidateSet("Auto", "MetaLink", "SteamVR")]
    [string]$Runtime = "Auto"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$playScript = Join-Path $PSScriptRoot "play-area51-vr.ps1"
$releaseScript = Join-Path $root "release\Start-Area51XR.ps1"

foreach ($required in @($RomPath, $playScript, $releaseScript)) {
    if (-not (Test-Path $required)) {
        throw "Required free-runtime test path not found: $required"
    }
}

function Assert-PowerShellSyntax([string]$Path) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($Path, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count -gt 0) {
        $messages = ($errors | ForEach-Object { "$($_.Extent.Text): $($_.Message)" }) -join [Environment]::NewLine
        throw "PowerShell syntax validation failed for '$Path':`n$messages"
    }
}

Write-Host "Validating Area51XR launcher syntax..."
Assert-PowerShellSyntax $playScript
Assert-PowerShellSyntax $releaseScript
Write-Host "Launcher syntax: PASS"

$metaManifest = "C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json"
$steamCandidates = New-Object System.Collections.Generic.List[string]
foreach ($candidate in @(
    "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json",
    "C:\Program Files\Steam\steamapps\common\SteamVR\steamxr_win64.json"
)) {
    if (Test-Path $candidate) { $steamCandidates.Add($candidate) }
}
foreach ($steamRegistry in @(
    "HKCU:\SOFTWARE\Valve\Steam",
    "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam"
)) {
    try {
        $props = Get-ItemProperty -Path $steamRegistry -ErrorAction Stop
        foreach ($propertyName in @("SteamPath", "InstallPath")) {
            $steamRoot = [string]$props.$propertyName
            if (-not [string]::IsNullOrWhiteSpace($steamRoot)) {
                $candidate = Join-Path $steamRoot "steamapps\common\SteamVR\steamxr_win64.json"
                if ((Test-Path $candidate) -and -not $steamCandidates.Contains($candidate)) {
                    $steamCandidates.Add($candidate)
                }
            }
        }
    }
    catch {}
}

$metaAvailable = Test-Path $metaManifest
$steamAvailable = $steamCandidates.Count -gt 0

Write-Host ""
Write-Host "Free PCVR runtimes detected:"
Write-Host "  Meta Horizon Link: $metaAvailable"
Write-Host "  SteamVR:          $steamAvailable"

$selected = $Runtime
if ($selected -eq "Auto") {
    if ($metaAvailable) { $selected = "MetaLink" }
    elseif ($steamAvailable) { $selected = "SteamVR" }
    else {
        throw "No free Area51XR PCVR runtime is installed. Install Meta Horizon Link or SteamVR first. Virtual Desktop is intentionally not used by this test."
    }
}

if ($selected -eq "MetaLink" -and -not $metaAvailable) {
    throw "Meta Horizon Link OpenXR runtime was not found at '$metaManifest'. Install Meta Horizon Link first."
}
if ($selected -eq "SteamVR" -and -not $steamAvailable) {
    throw "SteamVR OpenXR runtime was not found. Install SteamVR first."
}

Write-Host ""
Write-Host "FREE-RUNTIME HARDWARE TEST: $selected" -ForegroundColor Cyan
if ($selected -eq "MetaLink") {
    Write-Host "Before continuing, the Quest 3 must already be connected to the PC through Meta Horizon Link / Quest Link / Air Link."
}
else {
    Write-Host "Before continuing, the Quest 3 must already be connected through Steam Link and SteamVR must be running."
}
Write-Host ""
Write-Host "This test uses the existing known-good Area51XR and patched emulator binaries. It does not rebuild gameplay code."
Write-Host ""

& $playScript -RomPath $RomPath -Runtime $selected -SkipBuild
exit $LASTEXITCODE
