[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$WheelPath
)

$ErrorActionPreference = "Stop"
$ExpectedWheelName = "droplet_trainer-0.2.0-py3-none-any.whl"
$ExpectedVersion = "0.2.0"
$ExpectedHash = "20e070bf0e5a114bf9daadaec22ff81a704c754d8077a7cea7af93d9dddab796"
$ExpectedSize = 63123

function Get-LockEntries {
    param([string]$Path)
    $entries = @()
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -notmatch "==") {
            continue
        }
        if ($line -notmatch
            "^\s*([A-Za-z0-9_.-]+)==([^\s]+)\s+--hash=sha256:([0-9a-fA-F]{64})\s*$") {
            throw "Unsupported authoritative lock line: $line"
        }
        $entries += [pscustomobject]@{
            name = $Matches[1]
            version = $Matches[2]
            sha256 = $Matches[3].ToLowerInvariant()
        }
    }
    if ($entries.Count -ne 37) {
        throw "Authoritative lock must contain exactly 37 entries; got $($entries.Count)."
    }
    return $entries
}

function Assert-Authority {
    param([string]$LockPath, [string]$InventoryPath)
    $entries = @(Get-LockEntries -Path $LockPath)
    $trainerEntries = @($entries | Where-Object {
        $_.name -eq "droplet-trainer" -and $_.version -eq $ExpectedVersion
    })
    if ($trainerEntries.Count -ne 1 -or $trainerEntries[0].sha256 -ne $ExpectedHash) {
        throw "The authoritative lock does not contain the accepted droplet-trainer wheel."
    }

    $inventory = Get-Content -LiteralPath $InventoryPath -Raw | ConvertFrom-Json
    if ([string]$inventory.schema_version -ne "opendss-training-runtime-inventory-v1" -or
        [int]$inventory.runtime.distribution_count -ne 37 -or
        [string]$inventory.runtime.trainer_version -ne $ExpectedVersion -or
        [string]$inventory.distributions."droplet-trainer" -ne $ExpectedVersion) {
        throw "The authoritative training inventory is not accepted."
    }
    $expected = @{}
    foreach ($property in $inventory.distributions.PSObject.Properties) {
        $expected[$property.Name.ToLowerInvariant()] = [string]$property.Value
    }
    if ($expected.Count -ne 37) {
        throw "The authoritative inventory must contain exactly 37 distributions."
    }
    foreach ($entry in $entries) {
        $name = ([string]$entry.name).ToLowerInvariant()
        if (-not $expected.ContainsKey($name) -or $expected[$name] -ne [string]$entry.version) {
            throw "The lock and inventory differ for $($entry.name)."
        }
    }
    return $inventory
}

function Assert-Wheel {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "The accepted local trainer wheel is missing: $Path"
    }
    if ([System.IO.Path]::GetFileName($Path) -cne $ExpectedWheelName) {
        throw "Trainer wheel filename must be exactly $ExpectedWheelName."
    }
    $file = Get-Item -LiteralPath $Path
    if ($file.Length -ne $ExpectedSize) {
        throw "Trainer wheel size mismatch. Expected $ExpectedSize bytes; got $($file.Length)."
    }
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
    if ($hash -ne $ExpectedHash) {
        throw "Trainer wheel hash mismatch. Expected $ExpectedHash; got $hash."
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $metadataEntries = @($archive.Entries | Where-Object {
            $_.FullName -ceq "droplet_trainer-0.2.0.dist-info/METADATA"
        })
        if ($metadataEntries.Count -ne 1) {
            throw "Trainer wheel does not contain the exact 0.2.0 dist-info metadata."
        }
        $reader = [System.IO.StreamReader]::new($metadataEntries[0].Open())
        try {
            $metadata = $reader.ReadToEnd()
        } finally {
            $reader.Dispose()
        }
        if ($metadata -notmatch "(?mi)^Name:\s*droplet-trainer\s*$" -or
            $metadata -notmatch "(?mi)^Version:\s*0[.]2[.]0\s*$") {
            throw "Trainer wheel metadata name/version is not droplet-trainer 0.2.0."
        }
    } finally {
        $archive.Dispose()
    }
}

