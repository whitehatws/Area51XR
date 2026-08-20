$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"

cmake -S $root -B $build -A x64
cmake --build $build --config RelWithDebInfo
