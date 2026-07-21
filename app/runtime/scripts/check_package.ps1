param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$NiInstaller = "",
    [string]$VcRedist = "",
    [switch]$RequireVcRedist,
    [switch]$RequireInstallerInputs,
    [switch]$RequireExternalRuntimes,
    [switch]$WriteManifest,
    [string]$ManifestPath = "",
    [string]$ExpectedOnnxDir = "C:\onnxruntime-gpu",
    [string]$MinimumOnnxRuntimeMajorMinor = "1.23"
)

$ErrorActionPreference = "Stop"

function Add-CheckResult {
    param(
        [System.Collections.ArrayList]$List,
        [string]$Name,
        [string]$Status,
        [string]$Path,
        [string]$Detail = ""
    )

    [void]$List.Add([ordered]@{
        name = $Name
        status = $Status
        path = $Path
        detail = $Detail
    })
}

function Test-RequiredPath {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Name,
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        Add-CheckResult -List $List -Name $Name -Status "pass" -Path $Path
        return $true
    }

    Add-CheckResult -List $List -Name $Name -Status "fail" -Path $Path -Detail "Missing required package file."
    [void]$Errors.Add("Missing required package file: $Path")
    return $false
}

function Test-ExcludedPath {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Name,
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        $message = "Unexpected packaged artifact present: $Path"
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $Path -Detail $message
        [void]$Errors.Add($message)
        return $false
    }

    Add-CheckResult -List $List -Name $Name -Status "pass" -Path $Path -Detail "Excluded from portable package."
    return $true
}

function Test-OptionalPrerequisite {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [System.Collections.ArrayList]$Warnings,
        [string]$Name,
        [string]$Path,
        [bool]$Required
    )

    if ($Path -and (Test-Path -LiteralPath $Path)) {
        Add-CheckResult -List $List -Name $Name -Status "pass" -Path $Path
        return
    }

    $message = "Missing prerequisite input: $Name ($Path)"
    if ($Required) {
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $Path -Detail $message
        [void]$Errors.Add($message)
    } else {
        Add-CheckResult -List $List -Name $Name -Status "warn" -Path $Path -Detail $message
        [void]$Warnings.Add($message)
    }
}

function Test-Hash {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Name,
        [string]$Path,
        [string]$Expected
    )

    if (-not $Expected) {
        Add-CheckResult -List $List -Name $Name -Status "skip" -Path $Path -Detail "No expected SHA256 declared."
        return
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
    if ($actual -eq $Expected.ToLowerInvariant()) {
        Add-CheckResult -List $List -Name $Name -Status "pass" -Path $Path -Detail "SHA256 matched."
    } else {
        $message = "SHA256 mismatch for $Path. Expected $Expected, got $actual."
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $Path -Detail $message
        [void]$Errors.Add($message)
    }
}

function Get-FileVersionString {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }
    $item = Get-Item -LiteralPath $Path
    return [string]$item.VersionInfo.FileVersion
}

