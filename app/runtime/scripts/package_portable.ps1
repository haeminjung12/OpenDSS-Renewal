param(
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [Parameter(Mandatory = $true)]
    [string]$AcceptedExecutablePath,
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9A-Fa-f]{64}$")]
    [string]$AcceptedExecutableSha256,
    [string]$QtDir = "C:\Qt\6.10.1\msvc2022_64",
    [string]$OnnxDir = "C:\onnxruntime-gpu",
    [string]$CudaRuntimeDir = "$env:LOCALAPPDATA\OpenDSS\training-venv-gpu\Lib\site-packages\torch\lib",
    [string]$VcpkgBin = "C:\vcpkg\installed\x64-windows\bin",
    [string]$ModelsDir = "",
    [string]$TrainerWheelPath = $env:OPENDSS_TRAINER_WHEEL,
    [string]$WheelBuildPython = (
        Join-Path $env:LOCALAPPDATA "OpenDSS\training-venv-gpu\Scripts\python.exe"
    ),
    [string]$OutputDir = "",
    [switch]$CopyNidaq,
    [switch]$SkipPackageCheck,
    [switch]$NoManifest,
    [string]$NidaqBin = "C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc"
)

function Copy-FilteredTree {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,
        [Parameter(Mandatory = $true)]
        [string]$DestinationDir,
        [string[]]$ExcludedRelativeRoots = @()
    )

    $sourceRoot = (Resolve-Path -LiteralPath $SourceDir).Path
    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null

    Get-ChildItem -LiteralPath $sourceRoot -Recurse -Force | ForEach-Object {
        $relativePath = $_.FullName.Substring($sourceRoot.Length).TrimStart('\')
        if (-not $relativePath) {
            return
        }
        foreach ($excludedRoot in $ExcludedRelativeRoots) {
            $normalizedRoot = $excludedRoot.Trim("\")
            if ($relativePath.Equals(
                    $normalizedRoot,
                    [System.StringComparison]::OrdinalIgnoreCase) -or
                $relativePath.StartsWith(
                    $normalizedRoot + "\",
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                return
            }
        }
        if ($relativePath -match '(^|\\)__pycache__(\\|$)' -or
            $relativePath -like '*.pyc' -or
            $relativePath -like '*.pyo') {
            return
        }

        $destinationPath = Join-Path $DestinationDir $relativePath
        if ($_.PSIsContainer) {
            New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
        } else {
            $destinationParent = Split-Path -Parent $destinationPath
            if ($destinationParent) {
                New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
            }
            Copy-Item -LiteralPath $_.FullName -Destination $destinationPath -Force
        }
    }
}

$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $SourceRoot "..\..")).Path
$RepoParent = Split-Path -Parent $RepoRoot

if (-not $ModelsDir) { $ModelsDir = Join-Path $SourceRoot "models" }
if (-not $OutputDir) { $OutputDir = Join-Path $RepoParent "artifacts\internal-release" }
$preparedDatasetRoot = Join-Path $RepoRoot "datasets\prepared"
$requiredDatasetDirs = @(
    "droplet_target_nontarget_binary_starter",
    "droplet_target_nontarget_3class_starter"
)

$trainerSourceRoot = Join-Path $RepoRoot "training\python"
$requiredTrainerFiles = @(
    "pyproject.toml",
    "README-windows-training.md",
    "droplet_trainer\__main__.py",
    "droplet_trainer\cli.py",
    "scripts\windows\build-training-bootstrap-wheel.ps1",
    "scripts\windows\provision-training-runtime.ps1",
    "requirements\windows-py312-gpu-cu130-downloads.json",
    "requirements\windows-py312-gpu-cu130.lock",
    "requirements\windows-py312-gpu-cu130-inventory.json"
)

$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
if ($OutputDir.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDir must be outside the clean release repo. Got: $OutputDir"
}

$exeName = "OpenDSS.exe"
$exePath = (Resolve-Path -LiteralPath $AcceptedExecutablePath).Path
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Accepted v2 executable not found: $exePath"
}
$acceptedExecutableHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exePath).Hash
if ($acceptedExecutableHash -ne $AcceptedExecutableSha256.ToUpperInvariant()) {
    throw "Accepted v2 executable SHA-256 mismatch. Expected $($AcceptedExecutableSha256.ToUpperInvariant()), got $acceptedExecutableHash."
}
$acceptedRuntimeRoot = Split-Path -Parent $exePath
$acceptedQmlRoot = Join-Path $acceptedRuntimeRoot "qml"
foreach ($acceptedRuntimeAsset in @(
    $acceptedQmlRoot,
    (Join-Path $acceptedRuntimeRoot "qt.conf")
)) {
    if (-not (Test-Path -LiteralPath $acceptedRuntimeAsset)) {
        throw "Accepted deployed runtime asset not found: $acceptedRuntimeAsset"
    }
}

