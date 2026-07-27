$ErrorActionPreference = "Stop"

$designDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path (Join-Path $designDirectory "..\..\..")).Path
$lockPath = Join-Path $designDirectory "consolidated-design-lock.json"
$lock = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json
$documentPath = Join-Path $repositoryRoot $lock.document

if (-not (Test-Path -LiteralPath $documentPath -PathType Leaf)) {
    throw "Master specification is missing: $documentPath"
}

if ($lock.canonicalization -ne "crlf-to-lf") {
    throw "Unsupported master-specification canonicalization: $($lock.canonicalization)"
}

# Git may materialize the same committed text with LF or CRLF depending on the
# checkout's core.autocrlf/eol settings. Hash canonical LF bytes so the lock
# validates identical content across worktrees while still detecting text edits.
$documentBytes = [System.IO.File]::ReadAllBytes($documentPath)
$canonicalStream = [System.IO.MemoryStream]::new()

try {
    for ($index = 0; $index -lt $documentBytes.Length; $index++) {
        if (
            $documentBytes[$index] -eq 13 -and
            ($index + 1) -lt $documentBytes.Length -and
            $documentBytes[$index + 1] -eq 10
        ) {
            $canonicalStream.WriteByte(10)
            $index++
            continue
        }

        $canonicalStream.WriteByte($documentBytes[$index])
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $actualHash = -join (
            $sha256.ComputeHash($canonicalStream.ToArray()) |
                ForEach-Object { $_.ToString("x2") }
        )
    }
    finally {
        $sha256.Dispose()
    }
}
finally {
    $canonicalStream.Dispose()
}

$expectedHash = ([string]$lock.sha256).ToLowerInvariant()

if ($actualHash -ne $expectedHash) {
    throw "SPEC_LOCK_MISMATCH expected=$expectedHash actual=$actualHash document=$($lock.document)"
}

Write-Output "SPEC_LOCK_OK document=$($lock.document) id=$($lock.document_id) sha256=$actualHash"
