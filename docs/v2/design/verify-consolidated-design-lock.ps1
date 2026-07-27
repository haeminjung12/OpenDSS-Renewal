$ErrorActionPreference = "Stop"

$designDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path (Join-Path $designDirectory "..\..\..")).Path
$lockPath = Join-Path $designDirectory "consolidated-design-lock.json"
$lock = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json
$documentPath = Join-Path $repositoryRoot $lock.document

if (-not (Test-Path -LiteralPath $documentPath -PathType Leaf)) {
    throw "Master specification is missing: $documentPath"
}

$actualHash = (Get-FileHash -LiteralPath $documentPath -Algorithm SHA256).Hash.ToLowerInvariant()
$expectedHash = ([string]$lock.sha256).ToLowerInvariant()

if ($actualHash -ne $expectedHash) {
    throw "SPEC_LOCK_MISMATCH expected=$expectedHash actual=$actualHash document=$($lock.document)"
}

Write-Output "SPEC_LOCK_OK document=$($lock.document) id=$($lock.document_id) sha256=$actualHash"