function Get-CanonicalItem {
    param([string]$Path, [string]$Description)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $pathRoot = [System.IO.Path]::GetPathRoot($fullPath)
    $rootItem = Get-Item -LiteralPath $pathRoot -Force
    if (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Description path root is a reparse point or symlink: $($rootItem.FullName)"
    }
    $cursor = $rootItem.FullName
    foreach ($segment in $fullPath.Substring($pathRoot.Length).Split(
        [char[]]@("\", "/"), [System.StringSplitOptions]::RemoveEmptyEntries)) {
        $cursor = Join-Path $cursor $segment
        if (-not (Test-Path -LiteralPath $cursor)) {
            throw "$Description is missing: $cursor"
        }
        $item = Get-Item -LiteralPath $cursor -Force
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Description contains a reparse point or symlink: $($item.FullName)"
        }
        $cursor = $item.FullName
    }
    $finalItem = Get-Item -LiteralPath $cursor -Force
    return $finalItem
}

function Assert-CanonicalDescendant {
    param([object]$Item, [object]$Root, [string]$Description)
    $rootPath = $Root.FullName.TrimEnd("\")
    $cursor = if ($Item.PSIsContainer) { $Item } else { $Item.Directory }
    while ($null -ne $cursor) {
        if ($cursor.FullName.TrimEnd("\") -ieq $rootPath) {
            if ($Item.FullName.TrimEnd("\") -ieq $rootPath) {
                throw "$Description must be a child of, not equal to, $rootPath."
            }
            return
        }
        $cursor = $cursor.Parent
    }
    throw "$Description is not canonically contained by $rootPath`: $($Item.FullName)"
}

function Assert-NoReparseTree {
    param([object]$Item, [string]$Description)
    if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Description is a reparse point or symlink: $($Item.FullName)"
    }
    if (-not $Item.PSIsContainer) {
        return
    }
    foreach ($child in @(Get-ChildItem -LiteralPath $Item.FullName -Force -Recurse)) {
        if (($child.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Description contains a reparse point or symlink: $($child.FullName)"
        }
    }
}

function Get-EnvironmentRoots {
    param([string]$EnvironmentRoot)
    $environment = Get-CanonicalItem -Path $EnvironmentRoot -Description "Training environment"
    if (-not $environment.PSIsContainer) {
        throw "Training environment is not a directory: $($environment.FullName)"
    }
    $lib = Get-CanonicalItem -Path (Join-Path $environment.FullName "Lib") `
        -Description "Training environment Lib directory"
    $sitePackages = Get-CanonicalItem -Path (Join-Path $lib.FullName "site-packages") `
        -Description "Training environment site-packages directory"
    $scripts = Get-CanonicalItem -Path (Join-Path $environment.FullName "Scripts") `
        -Description "Training environment Scripts directory"
    foreach ($root in @($lib, $sitePackages, $scripts)) {
        if (-not $root.PSIsContainer) {
            throw "Required training environment path is not a directory: $($root.FullName)"
        }
        Assert-CanonicalDescendant -Item $root -Root $environment `
            -Description "Training environment directory"
    }
    return [pscustomobject]@{
        Environment = $environment
        Lib = $lib
        SitePackages = $sitePackages
        Scripts = $scripts
    }
}

function Get-TrainerCandidateArtifacts {
    param([object]$Roots)
    $artifacts = @()
    foreach ($item in @(Get-ChildItem -LiteralPath $Roots.SitePackages.FullName -Force)) {
        $name = $item.Name.ToLowerInvariant()
        $normalized = $name -replace "[-.]+", "_"
        $hasExactOrReplacedStem = $name -match "^.roplet[_-]trainer(?<tail>.*)$"
        $tail = if ($hasExactOrReplacedStem) { [string]$Matches.tail } else { "" }
        $isPackageOrStash = $hasExactOrReplacedStem -and $tail.Length -eq 0
        $isDistInfoOrStash =
            $normalized -match "^.roplet_trainer_.+_dist_info(?:_.*)?$"
        $isBoundedTemp = $hasExactOrReplacedStem -and
            $tail -match "^[-_.~](?:tmp|old|bak|deleteme)(?:$|[-_.~].*)"
        if (-not ($isPackageOrStash -or $isDistInfoOrStash -or $isBoundedTemp)) {
            continue
        }
        $canonical = Get-CanonicalItem -Path $item.FullName `
            -Description "Trainer site-packages candidate"
        Assert-CanonicalDescendant -Item $canonical -Root $Roots.SitePackages `
            -Description "Trainer site-packages candidate"
        Assert-NoReparseTree -Item $canonical -Description "Trainer site-packages candidate"
        $artifacts += [pscustomobject]@{
            Path = $canonical.FullName
            RelativePath = "Lib\site-packages\$($canonical.Name)"
            IsDirectory = [bool]$canonical.PSIsContainer
        }
    }
    foreach ($item in @(Get-ChildItem -LiteralPath $Roots.Scripts.FullName -Force)) {
        if ($item.Name.ToLowerInvariant() -notmatch
            "^.roplet-trainer(?:$|[-_.~].*)") {
            continue
        }
        $canonical = Get-CanonicalItem -Path $item.FullName `
            -Description "Trainer entry-script candidate"
        Assert-CanonicalDescendant -Item $canonical -Root $Roots.Scripts `
            -Description "Trainer entry-script candidate"
        Assert-NoReparseTree -Item $canonical -Description "Trainer entry-script candidate"
        $artifacts += [pscustomobject]@{
            Path = $canonical.FullName
            RelativePath = "Scripts\$($canonical.Name)"
            IsDirectory = [bool]$canonical.PSIsContainer
        }
    }
    return @($artifacts | Sort-Object RelativePath)
}

function Get-InstalledTrainerLayout {
    param([string]$EnvironmentRoot)
    $sitePackages = Join-Path $EnvironmentRoot "Lib\site-packages"
    $scripts = Join-Path $EnvironmentRoot "Scripts"
    $packagePath = Join-Path $sitePackages "droplet_trainer"
    if (-not (Test-Path -LiteralPath $packagePath -PathType Container)) {
        throw "Installed droplet_trainer package directory is missing."
    }

    $distInfos = @(Get-ChildItem -LiteralPath $sitePackages -Directory |
        Where-Object { $_.Name -ceq "droplet_trainer-0.2.0.dist-info" })
    if ($distInfos.Count -ne 1) {
        throw "Expected exactly one installed droplet_trainer 0.2.0 dist-info directory."
    }
    $distInfoPath = $distInfos[0].FullName
    $metadataPath = Join-Path $distInfoPath "METADATA"
    $entryPointsPath = Join-Path $distInfoPath "entry_points.txt"
    $recordPath = Join-Path $distInfoPath "RECORD"
    foreach ($required in @($metadataPath, $entryPointsPath, $recordPath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Installed trainer metadata is incomplete: $required"
        }
    }
    $metadata = Get-Content -LiteralPath $metadataPath -Raw
    $entryPoints = Get-Content -LiteralPath $entryPointsPath -Raw
    if ($metadata -notmatch "(?mi)^Name:\s*droplet-trainer\s*$" -or
        $metadata -notmatch "(?mi)^Version:\s*0[.]2[.]0\s*$" -or
        $entryPoints -notmatch "(?mi)^droplet-trainer\s*=\s*droplet_trainer[.]cli:main\s*$") {
        throw "Installed trainer name, version, or entry point is not accepted."
    }

    $entryScripts = @()
    foreach ($row in @(Import-Csv -LiteralPath $recordPath -Header Path, Hash, Size)) {
        if ([string]$row.Path -notmatch "^(?:[.][.]/){2}Scripts/(droplet-trainer[^/]*)$") {
            continue
        }
        $candidate = [System.IO.Path]::GetFullPath((Join-Path $sitePackages ([string]$row.Path)))
        if ([System.IO.Path]::GetDirectoryName($candidate) -ne
            [System.IO.Path]::GetFullPath($scripts) -or
            -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            throw "Installed trainer entry script is outside or missing from the environment Scripts directory."
        }
        $entryScripts += $candidate
    }
    $entryScripts = @($entryScripts | Sort-Object -Unique)
    if ($entryScripts.Count -lt 1) {
        throw "Installed trainer entry scripts are missing from RECORD."
    }
    return [pscustomobject]@{
        SitePackages = $sitePackages
        Scripts = $scripts
        PackagePath = $packagePath
        DistInfoPath = $distInfoPath
        EntryScripts = $entryScripts
    }
}

function Get-ArtifactManifest {
    param([object[]]$Artifacts)
    $manifest = @()
    foreach ($artifact in @($Artifacts)) {
        $item = Get-CanonicalItem -Path $artifact.Path -Description "Trainer candidate artifact"
        Assert-NoReparseTree -Item $item -Description "Trainer candidate artifact"
        $manifest += [pscustomobject]@{
            path = [string]$artifact.RelativePath
            kind = if ($item.PSIsContainer) { "directory" } else { "file" }
            length = if ($item.PSIsContainer) { 0 } else { $item.Length }
            sha256 = if ($item.PSIsContainer) {
                ""
            } else {
                (Get-FileHash -Algorithm SHA256 -LiteralPath $item.FullName).Hash.ToLowerInvariant()
            }
        }
        if (-not $item.PSIsContainer) {
            continue
        }
        $prefixLength = $item.FullName.TrimEnd("\").Length + 1
        foreach ($child in @(Get-ChildItem -LiteralPath $item.FullName -Force -Recurse)) {
            $relative = $child.FullName.Substring($prefixLength)
            $manifest += [pscustomobject]@{
                path = "$($artifact.RelativePath)\$relative"
                kind = if ($child.PSIsContainer) { "directory" } else { "file" }
                length = if ($child.PSIsContainer) { 0 } else { $child.Length }
                sha256 = if ($child.PSIsContainer) {
                    ""
                } else {
                    (Get-FileHash -Algorithm SHA256 -LiteralPath $child.FullName).Hash.ToLowerInvariant()
                }
            }
        }
    }
    return @($manifest | Sort-Object path)
}

function Assert-ArtifactManifest {
    param([object[]]$Expected, [object[]]$Artifacts, [string]$Description)
    $actual = @(Get-ArtifactManifest -Artifacts $Artifacts)
    if (($Expected | ConvertTo-Json -Compress) -cne ($actual | ConvertTo-Json -Compress)) {
        throw "$Description candidate artifact set or hashes differ from the pre-update snapshot."
    }
}

function New-SafeSnapshotRoot {
    param([string]$Path, [object]$InstallRoot)
    $canonicalInstallRoot = Get-CanonicalItem -Path $InstallRoot.FullName `
        -Description "OpenDSS install root"
    if (Test-Path -LiteralPath $Path) {
        throw "Trainer snapshot path already exists: $Path"
    }
    $parent = Get-CanonicalItem -Path ([System.IO.Path]::GetDirectoryName($Path)) `
        -Description "Trainer snapshot parent"
    if ($parent.FullName -ine $canonicalInstallRoot.FullName) {
        Assert-CanonicalDescendant -Item $parent -Root $canonicalInstallRoot `
            -Description "Trainer snapshot parent"
    }
    $created = New-Item -ItemType Directory -Path $Path
    $canonical = Get-CanonicalItem -Path $created.FullName -Description "Trainer snapshot root"
    Assert-CanonicalDescendant -Item $canonical -Root $canonicalInstallRoot `
        -Description "Trainer snapshot root"
    return $canonical
}

function Copy-ArtifactSet {
    param(
        [object[]]$Artifacts,
        [object]$SourceRoot,
        [object]$DestinationRoot
    )
    $canonicalSourceRoot = Get-CanonicalItem -Path $SourceRoot.FullName `
        -Description "Trainer artifact copy source root"
    $canonicalDestinationRoot = Get-CanonicalItem -Path $DestinationRoot.FullName `
        -Description "Trainer artifact copy destination root"
    $destinationLibPath = Join-Path $canonicalDestinationRoot.FullName "Lib"
    $destinationLib = if (Test-Path -LiteralPath $destinationLibPath) {
        Get-CanonicalItem -Path $destinationLibPath -Description "Trainer artifact copy Lib"
    } else {
        New-Item -ItemType Directory -Path $destinationLibPath
    }
    $destinationSitePackagesPath = Join-Path $destinationLib.FullName "site-packages"
    $destinationSitePackages = if (Test-Path -LiteralPath $destinationSitePackagesPath) {
        Get-CanonicalItem -Path $destinationSitePackagesPath `
            -Description "Trainer artifact copy site-packages"
    } else {
        New-Item -ItemType Directory -Path $destinationSitePackagesPath
    }
    $destinationScriptsPath = Join-Path $canonicalDestinationRoot.FullName "Scripts"
    $destinationScripts = if (Test-Path -LiteralPath $destinationScriptsPath) {
        Get-CanonicalItem -Path $destinationScriptsPath `
            -Description "Trainer artifact copy Scripts"
    } else {
        New-Item -ItemType Directory -Path $destinationScriptsPath
    }
    foreach ($destination in @($destinationLib, $destinationSitePackages, $destinationScripts)) {
        $canonicalDestination = Get-CanonicalItem -Path $destination.FullName `
            -Description "Trainer artifact copy destination"
        Assert-CanonicalDescendant -Item $canonicalDestination -Root $canonicalDestinationRoot `
            -Description "Trainer artifact copy destination"
    }

    foreach ($artifact in @($Artifacts)) {
        $source = Get-CanonicalItem -Path $artifact.Path -Description "Trainer artifact copy source"
        Assert-CanonicalDescendant -Item $source -Root $canonicalSourceRoot `
            -Description "Trainer artifact copy source"
        Assert-NoReparseTree -Item $source -Description "Trainer artifact copy source"
        if ($artifact.RelativePath -like "Lib\site-packages\*") {
            $destinationParent = $destinationSitePackages
        } elseif ($artifact.RelativePath -like "Scripts\*") {
            $destinationParent = $destinationScripts
        } else {
            throw "Unsupported trainer artifact path: $($artifact.RelativePath)"
        }
        $destinationPath = Join-Path $destinationParent.FullName (
            [System.IO.Path]::GetFileName($artifact.Path))
        if (Test-Path -LiteralPath $destinationPath) {
            throw "Trainer artifact copy destination already exists: $destinationPath"
        }
        Copy-Item -LiteralPath $source.FullName -Destination $destinationPath -Recurse
        $copied = Get-CanonicalItem -Path $destinationPath `
            -Description "Copied trainer artifact"
        Assert-CanonicalDescendant -Item $copied -Root $canonicalDestinationRoot `
            -Description "Copied trainer artifact"
        Assert-NoReparseTree -Item $copied -Description "Copied trainer artifact"
    }
}

function Remove-ScopedItem {
    param([string]$Path, [object]$AllowedRoot)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $canonicalRoot = Get-CanonicalItem -Path $AllowedRoot.FullName `
        -Description "Trainer removal root"
    $item = Get-CanonicalItem -Path $Path -Description "Trainer removal target"
    Assert-CanonicalDescendant -Item $item -Root $canonicalRoot `
        -Description "Trainer removal target"
    Assert-NoReparseTree -Item $item -Description "Trainer removal target"
    Remove-Item -LiteralPath $item.FullName -Recurse -Force
}

function Restore-ArtifactSet {
    param(
        [object[]]$SnapshotArtifacts,
        [object]$SnapshotRoot,
        [object]$EnvironmentRoots
    )
    $currentArtifacts = @(Get-TrainerCandidateArtifacts -Roots $EnvironmentRoots)
    foreach ($artifact in $currentArtifacts) {
        Remove-ScopedItem -Path $artifact.Path -AllowedRoot $EnvironmentRoots.Environment
    }
    if (@(Get-TrainerCandidateArtifacts -Roots $EnvironmentRoots).Count -ne 0) {
        throw "Trainer replacement artifacts remain after bounded rollback removal."
    }
    Copy-ArtifactSet -Artifacts $SnapshotArtifacts -SourceRoot $SnapshotRoot `
        -DestinationRoot $EnvironmentRoots.Environment
}

function Assert-Inventory {
    param([string]$Python, [object]$Inventory)
    $actualJson = & $Python -I -m pip list --format=json
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inventory the updated training environment."
    }
    $actual = @{}
    foreach ($entry in @($actualJson | ConvertFrom-Json)) {
        $actual[([string]$entry.name).ToLowerInvariant()] = [string]$entry.version
    }
    $expected = @{}
    foreach ($property in $Inventory.distributions.PSObject.Properties) {
        $expected[$property.Name.ToLowerInvariant()] = [string]$property.Value
    }
    if ($actual.Count -ne 37 -or $actual.Count -ne $expected.Count) {
        throw "Distribution count mismatch. Expected 37; got $($actual.Count)."
    }
    foreach ($name in $expected.Keys) {
        if (-not $actual.ContainsKey($name) -or $actual[$name] -ne $expected[$name]) {
            throw "Distribution mismatch for $name. Expected $($expected[$name]); got $($actual[$name])."
        }
    }
    if ($actual.ContainsKey("onnxruntime") -or -not $actual.ContainsKey("onnxruntime-gpu")) {
        throw "Exactly the onnxruntime-gpu distribution must be installed."
    }
}

function Write-UpdaterDiagnostic {
    param(
        [object]$DiagnosticsRoot,
        [string]$LogPath,
        [string]$Phase,
        [string]$Status,
        [string]$ExceptionType,
        [string]$ExceptionMessage,
        [AllowNull()]
        [Nullable[int]]$ExitCode,
        [bool]$SnapshotCreated,
        [bool]$InstallStarted,
        [bool]$UpdateValidated,
        [string]$RollbackOutcome,
        [string]$RollbackDetail,
        [string]$CleanupOutcome,
        [string]$CleanupDetail
    )
    $canonicalRoot = Get-CanonicalItem -Path $DiagnosticsRoot.FullName `
        -Description "Trainer updater diagnostics root"
    $boundedException = [string]$ExceptionMessage
    $boundedRollback = [string]$RollbackDetail
    $boundedCleanup = [string]$CleanupDetail
    if ($boundedException.Length -gt 2048) {
        $boundedException = $boundedException.Substring(0, 2048) + "...[truncated]"
    }
    if ($boundedRollback.Length -gt 1024) {
        $boundedRollback = $boundedRollback.Substring(0, 1024) + "...[truncated]"
    }
    if ($boundedCleanup.Length -gt 1024) {
        $boundedCleanup = $boundedCleanup.Substring(0, 1024) + "...[truncated]"
    }
    $record = [ordered]@{
        schema_version = "opendss-trainer-wheel-update-diagnostic-v1"
        timestamp_utc = [DateTime]::UtcNow.ToString("o")
        phase = $Phase
        status = $Status
        exception = if ([string]::IsNullOrEmpty($ExceptionType)) {
            $null
        } else {
            [ordered]@{
                type = $ExceptionType
                message = $boundedException
            }
        }
        exit_code = if ($null -eq $ExitCode) { $null } else { [int]$ExitCode }
        snapshot_created = $SnapshotCreated
        install_started = $InstallStarted
        update_validated = $UpdateValidated
        rollback = [ordered]@{
            outcome = $RollbackOutcome
            detail = if ([string]::IsNullOrEmpty($boundedRollback)) {
                $null
            } else {
                $boundedRollback
            }
        }
        cleanup = [ordered]@{
            outcome = $CleanupOutcome
            detail = if ([string]::IsNullOrEmpty($boundedCleanup)) {
                $null
            } else {
                $boundedCleanup
            }
        }
    }
    $json = $record | ConvertTo-Json -Depth 4
    $tempPath = Join-Path $canonicalRoot.FullName (
        ".training-trainer-wheel-update-$([Guid]::NewGuid().ToString('N')).tmp")
    try {
        [System.IO.File]::WriteAllText(
            $tempPath,
            $json,
            [System.Text.UTF8Encoding]::new($false))
        $tempItem = Get-CanonicalItem -Path $tempPath `
            -Description "Trainer updater diagnostic temporary file"
        Assert-CanonicalDescendant -Item $tempItem -Root $canonicalRoot `
            -Description "Trainer updater diagnostic temporary file"
        if (Test-Path -LiteralPath $LogPath) {
            $logItem = Get-CanonicalItem -Path $LogPath `
                -Description "Trainer updater diagnostic log"
            Assert-CanonicalDescendant -Item $logItem -Root $canonicalRoot `
                -Description "Trainer updater diagnostic log"
            if ($logItem.PSIsContainer) {
                throw "Trainer updater diagnostic log path is a directory."
            }
            [System.IO.File]::Replace($tempItem.FullName, $logItem.FullName, $null, $true)
        } else {
            [System.IO.File]::Move($tempItem.FullName, $LogPath)
        }
    } finally {
        if (Test-Path -LiteralPath $tempPath) {
            $tempItem = Get-CanonicalItem -Path $tempPath `
                -Description "Trainer updater diagnostic temporary file"
            Assert-CanonicalDescendant -Item $tempItem -Root $canonicalRoot `
                -Description "Trainer updater diagnostic temporary file"
            Remove-Item -LiteralPath $tempItem.FullName -Force
        }
    }
}

if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    throw "LOCALAPPDATA is required for the exact OpenDSS training environment."
}
$installRoot = [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "OpenDSS"))
$installRootItem = Get-CanonicalItem -Path $installRoot -Description "OpenDSS install root"
$diagnosticsRootPath = Join-Path $installRootItem.FullName "diagnostics"
if (-not (Test-Path -LiteralPath $diagnosticsRootPath)) {
    New-Item -ItemType Directory -Path $diagnosticsRootPath | Out-Null
}
$diagnosticsRootItem = Get-CanonicalItem -Path $diagnosticsRootPath `
    -Description "Trainer updater diagnostics root"
Assert-CanonicalDescendant -Item $diagnosticsRootItem -Root $installRootItem `
    -Description "Trainer updater diagnostics root"
if (-not $diagnosticsRootItem.PSIsContainer) {
    throw "Trainer updater diagnostics path is not a directory."
}
$diagnosticLogPath = Join-Path $diagnosticsRootItem.FullName (
    "training-trainer-wheel-update.json")

$phase = "repository-preflight"
$commandExitCode = $null
$snapshotRootItem = $null
$snapshotArtifacts = @()
$installStarted = $false
$updateValidated = $false
$rollbackOutcome = "not-required"
$rollbackDetail = ""
$cleanupOutcome = "not-required"
$cleanupDetail = ""
try {
    $wheel = [System.IO.Path]::GetFullPath($WheelPath)
    $requirementsRoot = [System.IO.Path]::GetFullPath((
        Join-Path $PSScriptRoot "..\..\requirements"))
    $lockPath = Join-Path $requirementsRoot "windows-py312-gpu-cu130.lock"
    $inventoryPath = Join-Path $requirementsRoot (
        "windows-py312-gpu-cu130-inventory.json")
    foreach ($required in @($lockPath, $inventoryPath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Authoritative repository input is missing: $required"
        }
    }

    $phase = "environment-preflight"
    $environmentRoot = Join-Path $installRoot "training-venv-gpu"
    $python = Join-Path $environmentRoot "Scripts\python.exe"
    if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
        throw "The exact OpenDSS training Python is missing: $python"
    }
    $environmentRoots = Get-EnvironmentRoots -EnvironmentRoot $environmentRoot
    Assert-CanonicalDescendant -Item $environmentRoots.Environment -Root $installRootItem `
        -Description "Training environment"
    $pythonItem = Get-CanonicalItem -Path $python -Description "OpenDSS training Python"
    Assert-CanonicalDescendant -Item $pythonItem -Root $environmentRoots.Scripts `
        -Description "OpenDSS training Python"
    if ($pythonItem.PSIsContainer) {
        throw "The exact OpenDSS training Python is not a file: $($pythonItem.FullName)"
    }
    $python = $pythonItem.FullName

    $phase = "authority-preflight"
    $inventory = Assert-Authority -LockPath $lockPath -InventoryPath $inventoryPath
    Assert-Wheel -Path $wheel

    $phase = "installed-layout-preflight"
    $originalArtifacts = @(Get-TrainerCandidateArtifacts -Roots $environmentRoots)
    $original = Get-InstalledTrainerLayout -EnvironmentRoot $environmentRoot
    $candidatePaths = @{}
    foreach ($artifact in $originalArtifacts) {
        $candidatePaths[([string]$artifact.Path).ToLowerInvariant()] = $true
    }
    foreach ($requiredPath in @(
        $original.PackagePath,
        $original.DistInfoPath
    ) + @($original.EntryScripts)) {
        $requiredItem = Get-CanonicalItem -Path $requiredPath `
            -Description "Installed trainer artifact"
        if (-not $candidatePaths.ContainsKey($requiredItem.FullName.ToLowerInvariant())) {
            throw "Installed trainer artifact was not included in the bounded candidate set: $requiredPath"
        }
    }
    $originalManifest = @(Get-ArtifactManifest -Artifacts $originalArtifacts)
} catch {
    $preflightError = $_
    try {
        Write-UpdaterDiagnostic -DiagnosticsRoot $diagnosticsRootItem `
            -LogPath $diagnosticLogPath -Phase $phase -Status "failed" `
            -ExceptionType $preflightError.Exception.GetType().FullName `
            -ExceptionMessage $preflightError.Exception.Message -ExitCode $null `
            -SnapshotCreated $false -InstallStarted $false -UpdateValidated $false `
            -RollbackOutcome "not-required" -RollbackDetail "" `
            -CleanupOutcome "not-required" -CleanupDetail ""
    } catch {
        throw "Trainer updater preflight failed: $($preflightError.Exception.Message) Diagnostic capture also failed: $($_.Exception.Message)"
    }
    throw "Trainer updater preflight failed: $($preflightError.Exception.Message)"
}

$snapshotRoot = Join-Path $installRoot (
    ".training-trainer-wheel-snapshot-$([Guid]::NewGuid().ToString('N'))")

$oldPythonPath = $env:PYTHONPATH
$oldPythonHome = $env:PYTHONHOME
$oldPythonNoUserSite = $env:PYTHONNOUSERSITE
$oldPipNoIndex = $env:PIP_NO_INDEX
try {
    $phase = "snapshot-create"
    $snapshotRootItem = New-SafeSnapshotRoot -Path $snapshotRoot -InstallRoot $installRootItem
    $phase = "snapshot-copy"
    Copy-ArtifactSet -Artifacts $originalArtifacts `
        -SourceRoot $environmentRoots.Environment -DestinationRoot $snapshotRootItem
    $snapshotRoots = Get-EnvironmentRoots -EnvironmentRoot $snapshotRootItem.FullName
    $snapshotArtifacts = @(Get-TrainerCandidateArtifacts -Roots $snapshotRoots)
    Assert-ArtifactManifest -Expected $originalManifest -Artifacts $snapshotArtifacts `
        -Description "Snapshot"

    $env:PYTHONPATH = $null
    $env:PYTHONHOME = $null
    $env:PYTHONNOUSERSITE = "1"
    $env:PIP_NO_INDEX = "1"

    $phase = "pip-install"
    $commandExitCode = $null
    $installStarted = $true
    & $python -I -m pip install --no-index --no-deps --force-reinstall $wheel
    $commandExitCode = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) {
        throw "Pinned local trainer installation failed with exit code $LASTEXITCODE."
    }

    $phase = "post-install-layout"
    $updatedRoots = Get-EnvironmentRoots -EnvironmentRoot $environmentRoot
    $null = @(Get-TrainerCandidateArtifacts -Roots $updatedRoots)
    $null = Get-InstalledTrainerLayout -EnvironmentRoot $updatedRoots.Environment.FullName
    $phase = "inventory-validation"
    Assert-Inventory -Python $python -Inventory $inventory
    $commandExitCode = $LASTEXITCODE
    $phase = "environment-check"
    $checkOutput = Join-Path $snapshotRoot "env-check"
    New-Item -ItemType Directory -Path $checkOutput -Force | Out-Null
    & $python -I -m droplet_trainer env-check --device auto `
        --require-training --require-onnx --check-output $checkOutput --json
    $commandExitCode = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) {
        throw "Isolated droplet-trainer environment check failed with exit code $LASTEXITCODE."
    }
    $updateValidated = $true
} catch {
    $updateError = $_
    $failurePhase = $phase
    if ($failurePhase -eq "inventory-validation") {
        $commandExitCode = $LASTEXITCODE
    }
    $originalFailure = $updateError.Exception.Message
    $failureMessage = "Trainer update failed: $originalFailure"
    if ($installStarted -and $snapshotArtifacts.Count -gt 0) {
        $rollbackOutcome = "attempting"
        try {
            $rollbackRoots = Get-EnvironmentRoots -EnvironmentRoot $environmentRoot
            Restore-ArtifactSet -SnapshotArtifacts $snapshotArtifacts `
                -SnapshotRoot $snapshotRootItem -EnvironmentRoots $rollbackRoots
            $restoredRoots = Get-EnvironmentRoots -EnvironmentRoot $environmentRoot
            $restoredArtifacts = @(Get-TrainerCandidateArtifacts -Roots $restoredRoots)
            Assert-ArtifactManifest -Expected $originalManifest -Artifacts $restoredArtifacts `
                -Description "Post-rollback installation"
            $null = Get-InstalledTrainerLayout -EnvironmentRoot $restoredRoots.Environment.FullName
            $failureMessage =
                "Trainer update failed and the exact original candidate artifact set was restored and verified: $originalFailure"
            $rollbackOutcome = "restored-and-verified"
        } catch {
            $rollbackOutcome = "failed"
            $rollbackDetail = $_.Exception.Message
            $failureMessage =
                "$failureMessage Rollback also failed: $rollbackDetail Snapshot retained at $snapshotRoot"
        }
    }
    if ($rollbackOutcome -ne "failed" -and $null -ne $snapshotRootItem -and
        (Test-Path -LiteralPath $snapshotRoot)) {
        $cleanupOutcome = "attempting"
        try {
            Remove-ScopedItem -Path $snapshotRoot -AllowedRoot $installRootItem
            $cleanupOutcome = "removed"
        } catch {
            $cleanupOutcome = "failed"
            $cleanupDetail = $_.Exception.Message
            $failureMessage =
                "$failureMessage Snapshot cleanup also failed: $cleanupDetail Snapshot retained at $snapshotRoot"
        }
    } elseif ($rollbackOutcome -eq "failed") {
        $cleanupOutcome = "retained-after-rollback-failure"
    }
    try {
        Write-UpdaterDiagnostic -DiagnosticsRoot $diagnosticsRootItem `
            -LogPath $diagnosticLogPath -Phase $failurePhase -Status "failed" `
            -ExceptionType $updateError.Exception.GetType().FullName `
            -ExceptionMessage $originalFailure -ExitCode $commandExitCode `
            -SnapshotCreated ($null -ne $snapshotRootItem) `
            -InstallStarted $installStarted -UpdateValidated $updateValidated `
            -RollbackOutcome $rollbackOutcome -RollbackDetail $rollbackDetail `
            -CleanupOutcome $cleanupOutcome -CleanupDetail $cleanupDetail
    } catch {
        $failureMessage =
            "$failureMessage Diagnostic capture also failed: $($_.Exception.Message)"
    }
    throw $failureMessage
} finally {
    $env:PYTHONPATH = $oldPythonPath
    $env:PYTHONHOME = $oldPythonHome
    $env:PYTHONNOUSERSITE = $oldPythonNoUserSite
    $env:PIP_NO_INDEX = $oldPipNoIndex
}

if (-not $updateValidated) {
    throw "Trainer update did not reach validated success."
}
$phase = "snapshot-cleanup"
$cleanupOutcome = "attempting"
try {
    Remove-ScopedItem -Path $snapshotRoot -AllowedRoot $installRootItem
    $cleanupOutcome = "removed"
} catch {
    $cleanupOutcome = "failed"
    $cleanupDetail = $_.Exception.Message
    Write-UpdaterDiagnostic -DiagnosticsRoot $diagnosticsRootItem `
        -LogPath $diagnosticLogPath -Phase $phase -Status "failed" `
        -ExceptionType $_.Exception.GetType().FullName `
        -ExceptionMessage $_.Exception.Message -ExitCode $commandExitCode `
        -SnapshotCreated $true -InstallStarted $installStarted `
        -UpdateValidated $updateValidated -RollbackOutcome "not-required" `
        -RollbackDetail "" -CleanupOutcome $cleanupOutcome `
        -CleanupDetail $cleanupDetail
    throw
}
$phase = "complete"
Write-UpdaterDiagnostic -DiagnosticsRoot $diagnosticsRootItem `
    -LogPath $diagnosticLogPath -Phase $phase -Status "succeeded" `
    -ExceptionType "" -ExceptionMessage "" -ExitCode $commandExitCode `
    -SnapshotCreated $true -InstallStarted $installStarted `
    -UpdateValidated $updateValidated -RollbackOutcome "not-required" `
    -RollbackDetail "" -CleanupOutcome $cleanupOutcome -CleanupDetail ""
Write-Host "OpenDSS droplet-trainer updated atomically to $ExpectedHash."
