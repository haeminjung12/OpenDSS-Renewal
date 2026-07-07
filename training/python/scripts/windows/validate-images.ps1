[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VenvPath,
    [Parameter(Mandatory = $true)]
    [string]$Model,
    [Parameter(Mandatory = $true)]
    [string]$Metadata,
    [Parameter(Mandatory = $true)]
    [string]$Dataset,
    [Parameter(Mandatory = $true)]
    [string]$Output,
    [ValidateSet("auto", "cpu", "cuda")]
    [string]$Device = "auto",
    [string]$Classes = "0,1",
    [switch]$PromotionGate,
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
    $LogPath = Join-Path $resolvedOutput "validate-images.stdout.json"
}

$argsList = @(
    "-m", "droplet_trainer", "validate-images",
    "--model", ([System.IO.Path]::GetFullPath($Model)),
    "--metadata", ([System.IO.Path]::GetFullPath($Metadata)),
    "--dataset", ([System.IO.Path]::GetFullPath($Dataset)),
    "--output", $resolvedOutput,
    "--classes", $Classes,
    "--device", $Device,
    "--json"
)
if ($PromotionGate) { $argsList += "--promotion-gate" }

Write-Host "Command: `"$venvPython`" $($argsList -join ' ')"
& $venvPython @argsList 2>&1 | Tee-Object -FilePath $LogPath
exit $LASTEXITCODE