$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt.exe not found: $windeployqt"
}

if (-not (Test-Path $ModelsDir)) {
    throw "ModelsDir not found: $ModelsDir"
}
if (-not (Test-Path -LiteralPath $trainerSourceRoot)) {
    throw "Trainer source root not found: $trainerSourceRoot"
}
foreach ($relativeTrainerFile in $requiredTrainerFiles) {
    $trainerPath = Join-Path $trainerSourceRoot $relativeTrainerFile
    if (-not (Test-Path -LiteralPath $trainerPath)) {
        throw "Required trainer asset not found: $trainerPath"
    }
}
if (-not $TrainerWheelPath) {
    $wheelBuilder = Join-Path $trainerSourceRoot (
        "scripts\windows\build-training-bootstrap-wheel.ps1")
    if (-not (Test-Path -LiteralPath $WheelBuildPython -PathType Leaf)) {
        throw "WheelBuildPython is required to build the source-owned trainer wheel."
    }
    $TrainerWheelPath = & $wheelBuilder -SourceRoot $trainerSourceRoot `
        -PythonExecutable $WheelBuildPython `
        -OutputDirectory (Join-Path $OutputDir "training-bootstrap")
    $TrainerWheelPath = @($TrainerWheelPath)[-1]
}
if (-not (Test-Path -LiteralPath $TrainerWheelPath -PathType Leaf)) {
    throw "TrainerWheelPath must name the deterministic droplet-trainer 0.2.0 wheel."
}
$TrainerWheelPath = (Resolve-Path -LiteralPath $TrainerWheelPath).Path
$downloadCatalog =
    Get-Content -LiteralPath (Join-Path $trainerSourceRoot "requirements\windows-py312-gpu-cu130-downloads.json") -Raw |
        ConvertFrom-Json
if ((Split-Path -Leaf $TrainerWheelPath) -ne [string]$downloadCatalog.embedded_wheel) {
    throw "TrainerWheelPath filename differs from the repository-authoritative bootstrap catalog."
}
$trainerLockLine = Get-Content -LiteralPath (
    Join-Path $trainerSourceRoot "requirements\windows-py312-gpu-cu130.lock") |
        Where-Object { $_ -match "^droplet-trainer==" }