function Test-OnnxRuntimePackage {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [System.Collections.ArrayList]$Warnings,
        [string]$PackageDll,
        [string]$ExpectedDll,
        [string]$MinimumMajorMinor
    )

    if (-not (Test-Path -LiteralPath $PackageDll)) {
        return
    }

    $packageVersion = Get-FileVersionString -Path $PackageDll
    $packageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PackageDll).Hash.ToLowerInvariant()
    $detail = "FileVersion=$packageVersion; SHA256=$packageHash"
    if ($MinimumMajorMinor -and -not $packageVersion.StartsWith($MinimumMajorMinor + ".", [System.StringComparison]::OrdinalIgnoreCase)) {
        $message = "Package onnxruntime.dll version $packageVersion does not match required $MinimumMajorMinor.x."
        Add-CheckResult -List $List -Name "ONNX Runtime version" -Status "fail" -Path $PackageDll -Detail $message
        [void]$Errors.Add($message)
    } else {
        Add-CheckResult -List $List -Name "ONNX Runtime version" -Status "pass" -Path $PackageDll -Detail $detail
    }

    if ($ExpectedDll -and (Test-Path -LiteralPath $ExpectedDll)) {
        $expectedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExpectedDll).Hash.ToLowerInvariant()
        if ($packageHash -eq $expectedHash) {
            Add-CheckResult -List $List -Name "ONNX Runtime source hash" -Status "pass" -Path $PackageDll -Detail "Matches $ExpectedDll"
        } else {
            $message = "Package onnxruntime.dll hash does not match expected runtime. Expected $expectedHash from $ExpectedDll, got $packageHash."
            Add-CheckResult -List $List -Name "ONNX Runtime source hash" -Status "fail" -Path $PackageDll -Detail $message
            [void]$Errors.Add($message)
        }
    } else {
        $message = "Expected ONNX Runtime source DLL not found: $ExpectedDll"
        Add-CheckResult -List $List -Name "ONNX Runtime source hash" -Status "warn" -Path $ExpectedDll -Detail $message
        [void]$Warnings.Add($message)
    }
}

function Test-VcRedist {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Path,
        [bool]$Required
    )
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) {
        if ($Required) {
            Add-CheckResult -List $List -Name "Microsoft VC++ x64 runtime payload" -Status "fail" -Path $Path -Detail "Required installer payload is missing."
            [void]$Errors.Add("Required Microsoft VC++ x64 runtime payload is missing: $Path")
        }
        return
    }
    $file = Get-Item -LiteralPath $Path
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    $validMicrosoft = $file.Length -gt 0 -and
        $signature.Status -eq [System.Management.Automation.SignatureStatus]::Valid -and
        $signature.SignerCertificate.Subject -match '(^|, )O=Microsoft Corporation(,|$)'
    if (-not $validMicrosoft) {
        Add-CheckResult -List $List -Name "Microsoft VC++ x64 runtime payload" -Status "fail" -Path $Path -Detail "Payload is empty, unsigned, untrusted, or not signed by Microsoft."
        [void]$Errors.Add("Invalid Microsoft VC++ x64 runtime payload: $Path")
        return
    }
    $detail = "Version=$($file.VersionInfo.ProductVersion); SHA256=$((Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash)"
    Add-CheckResult -List $List -Name "Microsoft VC++ x64 runtime payload" -Status "pass" -Path $Path -Detail $detail
}

function Test-JsonFile {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Name,
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    try {
        Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json | Out-Null
        Add-CheckResult -List $List -Name $Name -Status "pass" -Path $Path -Detail "Valid JSON."
    } catch {
        $message = "Invalid JSON: $Path"
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $Path -Detail $message
        [void]$Errors.Add($message)
    }
}

function Test-SimpleModelPackageContract {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Name,
        $Entry,
        [string]$PackageDir
    )

    $issues = New-Object System.Collections.Generic.List[string]
    $entryFields = @($Entry.PSObject.Properties.Name | Sort-Object)
    $expectedFields = @("active", "display_name", "package_path", "registry_entry_id")
    if (($entryFields -join '|') -ne ($expectedFields -join '|')) {
        $issues.Add("registry fields must be exactly: " + ($expectedFields -join ', '))
    }
    $relativePackagePath = [string]$Entry.package_path
    if (-not $relativePackagePath -or [System.IO.Path]::IsPathRooted($relativePackagePath)) {
        $issues.Add("package_path must be package-relative")
    }
    $resolvedPackagePath = Join-Path $PackageDir $relativePackagePath
    $metadataPath = Join-Path $resolvedPackagePath "metadata.json"
    if (-not (Test-Path -LiteralPath $metadataPath)) { $issues.Add("metadata.json missing") }
    $isBlank = ([string]$Entry.registry_entry_id).StartsWith("opendss_blank_")
    $checkpointName = $(if ($isBlank) { "imagenet_weights.pth" } else { "checkpoint.pth" })
    if (-not (Test-Path -LiteralPath (Join-Path $resolvedPackagePath $checkpointName))) { $issues.Add("$checkpointName missing") }
    if (-not (Test-Path -LiteralPath (Join-Path $resolvedPackagePath "model.onnx"))) { $issues.Add("model.onnx missing") }

    if ($issues.Count -gt 0) {
        $message = "Bundled asset contract failed for $($Entry.registry_entry_id): " + ($issues -join "; ")
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $relativePackagePath -Detail $message
        [void]$Errors.Add($message)
        return
    }

    $detail = "Simple registry entry resolves to a complete model package."
    Add-CheckResult -List $List -Name $Name -Status "pass" -Path $relativePackagePath -Detail $detail
}

