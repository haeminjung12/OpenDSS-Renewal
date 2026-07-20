param(
    [Parameter(Mandatory = $true)][string]$RepoRoot,
    [Parameter(Mandatory = $true)][string]$ExperimentRoot,
    [Parameter(Mandatory = $true)][string]$ArchiveRoot
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath($RepoRoot)
$experiments = [IO.Path]::GetFullPath($ExperimentRoot)
$archive = [IO.Path]::GetFullPath($ArchiveRoot)
New-Item -ItemType Directory -Force -Path $archive | Out-Null

function Get-RelativeChildPath([string]$Root, [string]$Child) {
    $prefix = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $fullChild = [IO.Path]::GetFullPath($Child)
    if (-not $fullChild.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is not inside expected root: $fullChild"
    }
    return $fullChild.Substring($prefix.Length)
}

$items = @(
    @{ Source = Join-Path $repo 'app/runtime/models/blank_squeezenet_template.onnx'; Destination = 'runtime/blank_squeezenet_template.onnx' },
    @{ Source = Join-Path $repo 'app/runtime/models/blank_squeezenet_template_metadata.json'; Destination = 'runtime/blank_squeezenet_template_metadata.json' },
    @{ Source = Join-Path $repo 'app/runtime/models/squeezenet_final_new_condition.onnx'; Destination = 'runtime/squeezenet_final_new_condition.onnx' },
    @{ Source = Join-Path $repo 'app/runtime/models/model.onnx.data'; Destination = 'runtime/model.onnx.data' },
    @{ Source = Join-Path $repo 'app/runtime/models/metadata.json'; Destination = 'runtime/metadata.json' },
    @{ Source = Join-Path $repo 'app/runtime/models/pre_binary_promotion_backup.onnx'; Destination = 'runtime/pre_binary_promotion_backup.onnx' },
    @{ Source = Join-Path $repo 'app/runtime/models/pre_binary_promotion_backup_metadata.json'; Destination = 'runtime/pre_binary_promotion_backup_metadata.json' },
    @{ Source = Join-Path $repo 'docs/worker-reports/trainer-device-and-parity-2026-07-18/phase-2-training-parity-failure-investigation.md'; Destination = 'reports/phase-2-training-parity-failure-investigation.md' },
    @{ Source = Join-Path $repo 'docs/worker-reports/publication-gpu-training-experiments-2026-07-18/phase-2c-class-2-readiness.md'; Destination = 'reports/phase-2c-class-2-readiness.md' },
    @{ Source = Join-Path $repo 'docs/worker-reports/publication-gpu-training-experiments-2026-07-18/phase-3-gpu-screening.md'; Destination = 'reports/phase-3-gpu-screening.md' },
    @{ Source = Join-Path $repo 'docs/worker-reports/publication-gpu-training-experiments-2026-07-18/phase-3b-class-boundary-tuning.md'; Destination = 'reports/phase-3b-class-boundary-tuning.md' },
    @{ Source = Join-Path $experiments 'boundary-tuning/phase3b-20260719T052025122320Z'; Destination = 'evidence/phase3b-20260719T052025122320Z' },
    @{ Source = Join-Path $experiments 'architecture-comparison/phase3c-20260719T061808895130Z/deployment-benchmark/squeezenet_reference'; Destination = 'evidence/phase3c-squeezenet-reference' }
)

$manifest = @()
foreach ($item in $items) {
    $source = [string]$item.Source
    if (-not (Test-Path -LiteralPath $source)) {
        $manifest += [ordered]@{ source = $source; destination = $item.Destination; status = 'missing'; disposition = 'Recorded as unavailable; no source was removed.' }
        continue
    }
    $destination = Join-Path $archive ([string]$item.Destination)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    if ((Get-Item -LiteralPath $source).PSIsContainer) {
        Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
        $sourceFiles = Get-ChildItem -LiteralPath $source -Recurse -File
        foreach ($sourceFile in $sourceFiles) {
            $relative = Get-RelativeChildPath $source $sourceFile.FullName
            $copy = Join-Path $destination $relative
            $sourceHash = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $copyHash = (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($sourceHash -ne $copyHash) { throw "Archive hash mismatch: $($sourceFile.FullName)" }
            $manifest += [ordered]@{ source = $sourceFile.FullName; destination = Get-RelativeChildPath $archive $copy; size = $sourceFile.Length; sha256 = $sourceHash; status = 'verified' }
        }
    } else {
        Copy-Item -LiteralPath $source -Destination $destination -Force
        $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
        $copyHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($sourceHash -ne $copyHash) { throw "Archive hash mismatch: $source" }
        $manifest += [ordered]@{ source = $source; destination = Get-RelativeChildPath $archive $destination; size = (Get-Item -LiteralPath $source).Length; sha256 = $sourceHash; status = 'verified' }
    }
}

$manifestDocument = [ordered]@{
    schema_version = 'opendss-squeezenet-legacy-archive-v1'
    copied_at_utc = [DateTime]::UtcNow.ToString('o')
    archive_root = $archive
    policy = 'Publication/reproducibility archive. Active product creation uses MobileNetV3-Small and EfficientNet-B0. Existing valid SqueezeNet registry entries remain legacy-loadable.'
    files = $manifest
}
$manifestDocument | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $archive 'archive_manifest.json') -Encoding utf8

$readme = @'
# OpenDSS Legacy SqueezeNet Archive

This versioned archive preserves the SqueezeNet runtime assets and experiment evidence used before the 2026-07-19 production architecture decision.

SqueezeNet was replaced as a **new-model** choice because its best boundary-tuned configuration did not satisfy the fixed per-class recall gate on both frozen folds. MobileNetV3-Small and EfficientNet-B0 both passed that gate and provide the active Blank and Pre-trained choices.

Nothing in this archive is an active default. Existing valid historical SqueezeNet registry entries remain supported for loading and reproducibility. The trainer implementation is retained in the repository. See `archive_manifest.json` for original locations, sizes, SHA-256 hashes, copy time, and missing-item disposition.
'@
$readme | Set-Content -LiteralPath (Join-Path $archive 'README.md') -Encoding utf8

$verified = @($manifest | Where-Object { $_.status -eq 'verified' }).Count
$missing = @($manifest | Where-Object { $_.status -eq 'missing' }).Count
[ordered]@{ archive = $archive; verified_files = $verified; missing_items = $missing } | ConvertTo-Json -Compress
