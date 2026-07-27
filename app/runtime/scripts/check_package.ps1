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
    [string]$ExpectedCudaRuntimeDir = "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv-gpu\Lib\site-packages\torch\lib",
    [string]$MinimumOnnxRuntimeMajorMinor = "1.25"
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
    } elseif ($ExpectedDll) {
        $message = "Expected ONNX Runtime source DLL not found: $ExpectedDll"
        Add-CheckResult -List $List -Name "ONNX Runtime source hash" -Status "fail" -Path $ExpectedDll -Detail $message
        [void]$Errors.Add($message)
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
    "onnxruntime_providers_shared.dll",
    "onnxruntime_providers_cuda.dll",
    "cuda_inference_readiness.json",
    "cublas64_13.dll",
    "cublasLt64_13.dll",
    "cufft64_12.dll",
    "cudnn64_9.dll",
    "cudnn_ops64_9.dll",
    "cudnn_graph64_9.dll",
    "cudnn_heuristic64_9.dll",
    "cudnn_engines_precompiled64_9.dll",
    "cudnn_engines_runtime_compiled64_9.dll",
    "nvrtc64_130_0.dll",
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
Test-OnnxRuntimePackage -List $checks -Errors $errors -Warnings $warnings -PackageDll $packageOnnxRuntimeDll -ExpectedDll "" -MinimumMajorMinor $MinimumOnnxRuntimeMajorMinor

$qualifiedRuntimeHashes = [ordered]@{
    "onnxruntime.dll" = "a64bdd69d14f3685142c34ff46546a98cdd9ccae6130619bbee7414b5dd83b1b"
    "onnxruntime_providers_shared.dll" = "a22f19b4a103ac8c1828fdfe03c18a4e408332de9fd5003a9b6ccb3a0c2e3965"
    "onnxruntime_providers_cuda.dll" = "b54ad9ce0feac6eb39843bdbeb01253b8dd4c8032b839ee2a6102bf29fa73468"
    "cublas64_13.dll" = "5d083083cdd613577496791dd96d00fe3a78c955684b53ca46a8ffa3e0b1e170"
    "cublasLt64_13.dll" = "e1c26671801ecd435baf97c6e9cfe28196cf55005c2039356b48030b09ce5dd5"
    "cufft64_12.dll" = "611ba7e40dfab64b9b5bd35f4ad3593e00a8e93785fbf53160d9398aacd5ac14"
    "cudnn64_9.dll" = "99a529a5c252fbc1efcb2f51860498aed053be878090f2094ac86638d4765021"
    "cudnn_ops64_9.dll" = "92c09220f486c3eb3a456106e069667253f153792b2e8e91cba49db110a74f5e"
    "cudnn_graph64_9.dll" = "bd641fd259e877928ae9469efae7a40666d0b4db71ea1df18a40479378b75dfe"
    "cudnn_heuristic64_9.dll" = "c81c3cd1d76fade865512ceb5318f793b1871097aa92afefcddf5e4dd6cbd924"
    "cudnn_engines_precompiled64_9.dll" = "c329d58cb49e3a1574d2bbdde12c6a53f787c12f813efa9db7efa8d16c4207fb"
    "cudnn_engines_runtime_compiled64_9.dll" = "9d0cafc368b1998811f49509b031a4d69c5972a91a3210d89ae183ba7a918719"
    "nvrtc64_130_0.dll" = "37d2195bcdc5db37a838a8eb4f286f502ea94d82319bb7de68751ad672f4ad6d"
}
foreach ($entry in $qualifiedRuntimeHashes.GetEnumerator()) {
    Test-Hash -List $checks -Errors $errors -Name ("qualified hash: " + $entry.Key) -Path (Join-Path $PackageDir $entry.Key) -Expected $entry.Value
}

$readinessPath = Join-Path $PackageDir "cuda_inference_readiness.json"
if (Test-Path -LiteralPath $readinessPath) {
    try {
        $readiness = Get-Content -LiteralPath $readinessPath -Raw | ConvertFrom-Json
        if ([string]$readiness.status -ne "accepted" -or
            [string]$readiness.onnxruntime_version -ne "1.25.1" -or
            [bool]$readiness.cuda_provider_options.use_tf32) {
            throw "Readiness status, ONNX Runtime version, or TF32 contract is not accepted."
        }
        foreach ($entry in $qualifiedRuntimeHashes.GetEnumerator()) {
            $declared = [string]$readiness.runtime_file_hashes.PSObject.Properties[$entry.Key].Value
            if (-not $declared -or $declared.ToLowerInvariant() -ne $entry.Value) {
                throw "Readiness manifest does not pin the accepted hash for $($entry.Key)."
            }
        }
        Add-CheckResult -List $checks -Name "CUDA readiness trust manifest" -Status "pass" -Path $readinessPath -Detail "Accepted fixed 13-file runtime closure."
    } catch {
        $message = "CUDA readiness trust manifest failed: $($_.Exception.Message)"
        Add-CheckResult -List $checks -Name "CUDA readiness trust manifest" -Status "fail" -Path $readinessPath -Detail $message
        [void]$errors.Add($message)
    }
}

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
