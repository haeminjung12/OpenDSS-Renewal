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

$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $SourceRoot "..\..")).Path
$RepoParent = Split-Path -Parent $RepoRoot

if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoParent $AcceptedBuildDirName
    Write-Host "Using accepted build tree default: $BuildDir"
}
if (-not $ModelsDir) { $ModelsDir = Join-Path $SourceRoot "models" }
if (-not $OutputDir) { $OutputDir = Join-Path $RepoParent "artifacts\internal-release" }

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

$onnxRuntimeDll = Join-Path $OnnxDir "lib\onnxruntime.dll"
if (-not (Test-Path -LiteralPath $onnxRuntimeDll)) {
    throw "Required ONNX Runtime DLL not found: $onnxRuntimeDll"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$packageDir = Join-Path $OutputDir ("OpenVisualDropletSorterSuite_" + $stamp)
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

Copy-Item -Path $exePath -Destination $packageDir -Force

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
