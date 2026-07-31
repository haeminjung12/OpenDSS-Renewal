[CmdletBinding()]
param(
    [string]$RepoRoot,
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
$skillsRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$resumeScript = Join-Path $skillsRoot 'opendss-agent-rules-init\scripts\Initialize-OpenDssAgentRules.ps1'
if (-not (Test-Path -LiteralPath $resumeScript -PathType Leaf)) {
    throw "OpenDSS resume verifier is missing: $resumeScript"
}

$arguments = @{ AsJson = $true; ExpectedMode = 'debug' }
if (-not [string]::IsNullOrWhiteSpace($RepoRoot)) {
    $arguments.RepoRoot = $RepoRoot
}
$base = ((& $resumeScript @arguments) -join [Environment]::NewLine) | ConvertFrom-Json

$result = [ordered]@{
    schemaVersion = 2
    initializer = 'opendss-debug-init'
    repoRoot = $base.repoRoot
    gitTool = $base.gitTool
    policyRevision = $base.policyRevision
    expectedMode = 'debug'
    state = $base.state
    git = $base.git
    requiredReads = @('docs/agent-state/current.md', 'docs/debug/bug-ledger.md')
    missingRequiredFiles = @($base.missingRequiredFiles)
    mismatches = @($base.mismatches)
    ready = [bool]$base.ready
}

if ($AsJson) {
    $result | ConvertTo-Json -Depth 7
}
else {
    Write-Output "OpenDSS debug resume: $($result.ready) | $($base.git.branch) @ $($base.git.head) | Active: $($base.state.activeId)"
}
