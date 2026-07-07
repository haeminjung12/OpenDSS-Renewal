[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VenvPath,
    [Parameter(Mandatory = $true)]
    [string]$Dataset,
    [Parameter(Mandatory = $true)]
    [string]$Output,
    [string]$Classes = "0,1",
    [int]$MinPerClass = 10,
    [string]$Split = "train=0.70,val=0.15,test=0.15",
    [int]$Seed = 42,
    [switch]$WriteManifest,
    [switch]$Prepare,
    [string]$PreparedRoot
)

$ErrorActionPreference = "Stop"
$venvPython = Join-Path ([System.IO.Path]::GetFullPath($VenvPath)) "Scripts\python.exe"
if (-not (Test-Path -LiteralPath $venvPython)) {
    Write-Error "Venv Python not found: $venvPython"
    exit 20
}

$argsList = @(
    "-m", "droplet_trainer", "dataset-validate",
    "--dataset", ([System.IO.Path]::GetFullPath($Dataset)),
    "--output", ([System.IO.Path]::GetFullPath($Output)),
    "--classes", $Classes,
    "--min-per-class", "$MinPerClass",
    "--split", $Split,
    "--seed", "$Seed",
    "--json"
)
if ($WriteManifest) { $argsList += "--write-manifest" }
if ($Prepare) { $argsList += "--prepare" }
if ($PreparedRoot) { $argsList += @("--prepared-root", ([System.IO.Path]::GetFullPath($PreparedRoot))) }

Write-Host "Command: `"$venvPython`" $($argsList -join ' ')"
& $venvPython @argsList
exit $LASTEXITCODE
