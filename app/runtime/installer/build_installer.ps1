param(
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$BuildDir = "",
    [string]$AcceptedBuildDirName = "build-opendss-internal-release",
    [string]$Config = "Release",
    [string]$QtDir = "C:\Qt\6.10.1\msvc2022_64",
    [string]$OnnxDir = "C:\onnxruntime-gpu",
    [string]$VcpkgBin = "C:\vcpkg\installed\x64-windows\bin",
    [string]$ModelsDir = "",
    [string]$TrainerWheelPath = $env:OPENDSS_TRAINER_WHEEL,
    [string]$WheelBuildPython = (
        Join-Path $env:LOCALAPPDATA "OpenDSS\training-venv-gpu\Scripts\python.exe"
    ),
    [string]$OutputDir = "",
    [string]$NiInstaller = "",
    [string]$VcRedist = "",
    [string]$VcRedistUrl = "https://aka.ms/vc14/vc_redist.x64.exe",
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
if ($CopyNidaq) {
    Write-Warning "Copying NI-DAQmx runtime DLLs was requested. Use this only for internal validation with documented redistribution approval, not for prerequisite-only public releases."
}

if (-not (Test-Path $InnoSetup)) {
    throw "Inno Setup compiler not found: $InnoSetup"
}
$innoFile = Get-Item -LiteralPath $InnoSetup
$innoSignature = Get-AuthenticodeSignature -LiteralPath $InnoSetup
if ($innoFile.VersionInfo.FileDescription -ne "Inno Setup Command-Line Compiler" -or
    $innoSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
    $innoSignature.SignerCertificate.Subject -notmatch "Pyrsys B[.]V[.]") {
    throw "Inno Setup compiler must be the valid Pyrsys-signed command-line compiler: $InnoSetup"
}

if (-not $VcRedist) {
    $prerequisiteDir = Join-Path $RepoParent "artifacts\internal-release\prerequisites"
    New-Item -ItemType Directory -Path $prerequisiteDir -Force | Out-Null
    $VcRedist = Join-Path $prerequisiteDir "vc_redist.x64.exe"
    Write-Host "Downloading the official Microsoft Visual C++ x64 runtime..."
    Invoke-WebRequest -Uri $VcRedistUrl -OutFile $VcRedist
}
$VcRedist = (Resolve-Path -LiteralPath $VcRedist).Path
$vcFile = Get-Item -LiteralPath $VcRedist
if ($vcFile.Length -le 0) { throw "VC++ Redistributable payload is empty: $VcRedist" }
$vcSignature = Get-AuthenticodeSignature -LiteralPath $VcRedist
if ($vcSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
    $vcSignature.SignerCertificate.Subject -notmatch '(^|, )O=Microsoft Corporation(,|$)') {
    throw "VC++ Redistributable must have a valid Microsoft Authenticode signature: $VcRedist"
}
$vcVersion = $vcFile.VersionInfo.ProductVersion
if (-not $vcVersion) { throw "VC++ Redistributable ProductVersion is missing: $VcRedist" }
$vcSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $VcRedist).Hash
Write-Host "Validated VC++ Redistributable $vcVersion ($vcSha256)"

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
    -TrainerWheelPath $TrainerWheelPath `
    -WheelBuildPython $WheelBuildPython `
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
    -WriteManifest `
    -VcRedist $VcRedist `
    -RequireVcRedist
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
$defineVcRedist = "/DVcRedist=`"$VcRedist`""
$defineVcVersion = "/DVcRedistVersion=`"$vcVersion`""

& $InnoSetup $defineSource $defineOut $defineVcRedist $defineVcVersion $issPath
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}

$installerPath = Join-Path $OutputDir "OpenDSSSetup.exe"
if (-not (Test-Path -LiteralPath $installerPath)) { throw "Installer output was not created: $installerPath" }
$installerFile = Get-Item -LiteralPath $installerPath
if ($installerFile.Length -le $vcFile.Length) { throw "Installer is too small to contain the validated VC++ runtime payload." }
$evidence = [ordered]@{
    schema_version = "opendss-installer-evidence-v1"
    installer = [ordered]@{ path = $installerPath; size = $installerFile.Length; product_version = $installerFile.VersionInfo.ProductVersion; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $installerPath).Hash }
    vc_redist = [ordered]@{ path = $VcRedist; size = $vcFile.Length; product_version = $vcVersion; sha256 = $vcSha256; signature_status = [string]$vcSignature.Status; signer = $vcSignature.SignerCertificate.Subject }
    training_bootstrap = [ordered]@{
        catalog_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\windows-py312-gpu-cu130-downloads.json")).Hash
        lock_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\windows-py312-gpu-cu130.lock")).Hash
        embedded_trainer_wheel_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\droplet_trainer-0.2.0-py3-none-any.whl")).Hash
    }
}
$evidence | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDir "installer_evidence.json") -Encoding UTF8
Write-Host "Installer created in: $OutputDir"
