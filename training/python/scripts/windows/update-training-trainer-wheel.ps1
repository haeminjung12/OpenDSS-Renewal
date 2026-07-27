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
        if ([string]$row.Path -notmatch "^(?:[.][.]/){3}Scripts/(droplet-trainer[^/]*)$") {
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

function Get-LayoutManifest {
    param([object]$Layout)
    $manifest = @()
    foreach ($directory in @(
        [pscustomobject]@{ Path = $Layout.PackagePath; Label = "site-packages\droplet_trainer" },
        [pscustomobject]@{
            Path = $Layout.DistInfoPath
            Label = "site-packages\$([System.IO.Path]::GetFileName($Layout.DistInfoPath))"
        }
    )) {
        $prefixLength = ([string]$directory.Path).TrimEnd("\").Length + 1
        foreach ($file in @(Get-ChildItem -LiteralPath $directory.Path -File -Recurse)) {
            $relative = $file.FullName.Substring($prefixLength)
            $manifest += [pscustomobject]@{
                path = "$($directory.Label)\$relative"
                length = $file.Length
                sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
            }
        }
    }
    foreach ($script in @($Layout.EntryScripts)) {
        $file = Get-Item -LiteralPath $script
        $manifest += [pscustomobject]@{
            path = "Scripts\$($file.Name)"
            length = $file.Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
        }
    }
    return @($manifest | Sort-Object path)
}

function Assert-Manifest {
    param([object[]]$Expected, [object]$Layout, [string]$Description)
    $actual = @(Get-LayoutManifest -Layout $Layout)
    if (($Expected | ConvertTo-Json -Compress) -cne ($actual | ConvertTo-Json -Compress)) {
        throw "$Description does not match the exact pre-update trainer snapshot."
    }
}

function Copy-Layout {
    param([object]$Source, [string]$DestinationRoot)
    $destinationSitePackages = Join-Path $DestinationRoot "Lib\site-packages"
    $destinationScripts = Join-Path $DestinationRoot "Scripts"
    New-Item -ItemType Directory -Path $destinationSitePackages, $destinationScripts -Force | Out-Null
    $packageDestination = Join-Path $destinationSitePackages "droplet_trainer"
    $distInfoDestination = Join-Path $destinationSitePackages (
        [System.IO.Path]::GetFileName($Source.DistInfoPath))
    Copy-Item -LiteralPath $Source.PackagePath -Destination $packageDestination -Recurse
    Copy-Item -LiteralPath $Source.DistInfoPath -Destination $distInfoDestination -Recurse
    $entryDestinations = @()
    foreach ($script in @($Source.EntryScripts)) {
        $destination = Join-Path $destinationScripts ([System.IO.Path]::GetFileName($script))
        Copy-Item -LiteralPath $script -Destination $destination
        $entryDestinations += $destination
    }
    return [pscustomobject]@{
        SitePackages = $destinationSitePackages
        Scripts = $destinationScripts
        PackagePath = $packageDestination
        DistInfoPath = $distInfoDestination
        EntryScripts = $entryDestinations
    }
}

function Remove-ScopedItem {
    param([string]$Path, [string]$AllowedRoot)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $resolvedRoot = [System.IO.Path]::GetFullPath($AllowedRoot).TrimEnd("\") + "\"
    if (-not $resolvedPath.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove path outside the exact training environment: $resolvedPath"
    }
    Remove-Item -LiteralPath $resolvedPath -Recurse -Force
}

function Restore-Layout {
    param([object]$Original, [object]$Snapshot, [string]$EnvironmentRoot)
    Remove-ScopedItem -Path $Original.PackagePath -AllowedRoot $EnvironmentRoot
    Remove-ScopedItem -Path $Original.DistInfoPath -AllowedRoot $EnvironmentRoot
    foreach ($script in @($Original.EntryScripts)) {
        Remove-ScopedItem -Path $script -AllowedRoot $EnvironmentRoot
    }
    Copy-Item -LiteralPath $Snapshot.PackagePath -Destination $Original.PackagePath -Recurse
    Copy-Item -LiteralPath $Snapshot.DistInfoPath -Destination $Original.DistInfoPath -Recurse
    foreach ($script in @($Snapshot.EntryScripts)) {
        Copy-Item -LiteralPath $script -Destination (
            Join-Path $Original.Scripts ([System.IO.Path]::GetFileName($script)))
    }
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

$wheel = [System.IO.Path]::GetFullPath($WheelPath)
$requirementsRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\requirements"))
$lockPath = Join-Path $requirementsRoot "windows-py312-gpu-cu130.lock"
$inventoryPath = Join-Path $requirementsRoot "windows-py312-gpu-cu130-inventory.json"
foreach ($required in @($lockPath, $inventoryPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Authoritative repository input is missing: $required"
    }
}
if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    throw "LOCALAPPDATA is required for the exact OpenDSS training environment."
}
$installRoot = [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "OpenDSS"))
$environmentRoot = Join-Path $installRoot "training-venv-gpu"
$python = Join-Path $environmentRoot "Scripts\python.exe"
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
    throw "The exact OpenDSS training Python is missing: $python"
}

$inventory = Assert-Authority -LockPath $lockPath -InventoryPath $inventoryPath
Assert-Wheel -Path $wheel
$original = Get-InstalledTrainerLayout -EnvironmentRoot $environmentRoot
$originalManifest = @(Get-LayoutManifest -Layout $original)
$snapshotRoot = Join-Path $installRoot (
    ".training-trainer-wheel-snapshot-$([Guid]::NewGuid().ToString('N'))")
$snapshot = $null
$installStarted = $false
$keepSnapshot = $false

$oldPythonPath = $env:PYTHONPATH
$oldPythonHome = $env:PYTHONHOME
$oldPythonNoUserSite = $env:PYTHONNOUSERSITE
$oldPipNoIndex = $env:PIP_NO_INDEX
try {
    $snapshot = Copy-Layout -Source $original -DestinationRoot $snapshotRoot
    Assert-Manifest -Expected $originalManifest -Layout $snapshot -Description "Snapshot"

    $env:PYTHONPATH = $null
    $env:PYTHONHOME = $null
    $env:PYTHONNOUSERSITE = "1"
    $env:PIP_NO_INDEX = "1"

    $installStarted = $true
    & $python -I -m pip install --no-index --no-deps --force-reinstall $wheel
    if ($LASTEXITCODE -ne 0) {
        throw "Pinned local trainer installation failed with exit code $LASTEXITCODE."
    }

    $null = Get-InstalledTrainerLayout -EnvironmentRoot $environmentRoot
    Assert-Inventory -Python $python -Inventory $inventory
    $checkOutput = Join-Path $snapshotRoot "env-check"
    New-Item -ItemType Directory -Path $checkOutput -Force | Out-Null
    & $python -I -m droplet_trainer env-check --device auto `
        --require-training --require-onnx --check-output $checkOutput --json
    if ($LASTEXITCODE -ne 0) {
        throw "Isolated droplet-trainer environment check failed with exit code $LASTEXITCODE."
    }
    Write-Host "OpenDSS droplet-trainer updated atomically to $ExpectedHash."
} catch {
    $originalFailure = $_.Exception.Message
    if ($installStarted -and $null -ne $snapshot) {
        try {
            Restore-Layout -Original $original -Snapshot $snapshot -EnvironmentRoot $environmentRoot
            $restored = Get-InstalledTrainerLayout -EnvironmentRoot $environmentRoot
            Assert-Manifest -Expected $originalManifest -Layout $restored -Description "Post-rollback installation"
        } catch {
            $keepSnapshot = $true
            throw "Trainer update failed: $originalFailure Rollback also failed: $($_.Exception.Message) Snapshot retained at $snapshotRoot"
        }
        throw "Trainer update failed and the exact original installation was restored and verified: $originalFailure"
    }
    throw
} finally {
    $env:PYTHONPATH = $oldPythonPath
    $env:PYTHONHOME = $oldPythonHome
    $env:PYTHONNOUSERSITE = $oldPythonNoUserSite
    $env:PIP_NO_INDEX = $oldPipNoIndex
    if (-not $keepSnapshot -and (Test-Path -LiteralPath $snapshotRoot)) {
        try {
            Remove-ScopedItem -Path $snapshotRoot -AllowedRoot $installRoot
        } catch {
            Write-Warning "Could not remove trainer update snapshot: $snapshotRoot"
        }
    }
}