$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $PackageDir "package_manifest.json"
}
$packageManifestPath = $ManifestPath

$checks = New-Object System.Collections.ArrayList
$errors = New-Object System.Collections.ArrayList
$warnings = New-Object System.Collections.ArrayList

$requiredPackageFiles = New-Object System.Collections.Generic.List[string]
$requiredDatasetDirs = @(
    "droplet_target_nontarget_binary_starter",
    "droplet_target_nontarget_3class_starter"
)
foreach ($relativePath in @(
    "OpenDSS.exe",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "onnxruntime.dll",
    "opencv_core4.dll",
    "opencv_imgproc4.dll",
    "opencv_imgcodecs4.dll",
    "platforms\qwindows.dll",
    "models\model_registry.json",
    "training\python\pyproject.toml",
    "training\python\README-windows-training.md",
    "training\python\droplet_trainer\__main__.py",
    "training\python\droplet_trainer\cli.py",
    "training\python\scripts\windows\create-training-venv.ps1",
    "training\python\scripts\windows\install-training-cpu.ps1",
    "training\python\scripts\windows\install-training-gpu-cu128.ps1",
    "training\python\scripts\windows\install-training-gpu-cu130.ps1",
    "training\python\scripts\windows\inspect-dataset.ps1",
    "training\python\scripts\windows\train-model.ps1",
    "training\python\scripts\windows\validate-dataset.ps1",
    "training\python\scripts\windows\validate-images.ps1",
    "training\python\scripts\windows\verify-training-env.ps1",
    "training\python\requirements\windows-py312-common-constraints.txt",
    "training\python\requirements\windows-py312-cpu.txt",
    "training\python\requirements\windows-py312-gpu-cu128.txt",
    "training\python\requirements\windows-py312-gpu-cu130.txt"
)) {
    $requiredPackageFiles.Add($relativePath)
}
foreach ($datasetDir in $requiredDatasetDirs) {
    foreach ($relativePath in @(
        "datasets\prepared\$datasetDir\metadata\dataset_manifest.json",
        "datasets\prepared\$datasetDir\metadata\dataset_summary.json",
        "datasets\prepared\$datasetDir\metadata\class_balance.csv"
    )) {
        $requiredPackageFiles.Add($relativePath)
    }
}

$packageRegistryPath = Join-Path $PackageDir "models\model_registry.json"
$requiredPackageFiles = @($requiredPackageFiles | Select-Object -Unique)

foreach ($relativePath in $requiredPackageFiles) {
    Test-RequiredPath -List $checks -Errors $errors -Name $relativePath -Path (Join-Path $PackageDir $relativePath) | Out-Null
}

foreach ($relativePath in @(
    "training\python\build",
    "training\python\outputs",
    "training\python\droplet_trainer.egg-info",
    "training\python\droplet_trainer\__pycache__"
)) {
    Test-ExcludedPath -List $checks -Errors $errors -Name ("excluded: " + $relativePath) -Path (Join-Path $PackageDir $relativePath) | Out-Null
}

