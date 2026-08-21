param(
    [Parameter(Mandatory = $true)]
    [string]$MameRoot,

    [string]$MsysRoot = "C:\msys64",

    [string]$OpenXrSdk = "",

    [string]$OnnxRuntimeDir = "",

    [string]$DepthModelPath = "",

    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$bash = Join-Path $MsysRoot "usr\bin\bash.exe"
$mameSource = Join-Path $MameRoot "src\mame\atari\jaguar.cpp"

if ([string]::IsNullOrWhiteSpace($OpenXrSdk)) {
    $OpenXrSdk = Join-Path $root "external\openxr-sdk"
}
if ([string]::IsNullOrWhiteSpace($OnnxRuntimeDir)) {
    $OnnxRuntimeDir = Join-Path $root "external\onnxruntime-1.28.0"
}
if ([string]::IsNullOrWhiteSpace($DepthModelPath)) {
    $DepthModelPath = Join-Path $root "models\depth_anything_v2_vits.onnx"
}

function ConvertTo-MsysPath([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $matches[1].ToLowerInvariant()
        $tail = $matches[2] -replace '\\', '/'
        return "/$drive/$tail"
    }
    return ($full -replace '\\', '/')
}

if (-not (Test-Path $bash)) {
    throw "MSYS2 bash not found at '$bash'."
}
if (-not (Test-Path $mameSource)) {
    throw "MAME source tree not found at '$MameRoot'."
}
if (-not (Test-Path (Join-Path $OpenXrSdk "include\openxr\openxr.h"))) {
    throw "OpenXR SDK headers not found at '$OpenXrSdk'."
}
if (-not (Test-Path (Join-Path $OnnxRuntimeDir "build\native\include\onnxruntime_cxx_api.h"))) {
    throw "ONNX Runtime C++ headers not found at '$OnnxRuntimeDir'."
}
if (-not (Test-Path $DepthModelPath)) {
    throw "Depth Anything V2 Small model not found at '$DepthModelPath'."
}

if ($Jobs -le 0) {
    $Jobs = [Math]::Max(2, [Environment]::ProcessorCount)
}

Write-Host "[1/8] Applying Area51XR MAME integration..."
& (Join-Path $PSScriptRoot "apply-mame-patch.ps1") -MameRoot $MameRoot

$env:A51XR_ROOT_MSYS = ConvertTo-MsysPath $root
$env:A51XR_MAME_ROOT_MSYS = ConvertTo-MsysPath $MameRoot
$env:A51XR_OPENXR_SDK_MSYS = ConvertTo-MsysPath $OpenXrSdk
$env:A51XR_ONNXRUNTIME_DIR_MSYS = ConvertTo-MsysPath $OnnxRuntimeDir

Write-Host "[2/8] Building Area51XR with MSYS2/UCRT, OpenXR, and ONNX Runtime..."
$hostBuildCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
cmake -S "$A51XR_ROOT_MSYS" -B "$A51XR_ROOT_MSYS/build-mingw" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DA51XR_OPENXR_SDK="$A51XR_OPENXR_SDK_MSYS" -DA51XR_ONNXRUNTIME_DIR="$A51XR_ONNXRUNTIME_DIR_MSYS"
cmake --build "$A51XR_ROOT_MSYS/build-mingw" --parallel
'@
& $bash -lc $hostBuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "Area51XR build failed with exit code $LASTEXITCODE."
}

Write-Host "[3/8] Running Area51XR regression tests..."
$hostTestCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
ctest --test-dir "$A51XR_ROOT_MSYS/build-mingw" --output-on-failure --repeat until-pass:2
'@
& $bash -lc $hostTestCommand
if ($LASTEXITCODE -ne 0) {
    throw "Area51XR tests failed with exit code $LASTEXITCODE."
}

Write-Host "[4/8] Running synthetic reconstruction self-test..."
& (Join-Path $PSScriptRoot "reconstruction-selftest.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "Synthetic reconstruction self-test failed with exit code $LASTEXITCODE."
}

Write-Host "[5/8] Running Depth Anything V2 ONNX self-test..."
$depthSelfTest = Join-Path $root "build-mingw\area51xr-depth-selftest.exe"
if (-not (Test-Path $depthSelfTest)) {
    throw "Depth model self-test executable was not built."
}
& $depthSelfTest $DepthModelPath
if ($LASTEXITCODE -ne 0) {
    throw "Depth model self-test failed with exit code $LASTEXITCODE."
}

Write-Host "[6/8] Building Khronos OpenXR loader DLL..."
$loaderBuildCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
cmake -S "$A51XR_OPENXR_SDK_MSYS" -B "$A51XR_ROOT_MSYS/external/openxr-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DDYNAMIC_LOADER=ON -DBUILD_TESTING=OFF
cmake --build "$A51XR_ROOT_MSYS/external/openxr-build" --target openxr_loader --parallel
'@
& $bash -lc $loaderBuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "OpenXR loader build failed with exit code $LASTEXITCODE."
}

$loaderDll = Get-ChildItem -Path (Join-Path $root "external\openxr-build") -Filter "openxr_loader.dll" -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $loaderDll) {
    throw "OpenXR loader build completed but openxr_loader.dll was not found."
}
Copy-Item $loaderDll (Join-Path $root "build-mingw\openxr_loader.dll") -Force

Write-Host "[7/8] Building targeted MAME CoJag subtarget..."
$env:A51XR_MAME_JOBS = [string]$Jobs
$mameBuildCommand = @'
export PATH=/ucrt64/bin:/usr/bin:$PATH
cd "$A51XR_MAME_ROOT_MSYS"
make SUBTARGET=area51xr SOURCES=src/mame/atari/jaguar.cpp REGENIE=1 -j"$A51XR_MAME_JOBS"
'@
& $bash -lc $mameBuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "MAME build failed with exit code $LASTEXITCODE."
}

Write-Host "[8/8] Locating and validating the MAME executable..."
$candidates = @(
    (Join-Path $MameRoot "mamearea51xr.exe"),
    (Join-Path $MameRoot "area51xr.exe")
)
$mameExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $mameExe) {
    $mameExe = Get-ChildItem -Path $MameRoot -Filter "*area51xr*.exe" -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $mameExe) {
    throw "MAME build completed but the Area51XR subtarget executable was not found."
}

& $mameExe -listfull area51 | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "The patched MAME executable could not enumerate Area 51."
}

$hostExe = Join-Path $root "build-mingw\area51xr.exe"
if (-not (Test-Path $hostExe)) {
    throw "Area51XR host was not found at '$hostExe'."
}
$ortDll = Join-Path $root "build-mingw\onnxruntime.dll"
if (-not (Test-Path $ortDll)) {
    throw "Area51XR build completed without the expected onnxruntime.dll."
}

Write-Host ""
Write-Host "Smoke test passed."
Write-Host "Area51XR host:  $hostExe"
Write-Host "Patched MAME:  $mameExe"
Write-Host "Depth model:   $DepthModelPath"
Write-Host "OpenXR loader: $(Join-Path $root 'build-mingw\openxr_loader.dll')"
Write-Host "ONNX Runtime:  $ortDll"
