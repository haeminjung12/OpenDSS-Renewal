param(
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [Parameter(Mandatory = $true)]
    [string]$AcceptedExecutablePath,
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9A-Fa-f]{64}$")]
    [string]$AcceptedExecutableSha256,
    [string]$QtDir = "C:\Qt\6.10.1\msvc2022_64",
    [string]$OnnxDir = "C:\onnxruntime-gpu",
    [string]$VcpkgBin = "C:\vcpkg\installed\x64-windows\bin",
    [string]$ModelsDir = "",
    [string]$PreparedDatasetRoot = "",
    [string]$TrainerWheelPath = $env:OPENDSS_TRAINER_WHEEL,
    [string]$WheelBuildPython = "",
    [string]$OutputDir = "",
    [string]$PortableOutputDir = "",
    [string]$NiInstaller = "",
    [string]$VcRedist = "",
    [string]$VcRedistUrl = "https://aka.ms/vc14/vc_redist.x64.exe",
    [string]$InnoSetup = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    [switch]$CopyNidaq
)

$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $SourceRoot "..\..")).Path
$RepoParent = Split-Path -Parent $RepoRoot
$applicationMainPath =
    Join-Path $SourceRoot "Desktop_app_v2\App\main.cpp"
$applicationMain = Get-Content -LiteralPath $applicationMainPath -Raw
$appVersionMatches = [regex]::Matches(
    $applicationMain,
    'QCoreApplication::setApplicationVersion\s*\(\s*QStringLiteral\s*\(\s*"(?<version>[0-9]+(?:\.[0-9]+){1,3})"\s*\)\s*\)\s*;')
if ($appVersionMatches.Count -ne 1) {
    throw (
        "Expected exactly one numeric v2 application version in " +
        "${applicationMainPath}; found $($appVersionMatches.Count).")
}
$appVersion = $appVersionMatches[0].Groups["version"].Value

if (-not $WheelBuildPython) {
    $gpuWheelPython = Join-Path $env:LOCALAPPDATA (
        "OpenDSS\training-venv-gpu\Scripts\python.exe")
    $cpuWheelPython = Join-Path $env:LOCALAPPDATA (
        "OpenDSS\training-venv-cpu\Scripts\python.exe")
    $WheelBuildPython = if (Test-Path -LiteralPath $gpuWheelPython -PathType Leaf) {
        $gpuWheelPython
    } else {
        $cpuWheelPython
    }
}

if (-not $ModelsDir) { $ModelsDir = Join-Path $SourceRoot "models" }
if (-not $OutputDir) { $OutputDir = Join-Path $RepoParent "artifacts\internal-release\installer" }

$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
if (-not $PortableOutputDir) {
    $PortableOutputDir = Join-Path (Split-Path -Parent $OutputDir) "portable"
}
$PortableOutputDir =
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath(
        $PortableOutputDir)
if ($OutputDir.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDir must be outside the clean release repo. Got: $OutputDir"
}
if ($PortableOutputDir.StartsWith(
        $RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "PortableOutputDir must be outside the clean release repo. Got: $PortableOutputDir"
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
    $prerequisiteDir = Join-Path (Split-Path -Parent $OutputDir) "inputs"
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
    -AcceptedExecutablePath $AcceptedExecutablePath `
    -AcceptedExecutableSha256 $AcceptedExecutableSha256 `
    -QtDir $QtDir `
    -OnnxDir $OnnxDir `
    -VcpkgBin $VcpkgBin `
    -ModelsDir $ModelsDir `
    -PreparedDatasetRoot $PreparedDatasetRoot `
    -TrainerWheelPath $TrainerWheelPath `
    -WheelBuildPython $WheelBuildPython `
    -OutputDir $PortableOutputDir `
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
    -ExpectedOpenDssSha256 $AcceptedExecutableSha256 `
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
$defineAppVersion = "/DAppVersion=`"$appVersion`""

& $InnoSetup $defineSource $defineOut $defineVcRedist $defineVcVersion `
    $defineAppVersion $issPath
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}

$installerPath = Join-Path $OutputDir "OpenDSSSetup.exe"
if (-not (Test-Path -LiteralPath $installerPath)) { throw "Installer output was not created: $installerPath" }
$installerFile = Get-Item -LiteralPath $installerPath
if ($installerFile.Length -le $vcFile.Length) { throw "Installer is too small to contain the validated VC++ runtime payload." }
$evidence = [ordered]@{
    schema_version = "opendss-installer-evidence-v1"
    application_version = $appVersion
    installer = [ordered]@{ path = $installerPath; size = $installerFile.Length; product_version = $installerFile.VersionInfo.ProductVersion; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $installerPath).Hash }
    vc_redist = [ordered]@{ path = $VcRedist; size = $vcFile.Length; product_version = $vcVersion; sha256 = $vcSha256; signature_status = [string]$vcSignature.Status; signer = $vcSignature.SignerCertificate.Subject }
    training_bootstrap = [ordered]@{
        catalog_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\windows-py312-gpu-cu130-downloads.json")).Hash
        lock_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\windows-py312-gpu-cu130.lock")).Hash
        cuda_inventory_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\windows-py312-gpu-cu130-inventory.json")).Hash
        cpu_lock_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\windows-py312-cpu.lock")).Hash
        cpu_inventory_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\windows-py312-cpu-inventory.json")).Hash
        embedded_trainer_wheel_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
            Join-Path $packageDir "training\bootstrap\droplet_trainer-0.2.0-py3-none-any.whl")).Hash
    }
}
$evidence | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDir "installer_evidence.json") -Encoding UTF8
Write-Host "Installer created in: $OutputDir"
