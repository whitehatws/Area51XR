param(
    [string]$InputDir = "",

    [string]$OutputZip = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$logsRoot = Join-Path $root "logs"

if ([string]::IsNullOrWhiteSpace($InputDir)) {
    if (-not (Test-Path $logsRoot)) {
        throw "Logs directory not found: $logsRoot"
    }
    $latest = Get-ChildItem -Path $logsRoot -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $latest) {
        throw "No diagnostic log directories were found under $logsRoot"
    }
    $InputDir = $latest.FullName
}
if (-not (Test-Path $InputDir)) {
    throw "Input diagnostics directory not found: $InputDir"
}

if ([string]::IsNullOrWhiteSpace($OutputZip)) {
    $name = Split-Path -Leaf $InputDir
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputZip = Join-Path $logsRoot "$name-$stamp.zip"
}

$parent = Split-Path -Parent $OutputZip
New-Item -ItemType Directory -Force -Path $parent | Out-Null
if (Test-Path $OutputZip) {
    Remove-Item $OutputZip -Force
}

Compress-Archive -Path (Join-Path $InputDir "*") -DestinationPath $OutputZip -Force
Write-Host "Diagnostics package: $OutputZip"
