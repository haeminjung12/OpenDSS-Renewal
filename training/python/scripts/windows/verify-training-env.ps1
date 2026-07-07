[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VenvPath,
    [ValidateSet("auto", "cpu", "cuda")]
    [string]$Device = "cpu",
    [string]$CheckOutput = (Join-Path (Get-Location) "outputs\trainer_env_check"),
    [switch]$AllowUntestedPython
)

$ErrorActionPreference = "Stop"
$resolvedVenv = [System.IO.Path]::GetFullPath($VenvPath)
$venvPython = Join-Path $resolvedVenv "Scripts\python.exe"
if (-not (Test-Path -LiteralPath $venvPython)) {
    Write-Error "Venv Python not found: $venvPython"
    exit 20
}

$argsList = @(
    "-m", "droplet_trainer", "env-check",
    "--device", $Device,
    "--require-training",
    "--require-onnx",
    "--check-output", ([System.IO.Path]::GetFullPath($CheckOutput)),
    "--json"
)
if ($AllowUntestedPython) {
    $argsList += "--allow-untested-python"
}

Write-Host "Command: `"$venvPython`" $($argsList -join ' ')"
& $venvPython @argsList
$code = $LASTEXITCODE
if ($code -eq 0) {
    Write-Host "Training environment verification passed."
} else {
    Write-Error "Training environment verification failed with exit code $code."
}
exit $code