if (@($trainerLockLine).Count -ne 1 -or
    $trainerLockLine -notmatch "--hash=sha256:([0-9a-fA-F]{64})$") {
    throw "Authoritative droplet-trainer lock entry is missing or invalid."
}
$trainerWheelHash = (Get-FileHash -LiteralPath $TrainerWheelPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($trainerWheelHash -ne $Matches[1].ToLowerInvariant()) {
    throw "TrainerWheelPath hash differs from the authoritative lock."
}

$registryPath = Join-Path $ModelsDir "model_registry.json"
if (-not (Test-Path $registryPath)) {
    throw "Required model registry not found: $registryPath"
}
$registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json
if ([string]$registry.schema_version -ne "model-registry-v3-simple") {
    throw "Unsupported model registry schema: $($registry.schema_version)"
}
foreach ($entry in @($registry.entries)) {
    $packagePath = [string]$entry.package_path
    if (-not $packagePath -or [System.IO.Path]::IsPathRooted($packagePath)) {
        throw "Registry entry $($entry.registry_entry_id) has an invalid package_path: $packagePath"
    }
    $relativePackagePath = $packagePath -replace '^[.]?[\\/]*models[\\/]', ''
    $sourcePackagePath = Join-Path $ModelsDir $relativePackagePath
    if (-not (Test-Path -LiteralPath (Join-Path $sourcePackagePath "metadata.json"))) {
        throw "Model package metadata not found: $sourcePackagePath"
    }
}
foreach ($datasetDir in $requiredDatasetDirs) {
    $datasetPath = Join-Path $preparedDatasetRoot $datasetDir
    if (-not (Test-Path -LiteralPath $datasetPath)) {
        throw "Required starter dataset not found: $datasetPath"
    }
}

$onnxRuntimeDll = Join-Path $OnnxDir "lib\onnxruntime.dll"
if (-not (Test-Path -LiteralPath $onnxRuntimeDll)) {
    throw "Required ONNX Runtime DLL not found: $onnxRuntimeDll"
}
$cudaReadinessPath = Join-Path $SourceRoot "models\cuda_inference_readiness.json"
if (-not (Test-Path -LiteralPath $cudaReadinessPath)) {
    throw "Required CUDA readiness artifact not found: $cudaReadinessPath"
}
$qualifiedRuntimeHashes = @{
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

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$packageDir = Join-Path $OutputDir ("OpenDSS_" + $stamp)
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

$packagedExecutablePath = Join-Path $packageDir $exeName
Copy-Item -LiteralPath $exePath -Destination $packagedExecutablePath -Force
$packagedExecutableHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $packagedExecutablePath).Hash
if ($packagedExecutableHash -ne $acceptedExecutableHash) {
    throw "Packaged OpenDSS.exe differs from the accepted v2 executable."
}

foreach ($noticeFile in @("LICENSE", "THIRD_PARTY_NOTICES.md")) {
    $noticePath = Join-Path $RepoRoot $noticeFile
    if (-not (Test-Path -LiteralPath $noticePath)) {
        throw "Required release notice file not found: $noticePath"
    }
    Copy-Item -LiteralPath $noticePath -Destination (Join-Path $packageDir $noticeFile) -Force
}

# Copy any DLLs that the build already produced beside the executable.
$buildDllDir = Split-Path $exePath -Parent
Get-ChildItem -Path $buildDllDir -Filter "*.dll" | ForEach-Object {
    if (@("Qt6Test.dll", "Qt6QuickTest.dll") -notcontains $_.Name) {
        Copy-Item -Path $_.FullName -Destination $packageDir -Force
    }
}

# Add the selected ONNX Runtime DLLs. The main DLL is required so packages do
# not silently inherit stale PATH copies at runtime.
$onnxDlls = @(
    @{ Name = "onnxruntime.dll"; Required = $true },
    @{ Name = "onnxruntime_providers_shared.dll"; Required = $true },
    @{ Name = "onnxruntime_providers_cuda.dll"; Required = $true }
)
foreach ($dll in $onnxDlls) {
    $src = Join-Path $OnnxDir ("lib\" + $dll.Name)
    if (Test-Path -LiteralPath $src) {
        $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $src).Hash.ToLowerInvariant()
        if ($actualHash -ne $qualifiedRuntimeHashes[$dll.Name]) {
            throw "Qualified ONNX Runtime hash mismatch: $src"
        }
        Copy-Item -LiteralPath $src -Destination $packageDir -Force
    } elseif ($dll.Required) {
        throw "Required ONNX Runtime DLL not found: $src"
    }
}
Copy-Item -LiteralPath $cudaReadinessPath -Destination (Join-Path $packageDir "cuda_inference_readiness.json") -Force

$cudaRuntimeDlls = @(
    "cublas64_13.dll",
    "cublasLt64_13.dll",
    "cufft64_12.dll",
    "cudnn64_9.dll",
    "cudnn_ops64_9.dll",
    "cudnn_graph64_9.dll",
    "cudnn_heuristic64_9.dll",
    "cudnn_engines_precompiled64_9.dll",
    "cudnn_engines_runtime_compiled64_9.dll",
    "nvrtc64_130_0.dll"
)
foreach ($dllName in $cudaRuntimeDlls) {
    $src = Join-Path $CudaRuntimeDir $dllName
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Required qualified CUDA runtime DLL not found: $src"
    }
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $src).Hash.ToLowerInvariant()
    if ($actualHash -ne $qualifiedRuntimeHashes[$dllName]) {
        throw "Qualified CUDA runtime hash mismatch: $src"
    }
    Copy-Item -LiteralPath $src -Destination $packageDir -Force
}

# Add OpenCV DLLs if available from vcpkg.
if (Test-Path $VcpkgBin) {
    Get-ChildItem -Path $VcpkgBin -Filter "opencv_*4.dll" | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination $packageDir -Force
    }
}

# Optional: NI-DAQmx DLLs (only if you want to bundle them).
if ($CopyNidaq -and (Test-Path $NidaqBin)) {
    Get-ChildItem -Path $NidaqBin -Filter "NIDAQmx*.dll" | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination $packageDir -Force
    }
}

# Copy the complete model tree so each registry package keeps its checkpoint,
# embedded/external ONNX data, metadata, and audit companions.
$modelsOut = Join-Path $packageDir "models"
Copy-FilteredTree -SourceDir $ModelsDir -DestinationDir $modelsOut

$datasetsOut = Join-Path $packageDir "datasets\prepared"
New-Item -ItemType Directory -Path $datasetsOut -Force | Out-Null
foreach ($datasetDir in $requiredDatasetDirs) {
    Copy-FilteredTree -SourceDir (Join-Path $preparedDatasetRoot $datasetDir) -DestinationDir (Join-Path $datasetsOut $datasetDir)
}

