param(
    [Parameter(Mandatory = $true)]
    [string]$CapturePath,

    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $root "logs\replay-$stamp"
}

$exeCandidates = @(
    (Join-Path $root "build-mingw\area51xr-reconstruct.exe"),
    (Join-Path $root "build\RelWithDebInfo\area51xr-reconstruct.exe"),
    (Join-Path $root "build\area51xr-reconstruct.exe")
)
$replayExe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $replayExe) {
    throw "area51xr-reconstruct was not found. Run the bootstrap/build first."
}
if (-not (Test-Path $CapturePath)) {
    throw "Capture not found: $CapturePath"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$baseName = [System.IO.Path]::GetFileNameWithoutExtension($CapturePath)
$objPath = Join-Path $OutputDir "$baseName.obj"
$reportPath = Join-Path $OutputDir "$baseName.quality.json"
$stdoutPath = Join-Path $OutputDir "replay.log"
$stderrPath = Join-Path $OutputDir "replay.err.log"

$process = Start-Process -FilePath $replayExe -ArgumentList @($CapturePath, $objPath, $reportPath) `
    -PassThru -Wait -NoNewWindow `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

if ($process.ExitCode -ne 0) {
    $stderrText = if (Test-Path $stderrPath) { Get-Content -Raw $stderrPath } else { "" }
    throw "Replay failed with exit code $($process.ExitCode). $stderrText Logs: $OutputDir"
}

Write-Host "Replay passed."
Write-Host "OBJ:     $objPath"
Write-Host "Report:  $reportPath"
Write-Host "Logs:    $OutputDir"
