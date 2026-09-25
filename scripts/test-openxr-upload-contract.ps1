$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root "src\xr\openxr_runtime_win32.cpp"
$source = [System.IO.File]::ReadAllText($sourcePath)

$update = $source.IndexOf("p.context->UpdateSubresource(", [System.StringComparison]::Ordinal)
$flush = $source.IndexOf("p.context->Flush();", [System.StringComparison]::Ordinal)
$release = $source.IndexOf("p.xrReleaseSwapchainImage(", [System.StringComparison]::Ordinal)

if ($update -lt 0) { throw "OpenXR D3D11 upload call was not found." }
if ($flush -lt 0) { throw "OpenXR D3D11 upload is missing its explicit submission flush." }
if ($release -lt 0) { throw "OpenXR swapchain release call was not found." }
if (-not ($update -lt $flush -and $flush -lt $release)) {
    throw "OpenXR upload contract regression: expected UpdateSubresource, Flush, then xrReleaseSwapchainImage."
}
if ($source.IndexOf("sync=flush", [System.StringComparison]::Ordinal) -lt 0) {
    throw "OpenXR upload diagnostics no longer report the submission policy."
}

Write-Host "OpenXR D3D11 upload contract passed."
