[CmdletBinding()]
param(
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..\..").Path,
    [string]$PythonExecutable = (
        Join-Path $env:LOCALAPPDATA "OpenDSS\training-venv-gpu\Scripts\python.exe"
    ),
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$python = (Resolve-Path -LiteralPath $PythonExecutable).Path
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath(
    $OutputDirectory)
$lockPath = Join-Path $source "requirements\windows-py312-gpu-cu130.lock"
$catalogPath = Join-Path $source "requirements\windows-py312-gpu-cu130-downloads.json"
$catalog = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
$filename = [string]$catalog.embedded_wheel
$lockLine = @(
    Get-Content -LiteralPath $lockPath |
        Where-Object { $_ -match "^droplet-trainer==0[.]2[.]0\s" }
)
if ($lockLine.Count -ne 1 -or
    $lockLine[0] -notmatch "--hash=sha256:([0-9a-fA-F]{64})$") {
    throw "The authoritative droplet-trainer lock entry is missing or invalid."
}
$expectedHash = $Matches[1].ToLowerInvariant()
if ($filename -ne "droplet_trainer-0.2.0-py3-none-any.whl") {
    throw "The bootstrap catalog does not name the accepted trainer wheel."
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
$token = [Guid]::NewGuid().ToString("N")
$candidateRoot = Join-Path $output ".trainer-wheel-candidate-$token"
$candidateWheel = Join-Path $candidateRoot $filename
$publishedWheel = Join-Path $output $filename

$oldPythonHome = $env:PYTHONHOME
$oldPythonPath = $env:PYTHONPATH
$oldPythonNoUserSite = $env:PYTHONNOUSERSITE
$oldPipNoIndex = $env:PIP_NO_INDEX
$oldSourceDateEpoch = $env:SOURCE_DATE_EPOCH
try {
    New-Item -ItemType Directory -Path $candidateRoot -Force | Out-Null
    $env:PYTHONHOME = $null
    $env:PYTHONPATH = $null
    $env:PYTHONNOUSERSITE = "1"
    $env:PIP_NO_INDEX = "1"
    $env:SOURCE_DATE_EPOCH = "1753574400"

    & $python -I -m pip wheel --disable-pip-version-check --no-index --no-deps `
        --no-build-isolation --wheel-dir $candidateRoot $source
    if ($LASTEXITCODE -ne 0) {
        throw "The deterministic trainer wheel build failed with exit code $LASTEXITCODE."
    }
    $wheels = @(Get-ChildItem -LiteralPath $candidateRoot -File -Filter "*.whl")
    if ($wheels.Count -ne 1 -or $wheels[0].Name -ne $filename) {
        throw "The trainer build did not produce exactly $filename."
    }
    $actualHash = (Get-FileHash -LiteralPath $candidateWheel -Algorithm SHA256).Hash.
        ToLowerInvariant()
    if ($actualHash -ne $expectedHash) {
        throw "Deterministic trainer wheel hash mismatch. Expected $expectedHash; computed $actualHash."
    }

    if (Test-Path -LiteralPath $publishedWheel) {
        $publishedHash = (Get-FileHash -LiteralPath $publishedWheel -Algorithm SHA256).
            Hash.ToLowerInvariant()
        if ($publishedHash -ne $expectedHash) {
            throw "Refusing to replace a conflicting staged trainer wheel: $publishedWheel"
        }
    } else {
        Move-Item -LiteralPath $candidateWheel -Destination $publishedWheel
    }
    Write-Output $publishedWheel
} finally {
    if (Test-Path -LiteralPath $candidateRoot) {
        Remove-Item -LiteralPath $candidateRoot -Recurse -Force
    }
    $env:PYTHONHOME = $oldPythonHome
    $env:PYTHONPATH = $oldPythonPath
    $env:PYTHONNOUSERSITE = $oldPythonNoUserSite
    $env:PIP_NO_INDEX = $oldPipNoIndex
    $env:SOURCE_DATE_EPOCH = $oldSourceDateEpoch
}
