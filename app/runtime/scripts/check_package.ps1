param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$NiInstaller = "",
    [string]$VcRedist = "",
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

function Test-RegistryEntryAssetContract {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Name,
        $Entry,
        [string]$ExpectedRegistryEntryId,
        [string]$ExpectedModelPath,
        [string]$ExpectedMetadataPath
    )

    if (-not $Entry) {
        $message = "Missing required model registry entry: $ExpectedRegistryEntryId"
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $ExpectedRegistryEntryId -Detail $message
        [void]$Errors.Add($message)
        return
    }

    $issues = New-Object System.Collections.Generic.List[string]
    $registryEntryId = [string]$Entry.registry_entry_id
    $modelPath = [string]$Entry.model_path
    $metadataPath = [string]$Entry.metadata_path

    if ($registryEntryId -ne $ExpectedRegistryEntryId) {
        $issues.Add("registry_entry_id=$registryEntryId")
    }
    if ($modelPath -ne $ExpectedModelPath) {
        $issues.Add("model_path=$modelPath")
    }
    if ($metadataPath -ne $ExpectedMetadataPath) {
        $issues.Add("metadata_path=$metadataPath")
    }
    if ([System.IO.Path]::IsPathRooted($modelPath) -or $modelPath.StartsWith("\\") -or $modelPath.StartsWith("/")) {
        $issues.Add("model_path must stay package-relative")
    }
    if ([System.IO.Path]::IsPathRooted($metadataPath) -or $metadataPath.StartsWith("\\") -or $metadataPath.StartsWith("/")) {
        $issues.Add("metadata_path must stay package-relative")
    }

    if ($issues.Count -gt 0) {
        $message = "Bundled asset contract failed for ${ExpectedRegistryEntryId}: " + ($issues -join "; ")
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $ExpectedRegistryEntryId -Detail $message
        [void]$Errors.Add($message)
        return
    }

    $detail = "Registry entry points to bundled assets via $modelPath and $metadataPath."
    Add-CheckResult -List $List -Name $Name -Status "pass" -Path $ExpectedRegistryEntryId -Detail $detail
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
if (Test-Path -LiteralPath $packageRegistryPath) {
    $packageRegistry = Get-Content -LiteralPath $packageRegistryPath -Raw | ConvertFrom-Json
    foreach ($entry in @($packageRegistry.entries)) {
        foreach ($assetPath in @($entry.model_path, $entry.metadata_path)) {
            if ($assetPath) {
                $requiredPackageFiles.Add("models\" + (Split-Path -Leaf $assetPath))
            }
        }
        foreach ($sidecar in @($entry.model_sidecars)) {
            if ($sidecar.required -and $sidecar.path) {
                $requiredPackageFiles.Add("models\" + (Split-Path -Leaf $sidecar.path))
            }
        }
    }
}
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
    foreach ($entry in @($registry.entries)) {
        $modelName = Split-Path -Leaf $entry.model_path
        $metadataName = Split-Path -Leaf $entry.metadata_path
        if ($modelName) {
            Test-Hash -List $checks -Errors $errors -Name ("registry model hash: " + $entry.registry_entry_id) -Path (Join-Path $PackageDir ("models\" + $modelName)) -Expected $entry.model_sha256
        }
        if ($metadataName) {
            Test-Hash -List $checks -Errors $errors -Name ("registry metadata hash: " + $entry.registry_entry_id) -Path (Join-Path $PackageDir ("models\" + $metadataName)) -Expected $entry.metadata_sha256
        }
        foreach ($sidecar in @($entry.model_sidecars)) {
            if ($sidecar.required) {
                Test-Hash -List $checks -Errors $errors -Name ("registry sidecar hash: " + $entry.registry_entry_id) -Path (Join-Path $PackageDir ("models\" + (Split-Path -Leaf $sidecar.path))) -Expected $sidecar.sha256
            }
        }
    }
    $blankEntry = $registry.entries | Where-Object { $_.registry_entry_id -eq "blank_squeezenet_template_seed42" } | Select-Object -First 1
    Test-RegistryEntryAssetContract `
        -List $checks `
        -Errors $errors `
        -Name "blank starter bundled registry contract" `
        -Entry $blankEntry `
        -ExpectedRegistryEntryId "blank_squeezenet_template_seed42" `
        -ExpectedModelPath "app/runtime/models/blank_squeezenet_template.onnx" `
        -ExpectedMetadataPath "app/runtime/models/blank_squeezenet_template_metadata.json"
    $expectedPromotedEntryId = "run_20260429_221500_wsl2_binary_linuxmirror_onnx"
    $expectedPromotedModelPath = "app/runtime/models/squeezenet_final_new_condition.onnx"
    $expectedPromotedMetadataPath = "app/runtime/models/metadata.json"
    $expectedPromotedEntry = $registry.entries | Where-Object { $_.registry_entry_id -eq $expectedPromotedEntryId } | Select-Object -First 1
    Test-RegistryEntryAssetContract `
        -List $checks `
        -Errors $errors `
        -Name "promoted/current runtime registry contract" `
        -Entry $expectedPromotedEntry `
        -ExpectedRegistryEntryId $expectedPromotedEntryId `
        -ExpectedModelPath $expectedPromotedModelPath `
        -ExpectedMetadataPath $expectedPromotedMetadataPath

    $promotedEntries = @($registry.entries | Where-Object { $_.state -eq "promoted_current" })
    if ($promotedEntries.Count -ne 1) {
        $message = "Expected exactly one promoted_current entry in model registry; found $($promotedEntries.Count)."
        Add-CheckResult -List $checks -Name "model registry promoted entry" -Status "fail" -Path $registryPath -Detail $message
        [void]$errors.Add($message)
    } else {
        $currentEntry = $promotedEntries[0]
        $issues = New-Object System.Collections.Generic.List[string]
        if ([string]$currentEntry.registry_entry_id -ne $expectedPromotedEntryId) {
            $issues.Add("registry_entry_id=$($currentEntry.registry_entry_id)")
        }
        if (-not [bool]$currentEntry.selectable_for_normal_live_sorting) {
            $issues.Add("selectable_for_normal_live_sorting=false")
        }
        if ([string]$currentEntry.live_use_mode -ne "normal") {
            $issues.Add("live_use_mode=$($currentEntry.live_use_mode)")
        }
        if ($issues.Count -gt 0) {
            $message = "promoted_current contract mismatch: " + ($issues -join "; ")
            Add-CheckResult -List $checks -Name "model registry promoted entry" -Status "fail" -Path $registryPath -Detail $message
            [void]$errors.Add($message)
        } else {
            Add-CheckResult -List $checks -Name "model registry promoted entry" -Status "pass" -Path $registryPath -Detail ("promoted_current=" + $currentEntry.registry_entry_id)
        }
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
