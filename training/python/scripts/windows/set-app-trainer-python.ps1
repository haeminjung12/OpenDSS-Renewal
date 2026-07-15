[CmdletBinding(DefaultParameterSetName = "ByVenv")]
param(
    [Parameter(Mandatory = $true, ParameterSetName = "ByVenv")]
    [string]$VenvPath,
    [Parameter(Mandatory = $true, ParameterSetName = "ByPython")]
    [string]$PythonPath
)

$ErrorActionPreference = "Stop"

if ($PSCmdlet.ParameterSetName -eq "ByVenv") {
    $resolvedVenv = [System.IO.Path]::GetFullPath($VenvPath)
    $resolvedPython = Join-Path $resolvedVenv "Scripts\python.exe"
} else {
    $resolvedPython = [System.IO.Path]::GetFullPath($PythonPath)
}

if (-not (Test-Path -LiteralPath $resolvedPython)) {
    Write-Error "Python executable not found: $resolvedPython"
    exit 20
}

$settingsKey = "HKCU:\Software\Hamamatsu\OpenVisualDropletSorter\settings"
if (-not (Test-Path -LiteralPath $settingsKey)) {
    New-Item -Path $settingsKey -Force | Out-Null
}

Set-ItemProperty -Path $settingsKey -Name "pythonTrainer" -Value $resolvedPython
Write-Host "OpenDSS trainer Python set to: $resolvedPython"
