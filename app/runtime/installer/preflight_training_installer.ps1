[CmdletBinding()]
param(
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$TrainerWheelPath = $env:OPENDSS_TRAINER_WHEEL,
    [string]$PackageDir = "",
    [string]$AcceptedExecutablePath = "",
    [ValidatePattern("^$|^[0-9A-Fa-f]{64}$")]
    [string]$AcceptedExecutableSha256 = "",
    [string]$InnoSetup = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    [string]$EvidencePath = (
        Join-Path $env:TEMP "opendss-training-installer-preflight.json"
    ),
    [switch]$RequireCompleteInputs
)

$ErrorActionPreference = "Stop"
$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$repo = (Resolve-Path -LiteralPath (Join-Path $source "..\..")).Path
$training = Join-Path $repo "training\python"
$catalogPath = Join-Path $training (
    "requirements\windows-py312-gpu-cu130-downloads.json")
$lockPath = Join-Path $training "requirements\windows-py312-gpu-cu130.lock"
$inventoryPath = Join-Path $training (
    "requirements\windows-py312-gpu-cu130-inventory.json")
$provisionerPath = Join-Path $training (
    "scripts\windows\provision-training-runtime.ps1")
$wheelBuilderPath = Join-Path $training (
    "scripts\windows\build-training-bootstrap-wheel.ps1")
$packagePath = Join-Path $source "scripts\package_portable.ps1"
$checkPath = Join-Path $source "scripts\check_package.ps1"
$buildPath = Join-Path $source "installer\build_installer.ps1"
$issPath = Join-Path $source "installer\installer.iss"

$checks = [System.Collections.ArrayList]::new()
$errors = [System.Collections.ArrayList]::new()
$remaining = [System.Collections.ArrayList]::new()
function Add-Result {
    param([string]$Name, [string]$Status, [string]$Detail)
    [void]$checks.Add([ordered]@{
        name = $Name
        status = $Status
        detail = $Detail
    })
}
function Add-Failure {
    param([string]$Name, [string]$Detail)
    Add-Result -Name $Name -Status "fail" -Detail $Detail
    [void]$errors.Add($Detail)
}
function Add-Remaining {
    param([string]$Name, [string]$Detail)
    Add-Result -Name $Name -Status "provisional" -Detail $Detail
    [void]$remaining.Add($Detail)
}

foreach ($script in @(
    $wheelBuilderPath, $provisionerPath, $packagePath, $checkPath, $buildPath
)) {
    $tokens = $null
    $parseErrors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        $script, [ref]$tokens, [ref]$parseErrors) | Out-Null
    if ($parseErrors.Count) {
        Add-Failure -Name "PowerShell parser" -Detail (
            "${script}: " + (($parseErrors | ForEach-Object Message) -join "; "))
    } else {
        Add-Result -Name "PowerShell parser" -Status "pass" -Detail $script
    }
}

if (-not $AcceptedExecutablePath -or -not $AcceptedExecutableSha256) {
    Add-Remaining -Name "accepted v2 executable" -Detail (
        "Supply AcceptedExecutablePath and AcceptedExecutableSha256 for byte-identity validation.")
} elseif (-not (Test-Path -LiteralPath $AcceptedExecutablePath -PathType Leaf)) {
    Add-Failure -Name "accepted v2 executable" -Detail (
        "Accepted executable does not exist: $AcceptedExecutablePath")
} else {
    $acceptedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $AcceptedExecutablePath).Hash
    if ($acceptedHash -ne $AcceptedExecutableSha256.ToUpperInvariant()) {
        Add-Failure -Name "accepted v2 executable" -Detail (
            "Expected SHA-256 $($AcceptedExecutableSha256.ToUpperInvariant()); got $acceptedHash.")
    } else {
        Add-Result -Name "accepted v2 executable" -Status "pass" -Detail (
            "$AcceptedExecutablePath SHA-256 $acceptedHash")
    }
}