# Copy trainer source, Windows wrappers, and dependency metadata into the
# package-relative layout expected by the desktop app.
$trainerOut = Join-Path $packageDir "training\python"
New-Item -ItemType Directory -Path $trainerOut -Force | Out-Null
foreach ($trainerFile in @("pyproject.toml", "README-windows-training.md")) {
    Copy-Item -LiteralPath (Join-Path $trainerSourceRoot $trainerFile) -Destination (Join-Path $trainerOut $trainerFile) -Force
}
Copy-FilteredTree -SourceDir (Join-Path $trainerSourceRoot "droplet_trainer") -DestinationDir (Join-Path $trainerOut "droplet_trainer")
Copy-FilteredTree -SourceDir (Join-Path $trainerSourceRoot "requirements") -DestinationDir (Join-Path $trainerOut "requirements")
Copy-FilteredTree -SourceDir (Join-Path $trainerSourceRoot "scripts\windows") -DestinationDir (Join-Path $trainerOut "scripts\windows")
foreach ($onlineSetupFile in @(
    "scripts\windows\build-training-bootstrap-wheel.ps1",
    "scripts\windows\create-training-venv.ps1",
    "scripts\windows\inspect-dataset.ps1",
    "scripts\windows\install-training-gpu-cu130.ps1",
    "scripts\windows\install-training-cpu.ps1",
    "scripts\windows\install-training-gpu-cu128.ps1",
    "scripts\windows\set-app-trainer-python.ps1",
    "scripts\windows\train-model.ps1",
    "scripts\windows\validate-dataset.ps1",
    "scripts\windows\validate-images.ps1",
    "scripts\windows\verify-training-env.ps1",
    "requirements\windows-py312-cpu.txt",
    "requirements\windows-py312-gpu-cu128.txt",
    "requirements\windows-py312-common-constraints.txt",
    "requirements\windows-py312-gpu-cu130-downloads.json",
    "requirements\windows-py312-gpu-cu130-inventory.json",
    "requirements\windows-py312-gpu-cu130.lock",
    "requirements\windows-py312-gpu-cu130.txt"
)) {
    $packagedPath = Join-Path $trainerOut $onlineSetupFile
    if (Test-Path -LiteralPath $packagedPath) {
        Remove-Item -LiteralPath $packagedPath -Force
    }
}
$bootstrapOut = Join-Path $packageDir "training\bootstrap"
New-Item -ItemType Directory -Path $bootstrapOut -Force | Out-Null
foreach ($bootstrapFile in @(
    "windows-py312-gpu-cu130-downloads.json",
    "windows-py312-gpu-cu130.lock",
    "windows-py312-gpu-cu130-inventory.json"
)) {
    Copy-Item -LiteralPath (Join-Path $trainerSourceRoot "requirements\$bootstrapFile") `
        -Destination (Join-Path $bootstrapOut $bootstrapFile) -Force
}
Copy-Item -LiteralPath $TrainerWheelPath -Destination (
    Join-Path $bootstrapOut ([string]$downloadCatalog.embedded_wheel)) -Force

# Deploy Qt runtime and plugins next to the exe.
& $windeployqt (Join-Path $packageDir $exeName) | Out-Host

# Preserve every accepted deployed QML/plugin tree that windeployqt cannot
# infer from the resource-embedded application QML root.
Get-ChildItem -LiteralPath $acceptedRuntimeRoot -Directory | Where-Object {
    $_.Name -ne "models"
} | ForEach-Object {
    $excludedRoots = $(if ($_.Name -eq "qml") { @("QtTest") } else { @() })
    Copy-FilteredTree -SourceDir $_.FullName `
        -DestinationDir (Join-Path $packageDir $_.Name) `
        -ExcludedRelativeRoots $excludedRoots
}
Copy-Item -LiteralPath (Join-Path $acceptedRuntimeRoot "qt.conf") `
    -Destination (Join-Path $packageDir "qt.conf") -Force

if (-not $SkipPackageCheck) {
    $checkScript = Join-Path $PSScriptRoot "check_package.ps1"
    if (-not (Test-Path $checkScript)) {
        throw "Package check script not found: $checkScript"
    }

    & $checkScript -PackageDir $packageDir -SourceRoot $SourceRoot `
        -ExpectedOpenDssSha256 $acceptedExecutableHash `
        -ExpectedDeployedRuntimeRoot $acceptedRuntimeRoot `
        -ExpectedOnnxDir $OnnxDir -ExpectedCudaRuntimeDir $CudaRuntimeDir `
        -WriteManifest:(!$NoManifest)
    if ($LASTEXITCODE -ne 0) {
        throw "Package check failed with exit code $LASTEXITCODE"
    }
}

Write-Host "Portable package created at: $packageDir"
Write-Output $packageDir
