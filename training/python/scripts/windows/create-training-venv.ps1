[CmdletBinding()]
param(
    [string]$Python = "py",
    [Parameter(Mandatory = $true)]
    [string]$VenvPath,
    [switch]$Force,
    [switch]$AllowUntestedPython
)

$ErrorActionPreference = "Stop"

function Invoke-SelectedPython {
    param([string[]]$Arguments)
    if ($Python -eq "py") {
        & py -3.12 @Arguments
    } else {
        & $Python @Arguments
    }
}

$resolvedVenv = [System.IO.Path]::GetFullPath($VenvPath)
if ((Test-Path -LiteralPath $resolvedVenv) -and -not $Force) {
    Write-Host "Using existing venv: $resolvedVenv"
} else {
    if (Test-Path -LiteralPath $resolvedVenv) {
        Write-Host "Recreating venv: $resolvedVenv"
        Remove-Item -LiteralPath $resolvedVenv -Recurse -Force
    } else {
        Write-Host "Creating venv: $resolvedVenv"
    }
    Invoke-SelectedPython -Arguments @("-m", "venv", $resolvedVenv)
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$venvPython = Join-Path $resolvedVenv "Scripts\python.exe"
if (-not (Test-Path -LiteralPath $venvPython)) {
    Write-Error "Venv Python was not created at $venvPython"
    exit 70
}

$versionJson = & $venvPython -c "import json,platform,struct,sys; print(json.dumps({'version': platform.python_version(), 'major': sys.version_info[0], 'minor': sys.version_info[1], 'bits': struct.calcsize('P') * 8, 'executable': sys.executable}))"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$version = $versionJson | ConvertFrom-Json

if ($version.bits -ne 64) {
    Write-Error "Python must be 64-bit. Found $($version.bits)-bit at $($version.executable)."
    exit 10
}

if (($version.major -ne 3 -or $version.minor -ne 12) -and -not $AllowUntestedPython) {
    Write-Error "Windows training setup supports Python 3.12.x. Found $($version.version). Use -AllowUntestedPython for diagnostics only."
    exit 10
}

Write-Host "Python: $($version.version) x$($version.bits)"
& $venvPython -m pip install --upgrade pip setuptools wheel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Training venv Python: $venvPython"