foreach ($datasetDir in $requiredDatasetDirs) {
    $datasetManifestPath = Join-Path $PackageDir "datasets\prepared\$datasetDir\metadata\dataset_manifest.json"
    $summaryPath = Join-Path $PackageDir "datasets\prepared\$datasetDir\metadata\dataset_summary.json"
    Test-JsonFile -List $checks -Errors $errors -Name ("starter dataset manifest: " + $datasetDir) -Path $datasetManifestPath
    Test-JsonFile -List $checks -Errors $errors -Name ("starter dataset summary: " + $datasetDir) -Path $summaryPath
}

$registryPath = Join-Path $PackageDir "models\model_registry.json"
if (Test-Path -LiteralPath $registryPath) {
    $registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json
    if ([string]$registry.schema_version -ne "model-registry-v3-simple") {
        $message = "Expected model-registry-v3-simple; got $($registry.schema_version)."
        Add-CheckResult -List $checks -Name "simple model registry schema" -Status "fail" -Path $registryPath -Detail $message
        [void]$errors.Add($message)
    } else {
        Add-CheckResult -List $checks -Name "simple model registry schema" -Status "pass" -Path $registryPath -Detail "model-registry-v3-simple"
    }
    foreach ($entry in @($registry.entries)) {
        Test-SimpleModelPackageContract -List $checks -Errors $errors -Name ("model package: " + $entry.registry_entry_id) -Entry $entry -PackageDir $PackageDir
    }
}

$packageOnnxRuntimeDll = Join-Path $PackageDir "onnxruntime.dll"
$expectedOnnxRuntimeDll = Join-Path $ExpectedOnnxDir "lib\onnxruntime.dll"
Test-OnnxRuntimePackage -List $checks -Errors $errors -Warnings $warnings -PackageDll $packageOnnxRuntimeDll -ExpectedDll $expectedOnnxRuntimeDll -MinimumMajorMinor $MinimumOnnxRuntimeMajorMinor

$systemDir = [Environment]::SystemDirectory
$nidaqRuntime = Join-Path $systemDir "nicaiu.dll"
$dcamRuntime = Join-Path $systemDir "dcamapi.dll"
Test-OptionalPrerequisite -List $checks -Errors $errors -Warnings $warnings -Name "NI-DAQmx runtime nicaiu.dll" -Path $nidaqRuntime -Required ([bool]$RequireExternalRuntimes)
Test-OptionalPrerequisite -List $checks -Errors $errors -Warnings $warnings -Name "Hamamatsu DCAM runtime dcamapi.dll" -Path $dcamRuntime -Required ([bool]$RequireExternalRuntimes)

if ($RequireInstallerInputs) {
    Test-OptionalPrerequisite -List $checks -Errors $errors -Warnings $warnings -Name "NI-DAQmx installer input" -Path $NiInstaller -Required $true
    Test-OptionalPrerequisite -List $checks -Errors $errors -Warnings $warnings -Name "VC++ Redistributable installer input" -Path $VcRedist -Required $true
}

$manifest = [ordered]@{
    schema_version = "opendss-package-manifest-v1"
    generated_at = (Get-Date).ToString("o")
    package_dir = $PackageDir
    source_root = $SourceRoot
    status = $(if ($errors.Count -eq 0) { "pass" } else { "fail" })
    checks = $checks
    warnings = $warnings
    errors = $errors
}

if ($WriteManifest) {
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $packageManifestPath -Encoding UTF8
    Write-Host "Package manifest written to: $packageManifestPath"
}
Test-VcRedist -List $checks -Errors $errors -Path $VcRedist -Required ([bool]$RequireVcRedist)

foreach ($warning in $warnings) {
    Write-Warning $warning
}

if ($errors.Count -gt 0) {
    foreach ($failure in $errors) {
        Write-Error $failure -ErrorAction Continue
    }
    exit 1
}

Write-Host "Package check passed: $PackageDir"
exit 0
