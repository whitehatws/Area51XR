param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [int]$LookbackMinutes = 10
)

$ErrorActionPreference = "Continue"

$build = [System.IO.Path]::GetFullPath($BuildDir)
$parent = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

$cutoff = (Get-Date).AddMinutes(-[Math]::Abs($LookbackMinutes))
$targets = @(
    "area51xr_mame_ipc_tests.exe",
    "area51xr_depth_mesh_tests.exe"
)

$lines = New-Object System.Collections.Generic.List[string]
function Add-Line([string]$Text = "") {
    $lines.Add($Text)
}

Add-Line "Area51XR Windows executable-block diagnostics"
Add-Line "Generated: $(Get-Date -Format o)"
Add-Line "BuildDir: $build"
Add-Line "Cutoff: $($cutoff.ToString('o'))"
Add-Line

Add-Line "=== TARGET FILES ==="
foreach ($name in $targets) {
    $path = Join-Path $build $name
    Add-Line "[$name]"
    if (-not (Test-Path $path)) {
        Add-Line "missing: $path"
        Add-Line
        continue
    }

    $item = Get-Item $path
    Add-Line "path: $($item.FullName)"
    Add-Line "size: $($item.Length)"
    Add-Line "last_write: $($item.LastWriteTime.ToString('o'))"

    try {
        $hash = Get-FileHash -Path $path -Algorithm SHA256
        Add-Line "sha256: $($hash.Hash)"
    } catch {
        Add-Line "sha256_error: $($_.Exception.Message)"
    }

    try {
        $sig = Get-AuthenticodeSignature -FilePath $path
        Add-Line "signature_status: $($sig.Status)"
        Add-Line "signature_message: $($sig.StatusMessage)"
        if ($sig.SignerCertificate) {
            Add-Line "signer_subject: $($sig.SignerCertificate.Subject)"
        }
    } catch {
        Add-Line "signature_error: $($_.Exception.Message)"
    }

    try {
        $zone = Get-Content -Path $path -Stream Zone.Identifier -ErrorAction Stop
        Add-Line "zone_identifier:"
        foreach ($entry in $zone) { Add-Line "  $entry" }
    } catch {
        Add-Line "zone_identifier: none/unavailable"
    }
    Add-Line
}

Add-Line "=== MICROSOFT DEFENDER THREAT DETECTIONS ==="
try {
    $detections = Get-MpThreatDetection -ErrorAction Stop |
        Where-Object {
            $_.InitialDetectionTime -ge $cutoff -and
            (($_.Resources -join " ") -match "Area51XR|area51xr_")
        } |
        Sort-Object InitialDetectionTime
    if (-not $detections) {
        Add-Line "none"
    } else {
        foreach ($d in $detections) {
            Add-Line ($d | Select-Object InitialDetectionTime,ThreatID,ThreatStatusID,ActionSuccess,Resources | Format-List | Out-String).TrimEnd()
        }
    }
} catch {
    Add-Line "unavailable: $($_.Exception.Message)"
}
Add-Line

$logs = @(
    "Microsoft-Windows-Windows Defender/Operational",
    "Microsoft-Windows-CodeIntegrity/Operational",
    "Microsoft-Windows-AppLocker/EXE and DLL",
    "Microsoft-Windows-Windows Firewall With Advanced Security/Firewall"
)

foreach ($log in $logs) {
    Add-Line "=== EVENT LOG: $log ==="
    try {
        $events = Get-WinEvent -FilterHashtable @{ LogName = $log; StartTime = $cutoff } -ErrorAction Stop |
            Where-Object { $_.Message -match "Area51XR|area51xr_|build-mingw" } |
            Select-Object -First 100
        if (-not $events) {
            Add-Line "none"
        } else {
            foreach ($event in $events) {
                Add-Line "time=$($event.TimeCreated.ToString('o')) id=$($event.Id) level=$($event.LevelDisplayName) provider=$($event.ProviderName)"
                Add-Line ($event.Message -replace "`r?`n", " | ")
                Add-Line
            }
        }
    } catch {
        Add-Line "unavailable: $($_.Exception.Message)"
    }
    Add-Line
}

$lines | Set-Content -Path $OutputPath -Encoding UTF8
Write-Host "Windows block diagnostics: $OutputPath"
