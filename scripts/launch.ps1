$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "build\RelWithDebInfo\area51xr.exe"

if (-not (Test-Path $exe)) {
    & (Join-Path $PSScriptRoot "build.ps1")
}

& $exe @args
