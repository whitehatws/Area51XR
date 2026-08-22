param(
    [Parameter(Mandatory = $true)]
    [string]$MameRoot,

    [string]$MsysRoot = "C:\msys64",

    [string]$OpenXrSdk = "",

    [string]$OnnxRuntimeDir = "",

    [string]$DepthModelPath = "",

    [string]$RomPath = "",

    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$bash = Join-Path $MsysRoot "usr\bin\bash.exe"
$ucrtBin = Join-Path $MsysRoot "ucrt64\bin"
$mameSource = Join-Path $MameRoot "src\mame\atari\jaguar.cpp"

if ([string]::IsNullOrWhiteSpace($OpenXrSdk)) {
    $OpenXrSdk = Join-Path $MsysRoot "ucrt64"
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

function Collect-BlockDiagnostics {
    $diagDir = $env:A51XR_ACCEPTANCE_DIR
    if ([string]::IsNullOrWhiteSpace($diagDir)) {
        $diagDir = Join-Path $root "logs"
    }
    $diagPath = Join-Path $diagDir "windows-block-diagnostics.txt"
    try {
        & (Join-Path $PSScriptRoot "collect-windows-block-diagnostics.ps1") `
            -BuildDir (Join-Path $root "build-mingw") `
            -OutputPath $diagPath
    }
    catch {
        Write-Host "Warning: failed to collect Windows block diagnostics: $_"
    }
}

function Get-MameMediaPath([string]$PrimaryRomPath) {
    if ([string]::IsNullOrWhiteSpace($PrimaryRomPath)) {
        return ""
    }

    $primary = [System.IO.Path]::GetFullPath($PrimaryRomPath)
    $paths = New-Object System.Collections.Generic.List[string]
    $paths.Add($primary)

    $mameHome = Split-Path -Parent $primary
    foreach ($candidate in @(
        (Join-Path $mameHome "chds"),
        (Join-Path $mameHome "chd")
    )) {
        if ((Test-Path $candidate) -and -not $paths.Contains($candidate)) {
            $paths.Add([System.IO.Path]::GetFullPath($candidate))
        }
    }

    return ($paths.ToArray() -join ';')
}

if (-not (Test-Path $bash)) {
    throw "MSYS2 bash not found at '$bash'."
}
if (-not (Test-Path $ucrtBin)) {
    throw "MSYS2 UCRT64 bin directory not found at '$ucrtBin'."
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
if (-not (Test-Path (Join-Path $OnnxRuntimeDir "runtimes\win-x64\native\onnxruntime.dll"))) {
    throw "ONNX Runtime DLL not found at '$OnnxRuntimeDir'."
}
if (-not (Test-Path $DepthModelPath)) {
    throw "Depth Anything V2 Small model not found at '$DepthModelPath'."
}
if (-not [string]::IsNullOrWhiteSpace($RomPath) -and -not (Test-Path $RomPath)) {
    throw "ROM path not found at '$RomPath'."
}

if (-not (($env:PATH -split ';') -contains $ucrtBin)) {
    $env:PATH = "$ucrtBin;$env:PATH"
}

if ($Jobs -le 0) {
    $Jobs = [Math]::Max(2, [Math]::Min(8, [Environment]::ProcessorCount))
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
$regressionExe = Join-Path $root "build-mingw\area51xr_tests.exe"
if (-not (Test-Path $regressionExe)) {
    throw "Area51XR regression executable was not built: $regressionExe"
}
$testExit = 1
try {
    & $regressionExe
    $testExit = $LASTEXITCODE
}
catch {
    Write-Host "Regression executable could not be launched: $_"
    $testExit = 1
}
if ($testExit -ne 0) {
    Collect-BlockDiagnostics
    throw "Area51XR tests failed with exit code $testExit."
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

Write-Host "[6/8] Staging packaged OpenXR loader DLL..."
$packagedLoader = Join-Path $ucrtBin "libopenxr_loader.dll"
if (-not (Test-Path $packagedLoader)) {
    throw "MSYS2 OpenXR loader not found at '$packagedLoader'."
}
$loaderDll = Join-Path $root "build-mingw\openxr_loader.dll"
Copy-Item $packagedLoader $loaderDll -Force

Write-Host "[7/8] Building targeted MAME CoJag subtarget with $Jobs jobs..."
$env:A51XR_MAME_JOBS = [string]$Jobs
$mameBuildCommand = @'
export OS=Windows_NT
export MSYSTEM=UCRT64
export MINGW_PREFIX=/ucrt64
export MINGW_CHOST=x86_64-w64-mingw32
export MINGW_PACKAGE_PREFIX=mingw-w64-ucrt-x86_64
export PATH=/ucrt64/bin:/usr/bin:$PATH
printf 'MAME toolchain: MSYSTEM=%s MINGW_PREFIX=%s\n' "$MSYSTEM" "$MINGW_PREFIX"
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

if (-not [string]::IsNullOrWhiteSpace($RomPath)) {
    $mameMediaPath = Get-MameMediaPath $RomPath
    Write-Host "MAME media path: $mameMediaPath"

    $romZip = Join-Path $RomPath "area51.zip"
    Write-Host "Area 51 ROM zip: $romZip = $(Test-Path $romZip)"
    foreach ($mediaRoot in ($mameMediaPath -split ';')) {
        $chdCandidate = Join-Path $mediaRoot "area51\area51.chd"
        Write-Host "Area 51 CHD candidate: $chdCandidate = $(Test-Path $chdCandidate)"
    }

    $diagDir = $env:A51XR_ACCEPTANCE_DIR
    if ([string]::IsNullOrWhiteSpace($diagDir)) {
        $diagDir = Join-Path $root "logs"
    }
    New-Item -ItemType Directory -Force -Path $diagDir | Out-Null
    $verifyOut = Join-Path $diagDir "mame-verify-area51.out.txt"
    $verifyErr = Join-Path $diagDir "mame-verify-area51.err.txt"

    Write-Host "Verifying Area 51 ROM/CHD set with MAME..."
    $verify = Start-Process -FilePath $mameExe `
        -ArgumentList @("-rompath", $mameMediaPath, "-verifyroms", "area51") `
        -WorkingDirectory (Split-Path -Parent $mameExe) `
        -PassThru -Wait -NoNewWindow `
        -RedirectStandardOutput $verifyOut -RedirectStandardError $verifyErr

    if (Test-Path $verifyOut) { Get-Content $verifyOut | Write-Host }
    if (Test-Path $verifyErr) { Get-Content $verifyErr | Write-Host }

    if ($verify.ExitCode -ne 0) {
        throw "MAME could not verify Area 51 using media path '$mameMediaPath'. Full audit is saved in the acceptance diagnostics. The current MAME parent set requires area51.zip plus the Area 51 CHD (normally area51\area51.chd)."
    }

    $env:A51XR_MAME_MEDIA_PATH = $mameMediaPath
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
Write-Host "OpenXR loader: $loaderDll"
Write-Host "ONNX Runtime:  $ortDll"
