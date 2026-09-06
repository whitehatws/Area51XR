$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"

if (-not (Test-Path $build)) {
    & (Join-Path $PSScriptRoot "build.ps1")
}

ctest --test-dir $build -C RelWithDebInfo --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot "test-player-support.ps1")
