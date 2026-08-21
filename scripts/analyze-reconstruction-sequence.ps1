param(
    [Parameter(Mandatory = $true)]
    [string]$ManifestPath,

    [float]$MinValidDepthRatio = 0.60,

    [float]$MinMeshTriangleRatio = 0.20,

    [float]$MaxMeshTriangleRatioSwing = 0.25
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ManifestPath)) {
    throw "Manifest not found: $ManifestPath"
}

$entries = @()
Get-Content -Path $ManifestPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object {
    $entries += ($_ | ConvertFrom-Json)
}
if ($entries.Count -eq 0) {
    throw "Manifest contains no reconstruction entries."
}

$minValid = 1.0
$minMesh = 1.0
$maxMesh = 0.0
$failed = @()

foreach ($entry in $entries) {
    if (-not (Test-Path $entry.quality)) {
        throw "Quality report not found: $($entry.quality)"
    }
    $quality = Get-Content -Raw -Path $entry.quality | ConvertFrom-Json
    $validDepth = [double]$quality.reconstruction.valid_depth_ratio
    $meshRatio = [double]$quality.reconstruction.mesh_triangle_ratio

    if (-not $quality.passed) {
        $failed += "frame $($entry.index): $($quality.reason)"
    }
    if ($validDepth -lt $MinValidDepthRatio) {
        $failed += "frame $($entry.index): valid depth ratio $validDepth below $MinValidDepthRatio"
    }
    if ($meshRatio -lt $MinMeshTriangleRatio) {
        $failed += "frame $($entry.index): mesh ratio $meshRatio below $MinMeshTriangleRatio"
    }

    if ($validDepth -lt $minValid) { $minValid = $validDepth }
    if ($meshRatio -lt $minMesh) { $minMesh = $meshRatio }
    if ($meshRatio -gt $maxMesh) { $maxMesh = $meshRatio }
}

$meshSwing = $maxMesh - $minMesh
if ($meshSwing -gt $MaxMeshTriangleRatioSwing) {
    $failed += "mesh coverage swing $meshSwing exceeds $MaxMeshTriangleRatioSwing"
}

$resultPath = Join-Path (Split-Path -Parent $ManifestPath) "sequence-quality.txt"
if ($failed.Count -gt 0) {
    $body = @(
        "RECONSTRUCTION SEQUENCE: FAIL",
        "Frames: $($entries.Count)",
        "Min valid depth ratio: $minValid",
        "Min mesh ratio: $minMesh",
        "Max mesh ratio: $maxMesh",
        "Mesh ratio swing: $meshSwing",
        "Failures:",
        ($failed -join [Environment]::NewLine)
    ) -join [Environment]::NewLine
    Set-Content -Path $resultPath -Value $body
    Write-Error $body
    exit 2
}

$body = @(
    "RECONSTRUCTION SEQUENCE: PASS",
    "Frames: $($entries.Count)",
    "Min valid depth ratio: $minValid",
    "Min mesh ratio: $minMesh",
    "Max mesh ratio: $maxMesh",
    "Mesh ratio swing: $meshSwing"
) -join [Environment]::NewLine
Set-Content -Path $resultPath -Value $body
Write-Host $body
exit 0
