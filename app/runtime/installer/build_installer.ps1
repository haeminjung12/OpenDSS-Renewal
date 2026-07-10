param(
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$BuildDir = "",
    [string]$AcceptedBuildDirName = "build-opendss-internal-release",
    [string]$Config = "Release",
    [string]$QtDir = "C:\Qt\6.10.1\msvc2022_64",
    [string]$OnnxDir = "C:\onnxruntime-gpu",
    [string]$VcpkgBin = "C:\vcpkg\installed\x64-windows\bin",
    [string]$ModelsDir = "",
    [string]$OutputDir = "",
    [string]$NiInstaller = "",
    [string]$VcRedist = "",
    [string]$InnoSetup = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    [switch]$CopyNidaq
)

$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $SourceRoot "..\..")).Path
$RepoParent = Split-Path -Parent $RepoRoot

if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoParent $AcceptedBuildDirName
    Write-Host "Using accepted build tree default: $BuildDir"
}
if (-not $ModelsDir) { $ModelsDir = Join-Path $SourceRoot "models" }
if (-not $OutputDir) { $OutputDir = Join-Path $RepoParent "artifacts\internal-release\installer" }

$BuildDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDir)
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
if ($BuildDir.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDir must be outside the clean release repo. Got: $BuildDir"
}
if ($OutputDir.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDir must be outside the clean release repo. Got: $OutputDir"
}

if ($NiInstaller) {
    Write-Warning "Ignoring -NiInstaller. Public installers do not bundle NI-DAQmx installer payloads; users must install NI-DAQmx from NI before using DAQ output."
}
if ($VcRedist) {
    Write-Warning "Ignoring -VcRedist. Public installers do not bundle Microsoft Visual C++ Redistributable payloads; users must install it from Microsoft if needed."
}
if ($CopyNidaq) {
    Write-Warning "Copying NI-DAQmx runtime DLLs was requested. Use this only for internal validation with documented redistribution approval, not for prerequisite-only public releases."
}

if (-not (Test-Path $InnoSetup)) {
    throw "Inno Setup compiler not found: $InnoSetup"
}

$packageScript = Join-Path $SourceRoot "scripts\package_portable.ps1"
if (-not (Test-Path $packageScript)) {
    throw "Portable packaging script not found: $packageScript"
}

$packageDir = & $packageScript `
    -SourceRoot $SourceRoot `
    -BuildDir $BuildDir `
    -AcceptedBuildDirName $AcceptedBuildDirName `
    -Config $Config `
    -QtDir $QtDir `
    -OnnxDir $OnnxDir `
    -VcpkgBin $VcpkgBin `
    -ModelsDir $ModelsDir `
    -OutputDir (Join-Path $RepoParent "artifacts\internal-release\portable") `
    -CopyNidaq:$CopyNidaq

$packageDir = ($packageDir | Select-Object -Last 1)
if (-not $packageDir) {
    throw "Portable packaging did not return an output directory."
}
$packageDir = (Resolve-Path $packageDir).Path

$checkScript = Join-Path $SourceRoot "scripts\check_package.ps1"
& $checkScript `
    -PackageDir $packageDir `
    -SourceRoot $SourceRoot `
    -WriteManifest
if ($LASTEXITCODE -ne 0) {
    throw "Package check failed before installer build with exit code $LASTEXITCODE"
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$issPath = Join-Path $SourceRoot "installer\installer.iss"
if (-not (Test-Path $issPath)) {
    throw "Installer script not found: $issPath"
}

$defineSource = "/DSourceDir=`"$packageDir`""
$defineOut = "/DOutputDir=`"$OutputDir`""

& $InnoSetup $defineSource $defineOut $issPath
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}

Write-Host "Installer created in: $OutputDir"
