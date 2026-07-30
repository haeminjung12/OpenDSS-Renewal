[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BootstrapRoot,
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA "OpenDSS"),
    [string]$CheckOutput = (Join-Path $env:LOCALAPPDATA "OpenDSS\training-runtime-check")
)

$ErrorActionPreference = "Stop"
$PythonVersion = "3.12.10"
$RuntimeName = "python-$PythonVersion"
$EnvironmentName = "training-venv-gpu"

function Remove-ScopedDirectory {
    param([string]$Path, [string]$AllowedRoot)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $resolvedRoot = [System.IO.Path]::GetFullPath($AllowedRoot).TrimEnd("\") + "\"
    if (-not $resolvedPath.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove path outside InstallRoot: $resolvedPath"
    }
    Remove-Item -LiteralPath $resolvedPath -Recurse -Force
}

function Get-LockEntries {
    param([string]$LockPath)
    $entries = @()
    foreach ($line in Get-Content -LiteralPath $LockPath) {
        if ($line -notmatch "==") {
            continue
        }
        if ($line -match
            "^\s*([A-Za-z0-9_.-]+)==([^\s]+)\s+--hash=sha256:([0-9a-fA-F]{64})\s*$") {
            $name = $Matches[1]
            $version = $Matches[2]
            $hash = $Matches[3].ToLowerInvariant()
        } else {
            throw "Unsupported authoritative lock line: $line"
        }
        $entries += [pscustomobject]@{
            name = $name
            version = $version
            sha256 = $hash
        }
    }
    if ($entries.Count -ne 37) {
        throw "Authoritative lock must contain exactly 37 entries; got $($entries.Count)."
    }
    return $entries
}

function Get-LockedFiles {
    param([object[]]$LockEntries, [string[]]$Filenames)
    $result = @{}
    foreach ($entry in $LockEntries) {
        $normalizedName = ([string]$entry.name).ToLowerInvariant() -replace "[-.]+", "_"
        $prefix = "$normalizedName-$(([string]$entry.version).ToLowerInvariant())-"
        $matches = @($Filenames | Where-Object {
            $_.ToLowerInvariant().StartsWith($prefix, [System.StringComparison]::Ordinal)
        })
        if ($matches.Count -ne 1) {
            throw "Lock entry $($entry.name)==$($entry.version) resolves to $($matches.Count) bootstrap files."
        }
        $key = $matches[0].ToLowerInvariant()
        if ($result.ContainsKey($key)) {
            throw "Bootstrap filename is duplicated: $($matches[0])"
        }
        $result[$key] = [string]$entry.sha256
    }
    if ($result.Count -ne $Filenames.Count) {
        throw "Bootstrap filenames do not map one-to-one to the authoritative lock."
    }
    return $result
}

function Assert-Inventory {
    param([string]$Python, [string]$InventoryPath)
    $inventory = Get-Content -LiteralPath $InventoryPath -Raw | ConvertFrom-Json
    $actualJson = & $Python -I -m pip list --format=json
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inventory the provisioned environment."
    }
    $actual = @{}
    foreach ($entry in @($actualJson | ConvertFrom-Json)) {
        $actual[([string]$entry.name).ToLowerInvariant()] = [string]$entry.version
    }
    $expected = @{}
    foreach ($property in $inventory.distributions.PSObject.Properties) {
        $expected[$property.Name.ToLowerInvariant()] = [string]$property.Value
    }
    if ($actual.Count -ne [int]$inventory.runtime.distribution_count -or
        $actual.Count -ne $expected.Count) {
        throw "Distribution count mismatch. Expected $($expected.Count); got $($actual.Count)."
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

$bootstrap = [System.IO.Path]::GetFullPath($BootstrapRoot)
$install = [System.IO.Path]::GetFullPath($InstallRoot)
if ($install.Length -lt 8 -or [System.IO.Path]::GetPathRoot($install) -eq $install) {
    throw "InstallRoot is too broad: $install"
}

$catalogPath = Join-Path $bootstrap "windows-py312-gpu-cu130-downloads.json"
$lockPath = Join-Path $bootstrap "windows-py312-gpu-cu130.lock"
$inventoryPath = Join-Path $bootstrap "windows-py312-gpu-cu130-inventory.json"
foreach ($required in @($catalogPath, $lockPath, $inventoryPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Bootstrap input is missing: $required"
    }
}

$catalog = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
if ([string]$catalog.schema_version -ne "opendss-training-bootstrap-downloads-v1" -or
    @($catalog.wheels).Count -ne 36) {
    throw "Training bootstrap download catalog is not accepted."
}
$embeddedWheel = Join-Path $bootstrap ([string]$catalog.embedded_wheel)
if (-not (Test-Path -LiteralPath $embeddedWheel -PathType Leaf)) {
    throw "Embedded OpenDSS trainer wheel is missing: $embeddedWheel"
}

$downloadEntries = @($catalog.wheels)
$filenames = @($downloadEntries | ForEach-Object { [string]$_.filename })
$filenames += [string]$catalog.embedded_wheel
$lockedFiles = Get-LockedFiles -LockEntries (Get-LockEntries -LockPath $lockPath) -Filenames $filenames

$allowedHosts = @("files.pythonhosted.org", "download.pytorch.org", "download-r2.pytorch.org")
foreach ($entry in $downloadEntries) {
    $uri = [Uri]([string]$entry.url)
    $remoteName = [Uri]::UnescapeDataString($uri.Segments[-1])
    if ($uri.Scheme -ne "https" -or $allowedHosts -notcontains $uri.DnsSafeHost.ToLowerInvariant() -or
        $remoteName -ne [string]$entry.filename) {
        throw "Unapproved training wheel URL: $($entry.url)"
    }
}
$pythonUri = [Uri]([string]$catalog.python.url)
if ($pythonUri.Scheme -ne "https" -or $pythonUri.DnsSafeHost -ne "www.python.org" -or
    [Uri]::UnescapeDataString($pythonUri.Segments[-1]) -ne [string]$catalog.python.filename) {
    throw "Unapproved CPython download URL."
}

$inventory = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json
if ([string]$catalog.python.filename -ne [string]$inventory.python.installer_file) {
    throw "CPython catalog filename differs from the accepted inventory."
}

New-Item -ItemType Directory -Path $install -Force | Out-Null
$token = [Guid]::NewGuid().ToString("N")
$downloadCandidate = Join-Path $install ".training-download-$token"
$wheelhouse = Join-Path $downloadCandidate "wheelhouse"
$runtime = Join-Path $install $RuntimeName
$runtimeCandidate = Join-Path $install ".$RuntimeName.candidate-$token"
$runtimeBackup = Join-Path $install ".$RuntimeName.backup-$token"
$environment = Join-Path $install $EnvironmentName
$environmentCandidate = Join-Path $install ".$EnvironmentName.candidate-$token"
$environmentBackup = Join-Path $install ".$EnvironmentName.backup-$token"
$runtimePublished = $false
$environmentPublished = $false

$oldPythonPath = $env:PYTHONPATH
$oldPythonHome = $env:PYTHONHOME
$oldPythonNoUserSite = $env:PYTHONNOUSERSITE
$oldPipNoIndex = $env:PIP_NO_INDEX
try {
    $env:PYTHONPATH = $null
    $env:PYTHONHOME = $null
    $env:PYTHONNOUSERSITE = "1"
    $env:PIP_NO_INDEX = "1"

    New-Item -ItemType Directory -Path $wheelhouse -Force | Out-Null
    $installer = Join-Path $downloadCandidate ([string]$catalog.python.filename)
    Write-Host "Downloading verified CPython $PythonVersion runtime..."
    Invoke-WebRequest -Uri $catalog.python.url -OutFile $installer -UseBasicParsing
    $installerHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash.ToLowerInvariant()
    if ($installerHash -ne ([string]$inventory.python.installer_sha256).ToLowerInvariant()) {
        throw "Downloaded CPython installer hash does not match the accepted inventory."
    }
    $installerSignature = Get-AuthenticodeSignature -LiteralPath $installer
    if ($installerSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
        $installerSignature.SignerCertificate.Subject -notmatch "Python Software Foundation") {
        throw "Downloaded CPython installer does not have the accepted PSF signature."
    }

    foreach ($entry in $downloadEntries) {
        $destination = Join-Path $wheelhouse ([string]$entry.filename)
        Write-Host "Downloading verified training dependency: $($entry.filename)"
        Invoke-WebRequest -Uri $entry.url -OutFile $destination -UseBasicParsing
        $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant()
        if ($actualHash -ne $lockedFiles[([string]$entry.filename).ToLowerInvariant()]) {
            throw "Downloaded wheel hash does not match the authoritative lock: $($entry.filename)"
        }
    }
    $embeddedDestination = Join-Path $wheelhouse ([string]$catalog.embedded_wheel)
    Copy-Item -LiteralPath $embeddedWheel -Destination $embeddedDestination -Force
    $embeddedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $embeddedDestination).Hash.ToLowerInvariant()
    if ($embeddedHash -ne $lockedFiles[([string]$catalog.embedded_wheel).ToLowerInvariant()]) {
        throw "Embedded OpenDSS trainer wheel hash does not match the authoritative lock."
    }

    $installerArguments = @(
        "/quiet",
        "InstallAllUsers=0",
        "Include_pip=1",
        "Include_launcher=0",
        "Include_test=0",
        "Include_doc=0",
        "Include_tcltk=0",
        "Shortcuts=0",
        "AssociateFiles=0",
        "PrependPath=0",
        "AppendPath=0",
        "SimpleInstall=1",
        "TargetDir=$runtimeCandidate"
    )
    $installerProcess = Start-Process -FilePath $installer -ArgumentList $installerArguments `
        -Wait -PassThru -WindowStyle Hidden
    if ($installerProcess.ExitCode -ne 0) {
        throw "CPython installation failed with exit code $($installerProcess.ExitCode)."
    }
    $candidatePython = Join-Path $runtimeCandidate "python.exe"
    if (-not (Test-Path -LiteralPath $candidatePython -PathType Leaf)) {
        throw "CPython did not create the isolated candidate runtime."
    }
    $version = & $candidatePython -I -c "import platform,struct; print(platform.python_version() + '|' + str(struct.calcsize('P') * 8))"
    if ($LASTEXITCODE -ne 0 -or $version.Trim() -ne "3.12.10|64") {
        throw "CPython candidate is not Python 3.12.10 x64."
    }

    if (Test-Path -LiteralPath $runtime) {
        Move-Item -LiteralPath $runtime -Destination $runtimeBackup
    }
    Move-Item -LiteralPath $runtimeCandidate -Destination $runtime
    $runtimePublished = $true
    $runtimePython = Join-Path $runtime "python.exe"

    & $runtimePython -I -m venv $environmentCandidate
    if ($LASTEXITCODE -ne 0) {
        throw "Creating the training environment failed with exit code $LASTEXITCODE."
    }
    $candidateEnvironmentPython = Join-Path $environmentCandidate "Scripts\python.exe"
    & $candidateEnvironmentPython -I -m pip install --disable-pip-version-check `
        --no-index --find-links $wheelhouse --require-hashes --requirement $lockPath
    if ($LASTEXITCODE -ne 0) {
        throw "Hash-locked dependency installation failed with exit code $LASTEXITCODE."
    }

    Assert-Inventory -Python $candidateEnvironmentPython -InventoryPath $inventoryPath
    New-Item -ItemType Directory -Path $CheckOutput -Force | Out-Null
    & $candidateEnvironmentPython -I -m droplet_trainer env-check --device auto `
        --require-training --require-onnx --check-output ([System.IO.Path]::GetFullPath($CheckOutput)) --json
    if ($LASTEXITCODE -ne 0) {
        throw "Isolated droplet-trainer environment check failed with exit code $LASTEXITCODE."
    }

    if (Test-Path -LiteralPath $environment) {
        Move-Item -LiteralPath $environment -Destination $environmentBackup
    }
    Move-Item -LiteralPath $environmentCandidate -Destination $environment
    $environmentPublished = $true

    Remove-ScopedDirectory -Path $runtimeBackup -AllowedRoot $install
    Remove-ScopedDirectory -Path $environmentBackup -AllowedRoot $install
    Write-Host "OpenDSS training runtime provisioned: $(Join-Path $environment 'Scripts\python.exe')"
} catch {
    Remove-ScopedDirectory -Path $environmentCandidate -AllowedRoot $install
    if ($environmentPublished) {
        Remove-ScopedDirectory -Path $environment -AllowedRoot $install
    }
    if (Test-Path -LiteralPath $environmentBackup) {
        Move-Item -LiteralPath $environmentBackup -Destination $environment
    }
    if ($runtimePublished) {
        Remove-ScopedDirectory -Path $runtime -AllowedRoot $install
    } else {
        Remove-ScopedDirectory -Path $runtimeCandidate -AllowedRoot $install
    }
    if (Test-Path -LiteralPath $runtimeBackup) {
        Move-Item -LiteralPath $runtimeBackup -Destination $runtime
    }
    throw
} finally {
    Remove-ScopedDirectory -Path $downloadCandidate -AllowedRoot $install
    $env:PYTHONPATH = $oldPythonPath
    $env:PYTHONHOME = $oldPythonHome
    $env:PYTHONNOUSERSITE = $oldPythonNoUserSite
    $env:PIP_NO_INDEX = $oldPipNoIndex
}
