[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BootstrapRoot,
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA "OpenDSS"),
    [string]$CheckOutput = (Join-Path $env:LOCALAPPDATA "OpenDSS\training-runtime-check"),
    [ValidateSet("Auto", "cpu", "cuda")]
    [string]$ComputeProfile = "Auto"
)

$ErrorActionPreference = "Stop"
$PythonVersion = "3.12.10"
$RuntimeName = "python-$PythonVersion"
try {
    $Host.UI.RawUI.WindowTitle = "OpenDSS Training Setup"
} catch {
    # Non-console hosts do not expose a writable window title.
}

function Test-CudaAvailable {
    $nvidiaSmi = Get-Command "nvidia-smi.exe" -ErrorAction SilentlyContinue
    $nvidiaSmiPath = if ($nvidiaSmi) { $nvidiaSmi.Source } else { "" }
    if (-not $nvidiaSmiPath) {
        $standardPath = Join-Path $env:ProgramFiles (
            "NVIDIA Corporation\NVSMI\nvidia-smi.exe")
        if (Test-Path -LiteralPath $standardPath -PathType Leaf) {
            $nvidiaSmiPath = $standardPath
        }
    }
    if (-not $nvidiaSmiPath) {
        return $false
    }
    try {
        $driver = & $nvidiaSmiPath --query-gpu=driver_version `
            --format=csv,noheader 2>$null
        if ($LASTEXITCODE -ne 0 -or
            [string]::IsNullOrWhiteSpace((@($driver) -join "").Trim())) {
            return $false
        }
        $status = & $nvidiaSmiPath 2>$null
        if ($LASTEXITCODE -ne 0 -or
            (@($status) -join "`n") -notmatch
                "CUDA Version:\s*([0-9]+(?:[.][0-9]+)?)") {
            return $false
        }
        return [version]$Matches[1] -ge [version]"13.0"
    } catch {
        return $false
    }
}

function Get-VerifiedDownload {
    param(
        [string]$Uri,
        [string]$Destination,
        [string]$ExpectedSha256,
        [string]$Description
    )
    if (Test-Path -LiteralPath $Destination -PathType Leaf) {
        $cachedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Destination).
            Hash.ToLowerInvariant()
        if ($cachedHash -eq $ExpectedSha256.ToLowerInvariant()) {
            Write-Host (
                "Reusing verified ${Description}: " +
                (Split-Path -Leaf $Destination))
            return
        }
        Remove-Item -LiteralPath $Destination -Force
    }

    $partial = "$Destination.partial-$([Guid]::NewGuid().ToString('N'))"
    try {
        Write-Host (
            "Downloading ${Description}: " +
            (Split-Path -Leaf $Destination))
        Invoke-WebRequest -Uri $Uri -OutFile $partial -UseBasicParsing
        $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $partial).
            Hash.ToLowerInvariant()
        if ($actualHash -ne $ExpectedSha256.ToLowerInvariant()) {
            throw "Downloaded $Description hash does not match the accepted hash."
        }
        Move-Item -LiteralPath $partial -Destination $Destination
    } finally {
        if (Test-Path -LiteralPath $partial) {
            Remove-Item -LiteralPath $partial -Force
        }
    }
}

