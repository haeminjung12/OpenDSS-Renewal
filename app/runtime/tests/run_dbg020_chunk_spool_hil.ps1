[CmdletBinding()]
param(
  [ValidateRange(0.1,30.0)][double]$Duration = 3,
  [ValidateRange(1,2304)][int]$RoiWidth = 144,
  [ValidateRange(1,2304)][int]$RoiHeight = 144,
  [string]$OutputRoot,
  [string]$Report,
  [string]$ExePath = 'C:\b\odss-debug-lead-hil\desktop_app\tests\Release\opendss_dbg019_sequence_persistence_headless.exe',
  [string]$QtBin = 'C:\Qt\6.11.1\msvc2022_64\bin',
  [ValidateSet(8,16)][int]$CameraBitDepth = 8,
  [ValidateRange(1048576,1073741824)][Int64]$ChunkBytes = 33554432,
  [ValidateRange(1,1000)][int]$FlushMs = 50,
  [switch]$FinalizeSpool
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) { throw "Harness executable not found: $ExePath" }
if (-not (Test-Path -LiteralPath (Join-Path $QtBin 'Qt6Core.dll') -PathType Leaf)) { throw "Qt6Core.dll not found under QtBin: $QtBin" }
if (-not $OutputRoot -or -not $Report) { throw 'OutputRoot and Report are required.' }
if (-not (Test-Path -LiteralPath $OutputRoot -PathType Container)) { throw "Output root must already exist: $OutputRoot" }
$cameraUsers = Get-CimInstance Win32_Process | Where-Object {
  $_.ProcessId -ne $PID -and $_.Name -match 'OpenDSS|desktop_app' -and $_.CommandLine -notmatch 'opendss_dbg019_sequence_persistence_headless'
}
if ($cameraUsers) {
  $details = ($cameraUsers | ForEach-Object { "$($_.ProcessId): $($_.Name) $($_.CommandLine)" }) -join [Environment]::NewLine
  throw "Refusing camera HIL: another OpenDSS/camera process may own the device.`n$details"
}
$priorPath = $env:PATH
try {
  $env:PATH = "$QtBin;$priorPath"
  $arguments = @('--writer', 'chunk-spool', '--duration', $Duration, '--roi-width', $RoiWidth, '--roi-height', $RoiHeight, '--bit-depth', $CameraBitDepth, '--output-root', $OutputRoot, '--report', $Report, '--chunk-bytes', $ChunkBytes, '--flush-ms', $FlushMs)
if ($FinalizeSpool) { $arguments += '--finalize-spool' }
& $ExePath @arguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$result = Get-Content -LiteralPath $Report -Raw | ConvertFrom-Json
if (-not $result.pass -or $result.source_frame_gaps -ne 0 -or $result.out_of_order -ne 0 -or $result.detector_ordering_faults -ne 0 -or $result.offer_rejected_count -ne 0 -or $result.chunk_spool.rejected_frame_count -ne 0 -or $result.chunk_spool.failure_count -ne 0 -or $result.chunk_spool.accepted_frame_count -ne $result.chunk_spool.persisted_frame_count) {
  throw 'DBG-020 chunk-spool integrity validation failed; inspect the JSON report.'
}
if ($FinalizeSpool -and ($result.chunk_spool_finalization.order_fault_count -ne 0 -or $result.chunk_spool_finalization.header_fault_count -ne 0 -or $result.chunk_spool_finalization.record_count -ne $result.chunk_spool.persisted_frame_count -or $result.chunk_spool_finalization.written_count -ne $result.chunk_spool_finalization.record_count -or $result.chunk_spool_finalization.readable_count -ne $result.chunk_spool_finalization.record_count)) {
  throw 'DBG-020 spool finalization validation failed; inspect the JSON report.'
}
}
finally {
  $env:PATH = $priorPath
}
