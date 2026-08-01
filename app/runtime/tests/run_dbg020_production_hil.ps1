[CmdletBinding()]
param(
  [ValidateRange(0.1,30.0)][double]$Duration = 3,
  [ValidateRange(1,2304)][int]$RoiWidth = 44,
  [ValidateRange(1,2304)][int]$RoiHeight = 144,
  [ValidateSet(8,16)][int]$CameraBitDepth = 8,
  [ValidateRange(0.001,10000.0)][double]$ExposureMs = 0.1,
  [Parameter(Mandatory)][string]$OutputRoot,
  [Parameter(Mandatory)][string]$Report,
  [string]$ExePath = 'C:\b\odss-debug-lead-hil\desktop_app\tests\Release\opendss_dbg019_sequence_persistence_headless.exe',
  [string]$QtBin = 'C:\Qt\6.11.1\msvc2022_64\bin'
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
  throw "Harness executable not found: $ExePath"
}
if (-not (Test-Path -LiteralPath (Join-Path $QtBin 'Qt6Core.dll') -PathType Leaf)) {
  throw "Qt6Core.dll not found under QtBin: $QtBin"
}
if (-not (Test-Path -LiteralPath $OutputRoot -PathType Container)) {
  throw "Output root must already exist: $OutputRoot"
}

$cameraUsers = Get-CimInstance Win32_Process | Where-Object {
  $_.ProcessId -ne $PID -and
  $_.Name -match 'OpenDSS|desktop_app' -and
  $_.CommandLine -notmatch 'opendss_dbg019_sequence_persistence_headless'
}
if ($cameraUsers) {
  $details = ($cameraUsers | ForEach-Object {
    "$($_.ProcessId): $($_.Name) $($_.CommandLine)"
  }) -join [Environment]::NewLine
  throw "Refusing camera HIL: another OpenDSS/camera process may own the device.`n$details"
}

$priorPath = $env:PATH
try {
  $env:PATH = "$QtBin;$priorPath"
  & $ExePath --writer production --duration $Duration `
    --roi-width $RoiWidth --roi-height $RoiHeight `
    --bit-depth $CameraBitDepth --exposure-ms $ExposureMs `
    --output-root $OutputRoot --report $Report
  if ($LASTEXITCODE -ne 0) {
    throw "Production Image Sequence HIL exited with code $LASTEXITCODE."
  }

  $result = Get-Content -LiteralPath $Report -Raw | ConvertFrom-Json
  $countsMatch = $result.acquired_count -eq $result.detector_completed_count -and
    $result.acquired_count -eq $result.offer_accepted_count -and
    $result.acquired_count -eq $result.saved_frame_count
  $integrityPass = $result.pass -and $countsMatch -and
    $result.source_frame_gaps -eq 0 -and $result.duplicates -eq 0 -and
    $result.out_of_order -eq 0 -and $result.detector_ordering_faults -eq 0 -and
    $result.offer_rejected_count -eq 0 -and
    $result.manifest_queue_rejections -eq 0 -and
    $result.manifest_consumer_failures -eq 0
  if (-not $integrityPass) {
    throw 'Every-frame production integrity failed; inspect the JSON report.'
  }

  [PSCustomObject]@{
    Result = 'PASS'
    Acquired = $result.acquired_count
    DetectorCompleted = $result.detector_completed_count
    Saved = $result.saved_frame_count
    AcquisitionFps = $result.acquisition_fps
    DetectorCompletionFps = $result.detector_completion_fps
    PersistenceQueueHighWater = $result.save_queue_high_water
    Report = $Report
  }
}
finally {
  $env:PATH = $priorPath
}
