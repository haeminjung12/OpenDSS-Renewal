[CmdletBinding()]
param(
    [string]$RepoRoot,
    [ValidateSet('implementation', 'debug')]
    [string]$ExpectedMode = 'implementation',
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'

function Invoke-RepoGit {
    param(
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    Push-Location -LiteralPath $WorkingDirectory
    try {
        if (Get-Command rtk -ErrorAction SilentlyContinue) {
            $output = & rtk git @Arguments 2>&1
            $tool = 'rtk git'
        }
        elseif (Get-Command git -ErrorAction SilentlyContinue) {
            $output = & git @Arguments 2>&1
            $tool = 'git fallback'
        }
        else {
            throw 'Neither rtk nor git is available.'
        }
        if ($LASTEXITCODE -ne 0) {
            throw "Git command failed: $tool $($Arguments -join ' ')`n$($output -join [Environment]::NewLine)"
        }
        [pscustomobject]@{ Tool = $tool; Lines = @($output | ForEach-Object { [string]$_ }) }
    }
    finally {
        Pop-Location
    }
}

function Get-LastNonEmptyLine {
    param([string[]]$Lines)
    return @($Lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })[-1].Trim()
}

function Get-MarkdownField {
    param([string[]]$Lines, [string]$Name)
    $pattern = '^- ' + [regex]::Escape($Name) + ':\s+(.+?)\s*$'
    foreach ($line in $Lines) {
        if ($line -match $pattern) {
            return $Matches[1].Trim().Trim('`')
        }
    }
    return $null
}

$startPath = if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    [IO.Path]::GetFullPath((Get-Location).Path)
}
else {
    [IO.Path]::GetFullPath($RepoRoot)
}
if (-not (Test-Path -LiteralPath $startPath -PathType Container)) {
    throw "Start path does not exist: $startPath"
}

$rootProbe = Invoke-RepoGit -WorkingDirectory $startPath -Arguments @('rev-parse', '--show-toplevel')
$resolvedRoot = [IO.Path]::GetFullPath((Get-LastNonEmptyLine -Lines $rootProbe.Lines))
$policyPath = Join-Path $resolvedRoot 'AGENTS.md'
$statePath = Join-Path $resolvedRoot 'docs/agent-state/current.md'
$baseRequiredFiles = @('AGENTS.md', 'docs/agent-state/current.md')
$missing = @($baseRequiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $resolvedRoot $_) -PathType Leaf) })
$mismatches = [Collections.Generic.List[string]]::new()
$policyRevision = $null
$state = [ordered]@{}

if (Test-Path -LiteralPath $policyPath -PathType Leaf) {
    foreach ($line in [IO.File]::ReadAllLines($policyPath)) {
        if ($line -match '^Ruleset revision:\s+`?([^`]+)`?\s*$') {
            $policyRevision = $Matches[1].Trim()
            break
        }
    }
}

if (Test-Path -LiteralPath $statePath -PathType Leaf) {
    $stateLines = [IO.File]::ReadAllLines($statePath)
    $state = [ordered]@{
        policyRevision = Get-MarkdownField -Lines $stateLines -Name 'Policy revision'
        mode = Get-MarkdownField -Lines $stateLines -Name 'Mode'
        userFacingLead = Get-MarkdownField -Lines $stateLines -Name 'User-facing Lead'
        checkpointBranch = Get-MarkdownField -Lines $stateLines -Name 'Checkpoint branch'
        checkpointCommit = Get-MarkdownField -Lines $stateLines -Name 'Checkpoint commit'
        activeId = Get-MarkdownField -Lines $stateLines -Name 'Active ID'
        status = Get-MarkdownField -Lines $stateLines -Name 'Status'
        updated = Get-MarkdownField -Lines $stateLines -Name 'Updated'
    }
}

$activeRecord = if ($ExpectedMode -eq 'debug') {
    'docs/debug/bug-ledger.md'
}
else {
    'docs/v2/implementation/current-slice.md'
}
if (-not (Test-Path -LiteralPath (Join-Path $resolvedRoot $activeRecord) -PathType Leaf)) {
    $missing += $activeRecord
}

$branchResult = Invoke-RepoGit -WorkingDirectory $resolvedRoot -Arguments @('rev-parse', '--abbrev-ref', 'HEAD')
$headResult = Invoke-RepoGit -WorkingDirectory $resolvedRoot -Arguments @('rev-parse', 'HEAD')
$statusResult = Invoke-RepoGit -WorkingDirectory $resolvedRoot -Arguments @('status', '--porcelain')
$branch = Get-LastNonEmptyLine -Lines $branchResult.Lines
$head = Get-LastNonEmptyLine -Lines $headResult.Lines
$dirtyLines = @($statusResult.Lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })

if ($policyRevision -and $state.policyRevision -and $policyRevision -ne $state.policyRevision) {
    $mismatches.Add("Repository policy '$policyRevision' differs from checkpoint policy '$($state.policyRevision)'.")
}
if ([string]$state.mode -ne $ExpectedMode) {
    $mismatches.Add("Canonical state mode is '$($state.mode)'; expected '$ExpectedMode'.")
}
if ($state.checkpointBranch -and $branch -ne $state.checkpointBranch) {
    $mismatches.Add("Current branch '$branch' differs from checkpoint branch '$($state.checkpointBranch)'.")
}
if ($state.checkpointCommit -and $head -ne $state.checkpointCommit) {
    $mismatches.Add("Current HEAD '$head' differs from checkpoint commit '$($state.checkpointCommit)'.")
}

$result = [ordered]@{
    schemaVersion = 2
    initializer = 'opendss-agent-rules-init'
    repoRoot = $resolvedRoot
    gitTool = $branchResult.Tool
    policyRevision = $policyRevision
    expectedMode = $ExpectedMode
    state = $state
    git = [ordered]@{
        branch = $branch
        head = $head
        dirtyPaths = $dirtyLines
    }
    requiredReads = @('docs/agent-state/current.md', $activeRecord)
    missingRequiredFiles = @($missing | Sort-Object -Unique)
    mismatches = @($mismatches)
    ready = ($missing.Count -eq 0 -and $mismatches.Count -eq 0)
}

if ($AsJson) {
    $result | ConvertTo-Json -Depth 7
}
else {
    Write-Output "OpenDSS resume: $($result.ready) | $ExpectedMode | $branch @ $head | Active: $($state.activeId)"
}
