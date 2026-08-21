param(
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $root "logs\reconstruction-selftest-$stamp"
}

$generatorCandidates = @(
    (Join-Path $root "build-mingw\area51xr-generate-capture.exe"),
    (Join-Path $root "build\RelWithDebInfo\area51xr-generate-capture.exe"),
    (Join-Path $root "build\area51xr-generate-capture.exe")
)
$generator = $generatorCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $generator) {
    throw "area51xr-generate-capture was not found. Run the bootstrap/build first."
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$capture = Join-Path $OutputDir "synthetic.a51cap"
$obj = Join-Path $OutputDir "synthetic.obj"
$report = Join-Path $OutputDir "synthetic.quality.json"
$generateLog = Join-Path $OutputDir "generate.log"
$generateErr = Join-Path $OutputDir "generate.err.log"

$generated = Start-Process -FilePath $generator -ArgumentList @($capture) `
    -PassThru -Wait -NoNewWindow `
    -RedirectStandardOutput $generateLog -RedirectStandardError $generateErr
if ($generated.ExitCode -ne 0) {
    $err = if (Test-Path $generateErr) { Get-Content -Raw $generateErr } else { "" }
    throw "Synthetic capture generation failed with exit code $($generated.ExitCode). $err Logs: $OutputDir"
}

& (Join-Path $PSScriptRoot "replay-capture.ps1") -CapturePath $capture -OutputDir $OutputDir

if (-not (Test-Path $obj) -or -not (Test-Path $report)) {
    throw "Reconstruction self-test did not produce expected OBJ/report files. Logs: $OutputDir"
}

$quality = Get-Content -Raw -Path $report | ConvertFrom-Json
if (-not $quality.passed) {
    throw "Reconstruction self-test failed quality gate: $($quality.reason). Logs: $OutputDir"
}

Write-Host "Reconstruction self-test passed."
Write-Host "Capture: $capture"
Write-Host "OBJ:     $obj"
Write-Host "Report:  $report"
Write-Host "Logs:    $OutputDir"
