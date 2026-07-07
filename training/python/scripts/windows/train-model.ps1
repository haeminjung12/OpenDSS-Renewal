[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VenvPath,
    [Parameter(Mandatory = $true)]
    [string]$Dataset,
    [Parameter(Mandatory = $true)]
    [string]$Output,
    [ValidateSet("auto", "cpu", "cuda")]
    [string]$Device = "auto",
    [string]$Classes = "0,1",
    [string]$Config,
    [string]$RunName,
    [string]$LogPath
)

$ErrorActionPreference = "Stop"
$venvPython = Join-Path ([System.IO.Path]::GetFullPath($VenvPath)) "Scripts\python.exe"
if (-not (Test-Path -LiteralPath $venvPython)) {
    Write-Error "Venv Python not found: $venvPython"
    exit 20
}

$resolvedOutput = [System.IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $resolvedOutput | Out-Null
if (-not $LogPath) {
    $LogPath = Join-Path $resolvedOutput "train-model.stdout.jsonl"
}

$argsList = @(
    "-m", "droplet_trainer", "train",
    "--dataset", ([System.IO.Path]::GetFullPath($Dataset)),
    "--output", $resolvedOutput,
    "--classes", $Classes,
    "--device", $Device,
    "--jsonl"
)
if ($Config) { $argsList += @("--config", ([System.IO.Path]::GetFullPath($Config))) }
if ($RunName) { $argsList += @("--run-name", $RunName) }

Write-Host "Command: `"$venvPython`" $($argsList -join ' ')"
& $venvPython @argsList 2>&1 | Tee-Object -FilePath $LogPath
exit $LASTEXITCODE
