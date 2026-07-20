[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VenvPath,
    [string]$TrainerSourcePath = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path,
    [string]$CheckOutput = (Join-Path (Get-Location) "outputs\trainer_env_check")
)

$ErrorActionPreference = "Stop"
$resolvedVenv = [System.IO.Path]::GetFullPath($VenvPath)
$venvPython = Join-Path $resolvedVenv "Scripts\python.exe"
if (-not (Test-Path -LiteralPath $venvPython)) {
    Write-Error "Venv Python not found: $venvPython"
    exit 20
}

$requirements = Resolve-Path (Join-Path $PSScriptRoot "..\..\requirements\windows-py312-cpu.txt")
$source = Resolve-Path $TrainerSourcePath

Write-Host "Installing CPU training dependencies from $requirements"
& $venvPython -m pip uninstall -y onnxruntime-gpu onnxruntime
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $venvPython -m pip install -r $requirements
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Installing droplet_trainer from $source"
& $venvPython -m pip install $source
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot "verify-training-env.ps1") -VenvPath $resolvedVenv -Device cpu -CheckOutput $CheckOutput
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot "set-app-trainer-python.ps1") -VenvPath $resolvedVenv
exit $LASTEXITCODE
