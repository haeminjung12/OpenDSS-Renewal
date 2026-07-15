param(
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$BuildDir = "",
    [string]$AcceptedBuildDirName = "build-internal-release-wave61",
    [string]$Config = "Release",
    [string]$QtDir = "C:\Qt\6.10.1\msvc2022_64",
    [string]$OnnxDir = "C:\onnxruntime-gpu",
    [string]$VcpkgBin = "C:\vcpkg\installed\x64-windows\bin",
    [string]$ModelsDir = "",
    [string]$OutputDir = "",
    [switch]$CopyNidaq = $true,
    [switch]$SkipPackageCheck,
    [switch]$NoManifest,
    [string]$NidaqBin = "C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc"
)

function Copy-FilteredTree {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,
        [Parameter(Mandatory = $true)]
        [string]$DestinationDir
    )

    $sourceRoot = (Resolve-Path -LiteralPath $SourceDir).Path
    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null

    Get-ChildItem -LiteralPath $sourceRoot -Recurse -Force | ForEach-Object {
        $relativePath = $_.FullName.Substring($sourceRoot.Length).TrimStart('\')
        if (-not $relativePath) {
            return
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

if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoParent $AcceptedBuildDirName
    Write-Host "Using accepted build tree default: $BuildDir"
}
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
    "scripts\windows\create-training-venv.ps1",
    "scripts\windows\install-training-cpu.ps1",
    "scripts\windows\verify-training-env.ps1",
    "scripts\windows\train-model.ps1",
    "requirements\windows-py312-common-constraints.txt",
    "requirements\windows-py312-cpu.txt"
)

$BuildDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDir)
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
if ($BuildDir.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDir must be outside the clean release repo. Got: $BuildDir"
}
if ($OutputDir.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDir must be outside the clean release repo. Got: $OutputDir"
}

$exeName = "OpenVisualDropletSorter.exe"
$exePath = Join-Path $BuildDir ("desktop_app\" + $Config + "\" + $exeName)
if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath"
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

$registryPath = Join-Path $ModelsDir "model_registry.json"
if (-not (Test-Path $registryPath)) {
    throw "Required model registry not found: $registryPath"
}
$registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json
$requiredModelFiles = New-Object System.Collections.Generic.List[string]
$requiredModelFiles.Add("model_registry.json")
foreach ($entry in @($registry.entries)) {
    foreach ($assetPath in @($entry.model_path, $entry.metadata_path)) {
        if ($assetPath) {
            $requiredModelFiles.Add((Split-Path -Leaf $assetPath))
        }
    }
    foreach ($sidecar in @($entry.model_sidecars)) {
        if ($sidecar.required -and $sidecar.path) {
            $requiredModelFiles.Add((Split-Path -Leaf $sidecar.path))
        }
    }
}
$requiredModelFiles = @($requiredModelFiles | Select-Object -Unique)
foreach ($modelFile in $requiredModelFiles) {
    $modelPath = Join-Path $ModelsDir $modelFile
    if (-not (Test-Path $modelPath)) {
        throw "Required model asset not found: $modelPath"
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

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$packageDir = Join-Path $OutputDir ("OpenVisualDropletSorterSuite_" + $stamp)
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

Copy-Item -Path $exePath -Destination $packageDir -Force

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
    Copy-Item -Path $_.FullName -Destination $packageDir -Force
}

# Add the selected ONNX Runtime DLLs. The main DLL is required so packages do
# not silently inherit stale PATH copies at runtime.
$onnxDlls = @(
    @{ Name = "onnxruntime.dll"; Required = $true },
    @{ Name = "onnxruntime_providers_shared.dll"; Required = $false },
    @{ Name = "onnxruntime_providers_cuda.dll"; Required = $false },
    @{ Name = "onnxruntime_providers_tensorrt.dll"; Required = $false }
)
foreach ($dll in $onnxDlls) {
    $src = Join-Path $OnnxDir ("lib\" + $dll.Name)
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination $packageDir -Force
    } elseif ($dll.Required) {
        throw "Required ONNX Runtime DLL not found: $src"
    }
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

# Copy model assets into the package contract expected by the runtime.
$modelsOut = Join-Path $packageDir "models"
New-Item -ItemType Directory -Path $modelsOut -Force | Out-Null
foreach ($modelFile in $requiredModelFiles) {
    Copy-Item -Path (Join-Path $ModelsDir $modelFile) -Destination $modelsOut -Force
}

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

# Deploy Qt runtime and plugins next to the exe.
& $windeployqt (Join-Path $packageDir $exeName) | Out-Host

if (-not $SkipPackageCheck) {
    $checkScript = Join-Path $PSScriptRoot "check_package.ps1"
    if (-not (Test-Path $checkScript)) {
        throw "Package check script not found: $checkScript"
    }

    & $checkScript -PackageDir $packageDir -SourceRoot $SourceRoot -WriteManifest:(!$NoManifest)
    if ($LASTEXITCODE -ne 0) {
        throw "Package check failed with exit code $LASTEXITCODE"
    }
}

Write-Host "Portable package created at: $packageDir"
Write-Output $packageDir
