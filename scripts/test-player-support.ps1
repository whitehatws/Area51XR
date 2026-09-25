$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$module = Join-Path $root "release\Support\Area51XR.PlayerSupport.psm1"
Import-Module $module -Force

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("Area51XR-player-support-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
try {
    $legacy = Join-Path $testRoot "legacy"
    $legacyNvram = Join-Path $legacy "nvram\area51"
    New-Item -ItemType Directory -Force -Path $legacyNvram | Out-Null
    $scoreFile = Join-Path $legacyNvram "area51.nv"
    [System.IO.File]::WriteAllBytes($scoreFile,[byte[]](1,2,3,4,5,6,7,8))
    $dataRoot = Join-Path $testRoot "data"
    $existingRoot = Join-Path $dataRoot "mame\nvram\area51"
    New-Item -ItemType Directory -Force -Path $existingRoot | Out-Null
    [System.IO.File]::WriteAllBytes((Join-Path $existingRoot "existing.nv"),[byte[]](9,8,7))
    $data = Initialize-Area51XRPlayerData -LegacyRoots @($legacy) -DataRoot $dataRoot
    $migratedScore = Join-Path $data.Nvram "area51\area51.nv"
    if (-not (Test-Path $migratedScore)) { throw "NVRAM migration regression" }
    $backedUpCurrent = @(Get-ChildItem -Path $data.Backups -Filter "existing.nv" -File -Recurse)
    if ($backedUpCurrent.Count -ne 1) { throw "Existing persistent-data backup regression" }
    $before = (Get-FileHash $migratedScore -Algorithm SHA256).Hash

    # A restart and a second process launch reuse the same stable directories.
    $again = Initialize-Area51XRPlayerData -LegacyRoots @($legacy) -DataRoot $data.Root
    $after = (Get-FileHash (Join-Path $again.Nvram "area51\area51.nv") -Algorithm SHA256).Hash
    if ($before -ne $after) { throw "Persistent score data changed across relaunch simulation" }

    $media = Join-Path $testRoot "renamed-media"
    New-Item -ItemType Directory -Force -Path $media | Out-Null
    $manifest = New-Object System.Collections.Generic.List[object]
    foreach ($item in @(
        @{ Name="one.rom"; Bytes=[byte[]](1,1,1) },
        @{ Name="two.rom"; Bytes=[byte[]](2,2,2,2) }
    )) {
        $path = Join-Path $media ([Guid]::NewGuid().ToString("N") + ".bin")
        [System.IO.File]::WriteAllBytes($path,$item.Bytes)
        $hash = (Get-FileHash $path -Algorithm SHA1).Hash.ToLowerInvariant()
        $manifest.Add([pscustomobject]@{ Name=$item.Name; Kind="rom"; Size=[long]$item.Bytes.Length; Sha1=$hash })
    }
    $fakeChd = Join-Path $media "mystery.dat"
    $header = New-Object byte[] 124
    [System.Text.Encoding]::ASCII.GetBytes("MComprHD").CopyTo($header,0)
    $header[15] = 5
    $chdSha = [byte[]](0..19)
    $chdSha.CopyTo($header,84)
    [System.IO.File]::WriteAllBytes($fakeChd,$header)
    $manifest.Add([pscustomobject]@{ Name="area51.chd"; Kind="chd"; Size=-1L; Sha1=(ConvertTo-LowerHex $chdSha) })

    $resolved = Resolve-Area51Media -SearchRoot $media -DataRoot $data.Root -Manifest $manifest.ToArray()
    foreach ($component in $manifest) {
        if (-not (Test-Path (Join-Path $resolved "area51\$($component.Name)"))) {
            throw "Renamed-media staging regression for $($component.Name)"
        }
    }

    $canonical = Join-Path $testRoot "canonical"
    New-Item -ItemType Directory -Force -Path (Join-Path $canonical "area51") | Out-Null
    New-Item -ItemType File -Force -Path (Join-Path $canonical "area51.zip") | Out-Null
    New-Item -ItemType File -Force -Path (Join-Path $canonical "area51\area51.chd") | Out-Null
    if (-not (Test-Area51CanonicalMedia -MediaPath $canonical)) { throw "Canonical-layout regression" }

    Remove-Item -LiteralPath (Join-Path $media "mystery.dat") -Force
    try {
        Resolve-Area51Media -SearchRoot $media -DataRoot $data.Root -Manifest $manifest.ToArray() | Out-Null
        throw "Missing media was accepted"
    }
    catch {
        if ($_ -match "Missing media was accepted") { throw }
    }

    [System.IO.File]::WriteAllBytes((Join-Path $media "duplicate.bin"),[byte[]](1,1,1))
    try {
        Resolve-Area51Media -SearchRoot $media -DataRoot $data.Root -Manifest @($manifest[0]) | Out-Null
        throw "Ambiguous media was accepted"
    }
    catch {
        if ($_ -match "Ambiguous media was accepted") { throw }
    }

    Write-Host "Area51XR player-support regressions passed."
}
finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
