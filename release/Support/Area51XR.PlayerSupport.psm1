$ErrorActionPreference = "Stop"

function Get-Area51XRDataRoot {
    if (-not [string]::IsNullOrWhiteSpace($env:A51XR_DATA_ROOT)) {
        return [System.IO.Path]::GetFullPath($env:A51XR_DATA_ROOT)
    }
    $local = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    if ([string]::IsNullOrWhiteSpace($local)) {
        throw "Windows Local AppData is unavailable. Area51XR cannot safely store player data."
    }
    return (Join-Path $local "Area51XR")
}

function Initialize-Area51XRPlayerData {
    param(
        [string[]]$LegacyRoots = @(),
        [string]$DataRoot = ""
    )

    if ([string]::IsNullOrWhiteSpace($DataRoot)) { $DataRoot = Get-Area51XRDataRoot }
    $DataRoot = [System.IO.Path]::GetFullPath($DataRoot)
    $paths = [ordered]@{
        Root = $DataRoot
        Nvram = Join-Path $DataRoot "mame\nvram"
        Diff = Join-Path $DataRoot "mame\diff"
        Cfg = Join-Path $DataRoot "mame\cfg"
        Input = Join-Path $DataRoot "mame\input"
        MediaStage = Join-Path $DataRoot "media-staging-v1"
        Backups = Join-Path $DataRoot "Backups"
    }
    foreach ($directory in @($paths.Root,$paths.Nvram,$paths.Diff,$paths.Cfg,$paths.Input,$paths.Backups)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }

    $migrationMarker = Join-Path $DataRoot "migration-v1.complete"
    if (-not (Test-Path $migrationMarker)) {
        $sources = New-Object System.Collections.Generic.List[object]
        foreach ($legacyRoot in $LegacyRoots) {
            if ([string]::IsNullOrWhiteSpace($legacyRoot)) { continue }
            $fullLegacy = [System.IO.Path]::GetFullPath($legacyRoot)
            foreach ($mapping in @(
                @{ Name = "nvram"; Destination = $paths.Nvram },
                @{ Name = "diff"; Destination = $paths.Diff },
                @{ Name = "cfg"; Destination = $paths.Cfg },
                @{ Name = "input"; Destination = $paths.Input }
            )) {
                $source = Join-Path $fullLegacy $mapping.Name
                if (Test-Path $source) {
                    $files = @(Get-ChildItem -Path $source -File -Recurse -ErrorAction SilentlyContinue)
                    if ($files.Count -gt 0) {
                        $sources.Add([pscustomobject]@{ Source=$source; Destination=$mapping.Destination; Files=$files })
                    }
                }
            }
        }

        if ($sources.Count -gt 0) {
            $backup = Join-Path $paths.Backups ("v1-migration-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
            New-Item -ItemType Directory -Force -Path $backup | Out-Null
            $currentMame = Join-Path $DataRoot "mame"
            $currentFiles = @(Get-ChildItem -Path $currentMame -File -Recurse -ErrorAction SilentlyContinue)
            if ($currentFiles.Count -gt 0) {
                Copy-Item -Path $currentMame -Destination (Join-Path $backup "current-mame") -Recurse -Force
            }
            $sourceIndex = 0
            foreach ($entry in $sources) {
                $sourceIndex++
                $backupSource = Join-Path $backup ("source-$sourceIndex")
                Copy-Item -Path $entry.Source -Destination $backupSource -Recurse -Force
                foreach ($file in $entry.Files) {
                    $relative = $file.FullName.Substring($entry.Source.Length).TrimStart('\','/')
                    $destination = Join-Path $entry.Destination $relative
                    if (-not (Test-Path $destination)) {
                        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
                        Copy-Item -LiteralPath $file.FullName -Destination $destination
                    }
                }
            }
            Write-Host "Existing Area51XR cabinet data was backed up and migrated."
        }
        "Area51XR persistent-data migration v1 completed $(Get-Date -Format o)" |
            Set-Content -Path $migrationMarker -Encoding UTF8
    }

    Write-Host "Persistent scores and preferences: %LOCALAPPDATA%\Area51XR"
    return [pscustomobject]$paths
}

function Get-Area51MediaManifest {
    return @(
        [pscustomobject]@{ Name="2-c_area_51_hh.hh"; Kind="rom"; Size=524288L; Sha1="69da54ed6886e825156bbcc256e8d7abd4dc1ff8" },
        [pscustomobject]@{ Name="2-c_area_51_hl.hl"; Kind="rom"; Size=524288L; Sha1="9b4945bc04f8a73161638a2c5fa2fd84c6fd31b4" },
        [pscustomobject]@{ Name="2-c_area_51_lh.lh"; Kind="rom"; Size=524288L; Sha1="ae377a6803a4f7d1bbcc111725af121a3e82317d" },
        [pscustomobject]@{ Name="2-c_area_51_ll.ll"; Kind="rom"; Size=524288L; Sha1="4b5f45ee140b03a6be61475cae1c2dbef0f07457" },
        [pscustomobject]@{ Name="jagwave.rom"; Kind="rom"; Size=4096L; Sha1="58117e11fd6478c521fbd3fdbe157f39567552f0" },
        [pscustomobject]@{ Name="area51.chd"; Kind="chd"; Size=-1L; Sha1="3b303bc37e206a6d7339352c869f050d04186f11" }
    )
}

function ConvertTo-LowerHex([byte[]]$Bytes) {
    return (($Bytes | ForEach-Object { $_.ToString("x2") }) -join '')
}

function Get-ChdLogicalSha1 {
    param([Parameter(Mandatory=$true)][string]$Path)
    $stream = [System.IO.File]::Open($Path,[System.IO.FileMode]::Open,[System.IO.FileAccess]::Read,[System.IO.FileShare]::Read)
    try {
        if ($stream.Length -lt 124) { return $null }
        $header = New-Object byte[] 124
        if ($stream.Read($header,0,$header.Length) -ne $header.Length) { return $null }
        $magic = [System.Text.Encoding]::ASCII.GetString($header,0,8)
        if ($magic -ne "MComprHD") { return $null }
        $version = ([int]$header[12] -shl 24) -bor ([int]$header[13] -shl 16) -bor ([int]$header[14] -shl 8) -bor [int]$header[15]
        $offset = switch ($version) { 3 {80} 4 {48} 5 {84} default {-1} }
        if ($offset -lt 0) { return $null }
        $sha = New-Object byte[] 20
        [Array]::Copy($header,$offset,$sha,0,20)
        return ConvertTo-LowerHex $sha
    }
    finally { $stream.Dispose() }
}

function Test-Area51CanonicalMedia {
    param([Parameter(Mandatory=$true)][string]$MediaPath)
    $roots = @($MediaPath -split ';') | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    $romFound = $false
    $chdFound = $false
    foreach ($root in $roots) {
        if ((Test-Path (Join-Path $root "area51.zip")) -or
            (Test-Path (Join-Path $root "area51\2-c_area_51_hh.hh"))) { $romFound = $true }
        if (Test-Path (Join-Path $root "area51\area51.chd")) { $chdFound = $true }
    }
    return $romFound -and $chdFound
}

function Get-Area51MediaCandidates {
    param(
        [Parameter(Mandatory=$true)][string]$SearchRoot,
        [object[]]$Manifest = (Get-Area51MediaManifest)
    )
    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
    $byHash = @{}
    foreach ($component in $Manifest) { $byHash[$component.Sha1.ToLowerInvariant()] = $component }
    $found = @{}
    foreach ($component in $Manifest) { $found[$component.Name] = New-Object System.Collections.Generic.List[object] }

    foreach ($file in @(Get-ChildItem -LiteralPath $SearchRoot -File -Recurse -ErrorAction SilentlyContinue)) {
        if ($file.Extension -ieq '.zip') {
            try {
                $zip = [System.IO.Compression.ZipFile]::OpenRead($file.FullName)
                try {
                    foreach ($entry in $zip.Entries) {
                        if ($entry.Length -le 0) { continue }
                        $possible = @($Manifest | Where-Object { $_.Kind -eq 'rom' -and $_.Size -eq $entry.Length })
                        if ($possible.Count -eq 0) { continue }
                        $stream = $entry.Open()
                        try {
                            $sha = [System.Security.Cryptography.SHA1]::Create()
                            try { $hash = ConvertTo-LowerHex ($sha.ComputeHash($stream)) }
                            finally { $sha.Dispose() }
                        }
                        finally { $stream.Dispose() }
                        if ($byHash.ContainsKey($hash)) {
                            $component = $byHash[$hash]
                            $found[$component.Name].Add([pscustomobject]@{
                                Component=$component; Kind="zip"; Source=$file.FullName; Entry=$entry.FullName
                            })
                        }
                    }
                }
                finally { $zip.Dispose() }
            }
            catch { Write-Verbose "Could not inspect archive '$($file.FullName)': $_" }
            continue
        }

        $sizeMatches = @($Manifest | Where-Object { $_.Kind -eq 'rom' -and $_.Size -eq $file.Length })
        if ($sizeMatches.Count -gt 0) {
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA1).Hash.ToLowerInvariant()
            if ($byHash.ContainsKey($hash)) {
                $component = $byHash[$hash]
                $found[$component.Name].Add([pscustomobject]@{
                    Component=$component; Kind="file"; Source=$file.FullName; Entry=""
                })
            }
        }

        if ($file.Length -ge 124) {
            $chdHash = Get-ChdLogicalSha1 -Path $file.FullName
            if ($chdHash -and $byHash.ContainsKey($chdHash)) {
                $component = $byHash[$chdHash]
                if ($component.Kind -eq 'chd') {
                    $found[$component.Name].Add([pscustomobject]@{
                        Component=$component; Kind="file"; Source=$file.FullName; Entry=""
                    })
                }
            }
        }
    }
    return $found
}

function Resolve-Area51Media {
    param(
        [Parameter(Mandatory=$true)][string]$SearchRoot,
        [Parameter(Mandatory=$true)][string]$DataRoot,
        [object[]]$Manifest = (Get-Area51MediaManifest)
    )
    $SearchRoot = [System.IO.Path]::GetFullPath($SearchRoot)
    if (Test-Area51CanonicalMedia -MediaPath $SearchRoot) { return $SearchRoot }

    $found = Get-Area51MediaCandidates -SearchRoot $SearchRoot -Manifest $Manifest
    $missing = @($Manifest | Where-Object { $found[$_.Name].Count -eq 0 } | ForEach-Object { $_.Name })
    if ($missing.Count -gt 0) {
        throw "Area 51 media discovery could not identify: $($missing -join ', '). Files are accepted only by verified content signature."
    }
    $ambiguous = @($Manifest | Where-Object { $found[$_.Name].Count -gt 1 } | ForEach-Object { $_.Name })
    if ($ambiguous.Count -gt 0) {
        throw "Area 51 media discovery is ambiguous for: $($ambiguous -join ', '). Keep one verified copy of each component in the selected folder."
    }

    $stageRoot = Join-Path $DataRoot "media-staging-v1"
    if (Test-Area51CanonicalMedia -MediaPath $stageRoot) {
        return $stageRoot
    }
    $temporaryRoot = Join-Path $DataRoot ("media-staging-" + [Guid]::NewGuid().ToString("N"))
    $gameStage = Join-Path $temporaryRoot "area51"
    New-Item -ItemType Directory -Force -Path $gameStage | Out-Null
    try {
        foreach ($component in $Manifest) {
            $candidate = $found[$component.Name][0]
            $destination = Join-Path $gameStage $component.Name
            if ($candidate.Kind -eq "zip") {
                $zip = [System.IO.Compression.ZipFile]::OpenRead($candidate.Source)
                try {
                    $entry = $zip.GetEntry($candidate.Entry)
                    if (-not $entry) { throw "Archive entry disappeared during staging: $($candidate.Entry)" }
                    $input = $entry.Open()
                    $output = [System.IO.File]::Create($destination)
                    try { $input.CopyTo($output) }
                    finally { $output.Dispose(); $input.Dispose() }
                }
                finally { $zip.Dispose() }
            }
            else { Copy-Item -LiteralPath $candidate.Source -Destination $destination }
        }
        if (Test-Path $stageRoot) {
            $old = Join-Path $DataRoot ("media-staging-previous-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
            Move-Item -LiteralPath $stageRoot -Destination $old
        }
        Move-Item -LiteralPath $temporaryRoot -Destination $stageRoot
    }
    catch {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
        throw
    }
    Write-Host "Verified renamed Area 51 media was staged under the per-user Area51XR data folder."
    return $stageRoot
}

Export-ModuleMember -Function Get-Area51XRDataRoot,Initialize-Area51XRPlayerData,Get-Area51MediaManifest,ConvertTo-LowerHex,Get-ChdLogicalSha1,Test-Area51CanonicalMedia,Get-Area51MediaCandidates,Resolve-Area51Media