function Test-PathWithinRoot {
    param([string]$Path, [string]$Root)
    if (-not $Path) {
        return $false
    }
    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $resolvedRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd("\") + "\"
    return $resolvedPath.StartsWith(
        $resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)
}

function Test-ExactPython {
    param([string]$Python)
    if (-not (Test-Path -LiteralPath $Python -PathType Leaf)) {
        return $false
    }
    try {
        $version = & $Python -I -c (
            "import platform,struct;" +
            "print(platform.python_version()+'|'+str(struct.calcsize('P')*8))")
        return $LASTEXITCODE -eq 0 -and
            (@($version) -join "").Trim() -eq "3.12.10|64"
    } catch {
        return $false
    }
}

function Get-RegisteredPythonRoot {
    foreach ($registryPath in @(
        "HKCU:\Software\Python\PythonCore\3.12\InstallPath",
        "HKLM:\Software\Python\PythonCore\3.12\InstallPath",
        "HKLM:\Software\WOW6432Node\Python\PythonCore\3.12\InstallPath"
    )) {
        if (-not (Test-Path -LiteralPath $registryPath)) {
            continue
        }
        $registeredRoot =
            [string](Get-Item -LiteralPath $registryPath).GetValue("")
        if ($registeredRoot) {
            return [System.IO.Path]::GetFullPath($registeredRoot)
        }
    }
    return $null
}

function Set-OwnedPythonRegistration {
    param([string]$RuntimeRoot)
    if (-not (Test-PathWithinRoot -Path $RuntimeRoot -Root $install)) {
        throw "Refusing to register Python outside InstallRoot: $RuntimeRoot"
    }
    $pythonCore = "HKCU:\Software\Python\PythonCore\3.12"
    $installPath = Join-Path $pythonCore "InstallPath"
    $pythonPath = Join-Path $pythonCore "PythonPath"
    New-Item -Path $installPath -Force | Out-Null
    New-Item -Path $pythonPath -Force | Out-Null
    Set-Item -LiteralPath $installPath -Value ($RuntimeRoot.TrimEnd("\") + "\")
    Set-ItemProperty -LiteralPath $installPath -Name "ExecutablePath" `
        -Value (Join-Path $RuntimeRoot "python.exe")
    Set-ItemProperty -LiteralPath $installPath -Name "WindowedExecutablePath" `
        -Value (Join-Path $RuntimeRoot "pythonw.exe")
    Set-Item -LiteralPath $pythonPath -Value (
        (Join-Path $RuntimeRoot "Lib") + ";" +
        (Join-Path $RuntimeRoot "DLLs"))
    Set-ItemProperty -LiteralPath $pythonCore -Name "DisplayName" `
        -Value "Python 3.12 (64-bit)"
    Set-ItemProperty -LiteralPath $pythonCore -Name "Version" -Value "3.12.10"
    Set-ItemProperty -LiteralPath $pythonCore -Name "SysVersion" -Value "3.12"
    Set-ItemProperty -LiteralPath $pythonCore -Name "SysArchitecture" `
        -Value "64bit"
}

function Remove-OwnedPythonRegistration {
    $pythonCore = "HKCU:\Software\Python\PythonCore\3.12"
    $registeredRoot = Get-RegisteredPythonRoot
    if ($registeredRoot -and
        (Test-PathWithinRoot -Path $registeredRoot -Root $install) -and
        (Test-Path -LiteralPath $pythonCore)) {
        Remove-Item -LiteralPath $pythonCore -Recurse -Force
    }
}

function Test-PythonBundleInstalled {
    $uninstallRoot =
        "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall"
    if (-not (Test-Path -LiteralPath $uninstallRoot)) {
        return $false
    }
    foreach ($key in Get-ChildItem -LiteralPath $uninstallRoot) {
        $displayName = [string](Get-ItemPropertyValue -LiteralPath $key.PSPath `
            -Name "DisplayName" -ErrorAction SilentlyContinue)
        if ($displayName -eq "Python 3.12.10 (64-bit)") {
            return $true
        }
    }
    return $false
}

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

function Remove-ScopedFile {
    param([string]$Path, [string]$AllowedRoot)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    if (-not (Test-PathWithinRoot -Path $Path -Root $AllowedRoot)) {
        throw "Refusing to remove file outside InstallRoot: $Path"
    }
    Remove-Item -LiteralPath $Path -Force
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
    $parsedActual = $actualJson | ConvertFrom-Json
    foreach ($entry in @($parsedActual)) {
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
    $expectedOnnxRuntime = [string]$inventory.runtime.onnxruntime_distribution
    if (-not $actual.ContainsKey($expectedOnnxRuntime) -or
        ($expectedOnnxRuntime -eq "onnxruntime" -and
            $actual.ContainsKey("onnxruntime-gpu")) -or
        ($expectedOnnxRuntime -eq "onnxruntime-gpu" -and
            $actual.ContainsKey("onnxruntime"))) {
        throw "Exactly the $expectedOnnxRuntime distribution must be installed."
    }
}

$bootstrap = [System.IO.Path]::GetFullPath($BootstrapRoot)
$install = [System.IO.Path]::GetFullPath($InstallRoot)
if ($install.Length -lt 8 -or [System.IO.Path]::GetPathRoot($install) -eq $install) {
    throw "InstallRoot is too broad: $install"
}

$selectedProfile = $ComputeProfile.ToLowerInvariant()
if ($selectedProfile -eq "auto") {
    $selectedProfile = if (Test-CudaAvailable) { "cuda" } else { "cpu" }
}
$profileStem = if ($selectedProfile -eq "cuda") {
    "windows-py312-gpu-cu130"
} else {
    "windows-py312-cpu"
}
$EnvironmentName = if ($selectedProfile -eq "cuda") {
    "training-venv-gpu"
} else {
    "training-venv-cpu"
}
Write-Host (
    "Selected $selectedProfile training profile; environment: $EnvironmentName")

$catalogPath = Join-Path $bootstrap "windows-py312-gpu-cu130-downloads.json"
$lockPath = Join-Path $bootstrap "$profileStem.lock"
$inventoryPath = Join-Path $bootstrap "$profileStem-inventory.json"
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
$downloadEntries = @($catalog.wheels)
if ($selectedProfile -eq "cpu") {
    $cpuOverrides = @{
        "onnxruntime_gpu-1.25.1-cp312-cp312-win_amd64.whl" =
            [pscustomobject]@{
                filename = "onnxruntime-1.25.1-cp312-cp312-win_amd64.whl"
                url = "https://files.pythonhosted.org/packages/89/36/b4f3eb5e95c66389aafd490950b5255e87c9333742cf90516eb50898e1dc/onnxruntime-1.25.1-cp312-cp312-win_amd64.whl"
            }
        "torch-2.10.0+cu130-cp312-cp312-win_amd64.whl" =
            [pscustomobject]@{
                filename = "torch-2.10.0+cpu-cp312-cp312-win_amd64.whl"
                url = "https://download-r2.pytorch.org/whl/cpu/torch-2.10.0%2Bcpu-cp312-cp312-win_amd64.whl"
            }
        "torchvision-0.25.0+cu130-cp312-cp312-win_amd64.whl" =
            [pscustomobject]@{
                filename = "torchvision-0.25.0+cpu-cp312-cp312-win_amd64.whl"
                url = "https://download-r2.pytorch.org/whl/cpu/torchvision-0.25.0%2Bcpu-cp312-cp312-win_amd64.whl"
            }
    }
    $downloadEntries = @($downloadEntries | ForEach-Object {
        $override = $cpuOverrides[[string]$_.filename]
        if ($override) { $override } else { $_ }
    })
}

$embeddedWheel = Join-Path $bootstrap ([string]$catalog.embedded_wheel)
if (-not (Test-Path -LiteralPath $embeddedWheel -PathType Leaf)) {
    throw "Embedded OpenDSS trainer wheel is missing: $embeddedWheel"
}

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
if ([string]$inventory.runtime.environment_name -ne $EnvironmentName) {
    throw "Inventory environment name differs from the selected compute profile."
}

New-Item -ItemType Directory -Path $install -Force | Out-Null
$token = [Guid]::NewGuid().ToString("N")
$downloadCache = Join-Path $install "training-download-cache\$profileStem"
$wheelhouse = Join-Path $downloadCache "wheelhouse"
$runtime = Join-Path $install $RuntimeName
$runtimeBackup = Join-Path $install ".$RuntimeName.backup-$token"
$environment = Join-Path $install $EnvironmentName
$environmentCandidate = Join-Path $install ".$EnvironmentName.candidate-$token"
$environmentBackup = Join-Path $install ".$EnvironmentName.backup-$token"
$selectionPath = Join-Path $install "training-runtime-selection.txt"
$selectionCandidate = Join-Path $install (
    ".training-runtime-selection.candidate-$token.txt")
$selectionBackup = Join-Path $install (
    ".training-runtime-selection.backup-$token.txt")
$runtimeVerified = $false
$runtimeWasBackedUp = $false
$installerStarted = $false
$bundleExistedBefore = Test-PythonBundleInstalled
$environmentPublished = $false
$selectionPublished = $false

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
    $runtimePython = Join-Path $runtime "python.exe"
    if (Test-ExactPython -Python $runtimePython) {
        Write-Host "Reusing verified installer-owned CPython $PythonVersion x64."
        $runtimeVerified = $true
    } else {
        if (Test-Path -LiteralPath $runtime) {
            Move-Item -LiteralPath $runtime -Destination $runtimeBackup
            $runtimeWasBackedUp = $true
        }

        $registeredRoot = Get-RegisteredPythonRoot
        $registeredPython = if ($registeredRoot) {
            Join-Path $registeredRoot "python.exe"
        } else {
            ""
        }
        if ($registeredRoot -and
            (Test-ExactPython -Python $registeredPython)) {
            Write-Host (
                "Copying the verified Python $PythonVersion x64 installation " +
                "into the isolated OpenDSS runtime.")
            Copy-Item -LiteralPath $registeredRoot -Destination $runtime -Recurse
        } else {
            if ($registeredRoot -and
                -not (Test-PathWithinRoot -Path $registeredRoot -Root $install)) {
                throw (
                    "A non-OpenDSS Python 3.12 registration is stale or is not " +
                    "Python 3.12.10 x64. OpenDSS will not modify it.")
            }

            $installer =
                Join-Path $downloadCache ([string]$catalog.python.filename)
            Get-VerifiedDownload -Uri ([string]$catalog.python.url) `
                -Destination $installer `
                -ExpectedSha256 ([string]$inventory.python.installer_sha256) `
                -Description "CPython $PythonVersion runtime"
            $installerSignature = Get-AuthenticodeSignature -LiteralPath $installer
            if ($installerSignature.Status -ne
                    [System.Management.Automation.SignatureStatus]::Valid -or
                $installerSignature.SignerCertificate.Subject -notmatch
                    "Python Software Foundation") {
                throw (
                    "Downloaded CPython installer does not have the accepted " +
                    "PSF signature.")
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
                "TargetDir=$runtime"
            )
            if ($registeredRoot) {
                Write-Host (
                    "Repairing the installer-owned Python registration at " +
                    "$registeredRoot...")
                $installerArguments = @("/repair", "/quiet", "/norestart")
            } elseif ($bundleExistedBefore) {
                throw (
                    "Python 3.12.10 is registered without a verifiable install " +
                    "path. OpenDSS will not repair an unowned global installation.")
            } else {
                Write-Host (
                    "Installing isolated CPython $PythonVersion x64 before " +
                    "acquiring training dependencies...")
            }
            $installerStarted = $true
            $installerProcess = Start-Process -FilePath $installer `
                -ArgumentList $installerArguments -Wait -PassThru `
                -WindowStyle Hidden
            if ($installerProcess.ExitCode -ne 0) {
                throw (
                    "CPython installation failed with exit code " +
                    "$($installerProcess.ExitCode).")
            }

            if (-not (Test-ExactPython -Python $runtimePython) -and
                $registeredRoot -and
                (Test-ExactPython -Python $registeredPython)) {
                Copy-Item -LiteralPath $registeredRoot -Destination $runtime `
                    -Recurse
            }
        }

        if (-not (Test-ExactPython -Python $runtimePython)) {
            throw "OpenDSS Python runtime is not Python 3.12.10 x64."
        }
        if ($installerStarted) {
            Set-OwnedPythonRegistration -RuntimeRoot $runtime
        }
        $runtimeVerified = $true
        Remove-ScopedDirectory -Path $runtimeBackup -AllowedRoot $install
        $runtimeWasBackedUp = $false
        Write-Host "Verified isolated CPython $PythonVersion x64."
    }

    Write-Host (
        "Downloading or verifying $($downloadEntries.Count) locked " +
        "$selectedProfile training dependencies...")
    foreach ($entry in $downloadEntries) {
        $destination = Join-Path $wheelhouse ([string]$entry.filename)
        Get-VerifiedDownload -Uri ([string]$entry.url) `
            -Destination $destination `
            -ExpectedSha256 (
                $lockedFiles[([string]$entry.filename).ToLowerInvariant()]) `
            -Description "training dependency"
    }
    $embeddedDestination =
        Join-Path $wheelhouse ([string]$catalog.embedded_wheel)
    Copy-Item -LiteralPath $embeddedWheel -Destination $embeddedDestination `
        -Force
    $embeddedHash =
        (Get-FileHash -Algorithm SHA256 -LiteralPath $embeddedDestination).
            Hash.ToLowerInvariant()
    if ($embeddedHash -ne
        $lockedFiles[([string]$catalog.embedded_wheel).ToLowerInvariant()]) {
        throw (
            "Embedded OpenDSS trainer wheel hash does not match the " +
            "authoritative lock.")
    }

    Write-Host "Creating isolated $EnvironmentName environment..."
    & $runtimePython -I -m venv $environmentCandidate
    if ($LASTEXITCODE -ne 0) {
        throw "Creating the training environment failed with exit code $LASTEXITCODE."
    }
    $candidateEnvironmentPython = Join-Path $environmentCandidate "Scripts\python.exe"
    Write-Host "Installing hash-locked training dependencies..."
    & $candidateEnvironmentPython -I -m pip install --disable-pip-version-check `
        --no-index --find-links $wheelhouse --require-hashes --requirement $lockPath
    if ($LASTEXITCODE -ne 0) {
        throw "Hash-locked dependency installation failed with exit code $LASTEXITCODE."
    }

    Write-Host "Verifying installed package inventory and compute readiness..."
    Assert-Inventory -Python $candidateEnvironmentPython -InventoryPath $inventoryPath
    New-Item -ItemType Directory -Path $CheckOutput -Force | Out-Null
    & $candidateEnvironmentPython -I -m droplet_trainer env-check `
        --device $selectedProfile `
        --require-training --require-onnx --check-output ([System.IO.Path]::GetFullPath($CheckOutput)) --json
    if ($LASTEXITCODE -ne 0) {
        throw "Isolated droplet-trainer environment check failed with exit code $LASTEXITCODE."
    }

    Write-Host "Publishing the verified $EnvironmentName environment..."
    if (Test-Path -LiteralPath $environment) {
        Move-Item -LiteralPath $environment -Destination $environmentBackup
    }
    Move-Item -LiteralPath $environmentCandidate -Destination $environment
    $environmentPublished = $true

    [System.IO.File]::WriteAllText(
        $selectionCandidate,
        $EnvironmentName,
        [System.Text.UTF8Encoding]::new($false))
    if (Test-Path -LiteralPath $selectionPath -PathType Leaf) {
        Move-Item -LiteralPath $selectionPath -Destination $selectionBackup
    }
    Move-Item -LiteralPath $selectionCandidate -Destination $selectionPath
    $selectionPublished = $true

    Remove-ScopedDirectory -Path $environmentBackup -AllowedRoot $install
    Remove-ScopedFile -Path $selectionBackup -AllowedRoot $install
    Write-Host "OpenDSS training runtime provisioned: $(Join-Path $environment 'Scripts\python.exe')"
} catch {
    Remove-ScopedFile -Path $selectionCandidate -AllowedRoot $install
    if ($selectionPublished) {
        Remove-ScopedFile -Path $selectionPath -AllowedRoot $install
    }
    if (Test-Path -LiteralPath $selectionBackup -PathType Leaf) {
        Move-Item -LiteralPath $selectionBackup -Destination $selectionPath
    }
    Remove-ScopedDirectory -Path $environmentCandidate -AllowedRoot $install
    if ($environmentPublished) {
        Remove-ScopedDirectory -Path $environment -AllowedRoot $install
    }
    if (Test-Path -LiteralPath $environmentBackup) {
        Move-Item -LiteralPath $environmentBackup -Destination $environment
    }
    if (-not $runtimeVerified -and
        (Test-Path -LiteralPath $runtime)) {
        Remove-ScopedDirectory -Path $runtime -AllowedRoot $install
    }
    if ($runtimeWasBackedUp -and
        (Test-Path -LiteralPath $runtimeBackup)) {
        Move-Item -LiteralPath $runtimeBackup -Destination $runtime
    }
    if (-not $runtimeVerified -and $installerStarted -and
        -not $bundleExistedBefore) {
        $uninstallProcess = Start-Process -FilePath $installer `
            -ArgumentList @("/uninstall", "/quiet", "/norestart") `
            -Wait -PassThru -WindowStyle Hidden
        if ($uninstallProcess.ExitCode -ne 0) {
            Write-Warning (
                "Owned CPython rollback returned exit code " +
                "$($uninstallProcess.ExitCode).")
        }
        Remove-OwnedPythonRegistration
    }
    throw
} finally {
    $env:PYTHONPATH = $oldPythonPath
    $env:PYTHONHOME = $oldPythonHome
    $env:PYTHONNOUSERSITE = $oldPythonNoUserSite
    $env:PIP_NO_INDEX = $oldPipNoIndex
}