try {
    $catalog = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
    $inventory = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json
    if ([string]$catalog.schema_version -ne
        "opendss-training-bootstrap-downloads-v1") {
        throw "Unexpected catalog schema."
    }
    if (@($catalog.wheels).Count -ne 36) {
        throw "Expected exactly 36 third-party wheel URLs."
    }
    if ((Get-Content -LiteralPath $catalogPath -Raw) -match '"sha256"\s*:') {
        throw "Catalog duplicates authoritative hashes."
    }
    if ([string]$catalog.python.filename -ne
        [string]$inventory.python.installer_file) {
        throw "CPython filename differs from inventory."
    }
    $pythonUri = [Uri]([string]$catalog.python.url)
    if ($pythonUri.Scheme -ne "https" -or
        $pythonUri.DnsSafeHost -ne "www.python.org" -or
        [Uri]::UnescapeDataString($pythonUri.Segments[-1]) -ne
            [string]$catalog.python.filename) {
        throw "CPython URL is not the exact official source."
    }

    $allowedHosts = @(
        "files.pythonhosted.org",
        "download.pytorch.org",
        "download-r2.pytorch.org"
    )
    $filenames = @()
    foreach ($wheel in @($catalog.wheels)) {
        $uri = [Uri]([string]$wheel.url)
        $filename = [string]$wheel.filename
        if ($uri.Scheme -ne "https" -or
            $allowedHosts -notcontains $uri.DnsSafeHost.ToLowerInvariant() -or
            [Uri]::UnescapeDataString($uri.Segments[-1]) -ne $filename) {
            throw "Unapproved wheel URL: $($wheel.url)"
        }
        $filenames += $filename
    }
    $filenames += [string]$catalog.embedded_wheel
    if ($filenames.Count -ne 37 -or
        @($filenames | Select-Object -Unique).Count -ne 37) {
        throw "Catalog must define 37 unique wheel filenames."
    }

    $lockEntries = @()
    foreach ($line in Get-Content -LiteralPath $lockPath) {
        if ($line -notmatch "==") {
            continue
        }
        if ($line -notmatch
            "^\s*([A-Za-z0-9_.-]+)==([^\s]+)\s+--hash=sha256:([0-9a-fA-F]{64})\s*$") {
            throw "Unsupported lock line: $line"
        }
        $lockEntries += [pscustomobject]@{
            name = $Matches[1]
            version = $Matches[2]
            sha256 = $Matches[3].ToLowerInvariant()
        }
    }
    if ($lockEntries.Count -ne 37) {
        throw "Authoritative lock must contain exactly 37 entries."
    }
    $lockedByFile = @{}
    foreach ($entry in $lockEntries) {
        $normalized = ([string]$entry.name).ToLowerInvariant() -replace "[-.]+", "_"
        $prefix = "$normalized-$(([string]$entry.version).ToLowerInvariant())-"
        $matches = @($filenames | Where-Object {
            $_.ToLowerInvariant().StartsWith(
                $prefix, [System.StringComparison]::Ordinal)
        })
        if ($matches.Count -ne 1) {
            throw "$($entry.name)==$($entry.version) maps to $($matches.Count) files."
        }
        $lockedByFile[$matches[0].ToLowerInvariant()] = [string]$entry.sha256
    }
    if ($lockedByFile.Count -ne 37) {
        throw "Catalog and lock are not one-to-one."
    }
    Add-Result -Name "catalog and lock" -Status "pass" -Detail (
        "36 approved HTTPS URLs plus one embedded wheel map one-to-one to 37 hashes.")

    $embeddedName = [string]$catalog.embedded_wheel
    $expectedEmbeddedHash = $lockedByFile[$embeddedName.ToLowerInvariant()]
    if (-not $TrainerWheelPath) {
        Add-Remaining -Name "embedded trainer wheel" -Detail (
            "Build $embeddedName with build-training-bootstrap-wheel.ps1; expected SHA-256 $expectedEmbeddedHash.")
    } elseif (-not (Test-Path -LiteralPath $TrainerWheelPath -PathType Leaf)) {
        Add-Failure -Name "embedded trainer wheel" -Detail (
            "Trainer wheel does not exist: $TrainerWheelPath")
    } else {
        $actualName = Split-Path -Leaf $TrainerWheelPath
        $actualHash = (Get-FileHash -LiteralPath $TrainerWheelPath -Algorithm SHA256).
            Hash.ToLowerInvariant()
        if ($actualName -ne $embeddedName -or $actualHash -ne $expectedEmbeddedHash) {
            Add-Failure -Name "embedded trainer wheel" -Detail (
                "Expected $embeddedName SHA-256 $expectedEmbeddedHash; got $actualName SHA-256 $actualHash.")
        } else {
            Add-Result -Name "embedded trainer wheel" -Status "pass" -Detail (
                "$actualName SHA-256 $actualHash")
        }
    }

    if (-not $PackageDir) {
        Add-Remaining -Name "staged package" -Detail (
            "Portable staging/check and installer compile remain external gates.")
    } else {
        $staged = (Resolve-Path -LiteralPath $PackageDir).Path
        foreach ($pair in @(
            @($catalogPath, (Join-Path $staged "training\bootstrap\$(Split-Path -Leaf $catalogPath)")),
            @($lockPath, (Join-Path $staged "training\bootstrap\$(Split-Path -Leaf $lockPath)")),
            @($inventoryPath, (Join-Path $staged "training\bootstrap\$(Split-Path -Leaf $inventoryPath)")),
            @($provisionerPath, (Join-Path $staged "training\python\scripts\windows\provision-training-runtime.ps1"))
        )) {
            if ((Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash -ne
                (Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash) {
                throw "Staged bootstrap differs from source: $($pair[1])"
            }
        }
        Add-Result -Name "staged package" -Status "pass" -Detail $staged
    }
} catch {
    Add-Failure -Name "bootstrap contract" -Detail $_.Exception.Message
}

try {
    $provisioner = Get-Content -LiteralPath $provisionerPath -Raw
    foreach ($marker in @(
        ".candidate-", ".backup-", "Move-Item -LiteralPath `$runtimeCandidate",
        "Move-Item -LiteralPath `$environmentCandidate", "} catch {",
        "Move-Item -LiteralPath `$runtimeBackup -Destination `$runtime",
        "Move-Item -LiteralPath `$environmentBackup -Destination `$environment",
        "Remove-ScopedDirectory"
    )) {
        if (-not $provisioner.Contains($marker)) {
            throw "Transactional provisioner marker is absent: $marker"
        }
    }
    $installer = Get-Content -LiteralPath $issPath -Raw
    $stageMarkers = @(
        "StageWelcome = 'Welcome'",
        "StagePrerequisiteCheck = 'Prerequisite Check'",
        "StageOpenDssInstallation = 'OpenDSS installation'",
        "StageTrainingEnvironmentSetup = 'Training Environment setup'",
        "StageFinalVerification = 'Final Verification'"
    )
    $previousStageIndex = -1
    foreach ($marker in $stageMarkers) {
        $stageIndex = $installer.IndexOf(
            $marker, [System.StringComparison]::Ordinal)
        if ($stageIndex -le $previousStageIndex) {
            throw "Installer stage is absent or out of order: $marker"
        }
        $previousStageIndex = $stageIndex
    }
    foreach ($marker in @(
        "dcamapi.dll",
        "nicaiu.dll",
        "nvcuda.dll",
        "WinHttp.WinHttpRequest.5.1",
        "VcRedistRequired",
        "Driver Required",
        "Check Again",
        "https://www.hamamatsu.com/us/en/product/cameras/software/driver-software/dcam-api-for-windows.html",
        "https://www.ni.com/en/support/downloads/drivers/download.ni-daq-mx.html/",
        "RunTrainingProvisioner",
        "previous accepted runtime, if any, was preserved",
        "Repair Training Environment",
        "Training: Unavailable",
        "ProbeTrainingCompute",
        "Training compute: ",
        "CUDA",
        "CPU fallback"
    )) {
        if (-not $installer.Contains($marker)) {
            throw "Installer lifecycle marker is absent: $marker"
        }
    }
    foreach ($forbiddenMarker in @(
        "RaiseException('The OpenDSS training bootstrap could not be started.')",
        "RaiseException(Format('The OpenDSS training runtime download or verification failed"
    )) {
        if ($installer.Contains($forbiddenMarker)) {
            throw "Training failure still raises an installer-rollback exception: $forbiddenMarker"
        }
    }
    $packageScript = Get-Content -LiteralPath $packagePath -Raw
    $checkScript = Get-Content -LiteralPath $checkPath -Raw
    foreach ($marker in @(
        "AcceptedExecutablePath",
        "AcceptedExecutableSha256",
        "Packaged OpenDSS.exe differs from the accepted v2 executable"
    )) {
        if (-not $packageScript.Contains($marker)) {
            throw "Accepted executable packaging marker is absent: $marker"
        }
    }
    if (-not $checkScript.Contains("two local weights per architecture")) {
        throw "Exact bundled-weight validation marker is absent."
    }
    Add-Result -Name "installer lifecycle and packaging contract" `
        -Status "pass" -Detail (
            "Five stages are ordered; prerequisites/actions, recoverable Training, factual final verification, accepted EXE identity, and two weights per architecture are present.")
} catch {
    Add-Failure -Name "installer lifecycle and packaging contract" -Detail $_.Exception.Message
}

if (Test-Path -LiteralPath $InnoSetup -PathType Leaf) {
    $innoFile = Get-Item -LiteralPath $InnoSetup
    $innoSignature = Get-AuthenticodeSignature -LiteralPath $InnoSetup
    if ($innoFile.VersionInfo.FileDescription -ne
            "Inno Setup Command-Line Compiler" -or
        $innoSignature.Status -ne
            [System.Management.Automation.SignatureStatus]::Valid -or
        $innoSignature.SignerCertificate.Subject -notmatch "Pyrsys B[.]V[.]") {
        Add-Failure -Name "Inno Setup prerequisite" -Detail (
            "Inno compiler is not the valid Pyrsys-signed command-line compiler.")
    } else {
        Add-Result -Name "Inno Setup prerequisite" -Status "pass" -Detail (
            "$InnoSetup; signature $($innoSignature.Status); signer $($innoSignature.SignerCertificate.Subject)")
    }
} else {
    Add-Remaining -Name "Inno Setup prerequisite" -Detail (
        "Inno Setup 6 compiler is required: $InnoSetup")
}

if ($RequireCompleteInputs -and $remaining.Count) {
    [void]$errors.Add("Complete preflight was requested but external gates remain.")
}
$status = if ($errors.Count) {
    "fail"
} elseif ($remaining.Count) {
    "provisional"
} else {
    "pass"
}
$evidence = [ordered]@{
    schema_version = "opendss-training-installer-preflight-v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("o")
    status = $status
    source_root = $source
    checks = $checks
    remaining_external_gates = $remaining
    errors = $errors
}
$evidenceDirectory = Split-Path -Parent $EvidencePath
if ($evidenceDirectory) {
    New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
}
$evidence | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $EvidencePath -Encoding UTF8
Write-Output $EvidencePath
if ($errors.Count) {
    exit 1
}
exit 0
