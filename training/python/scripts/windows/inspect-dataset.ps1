[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VenvPath,
    [Parameter(Mandatory = $true)]
    [string]$Dataset,
    [string]$Classes = "0,1",
    [string]$ClassSchema
)

$ErrorActionPreference = "Stop"
$venvPython = Join-Path ([System.IO.Path]::GetFullPath($VenvPath)) "Scripts\python.exe"
if (-not (Test-Path -LiteralPath $venvPython)) {
    Write-Error "Venv Python not found: $venvPython"
    exit 20
}

$argsList = @("-m", "droplet_trainer", "dataset-inspect", "--dataset", ([System.IO.Path]::GetFullPath($Dataset)), "--classes", $Classes, "--json")
if ($ClassSchema) {
    $argsList += @("--class-schema", ([System.IO.Path]::GetFullPath($ClassSchema)))
}

Write-Host "Command: `"$venvPython`" $($argsList -join ' ')"
& $venvPython @argsList
exit $LASTEXITCODE
