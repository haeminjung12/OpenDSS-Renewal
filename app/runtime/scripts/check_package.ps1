param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$NiInstaller = "",
    [string]$VcRedist = "",
    [switch]$RequireInstallerInputs,
    [switch]$RequireExternalRuntimes,
    [switch]$WriteManifest,
    [string]$ManifestPath = "",
    [string]$ExpectedOnnxDir = "C:\onnxruntime-gpu",
    [string]$MinimumOnnxRuntimeMajorMinor = "1.23"
)

$ErrorActionPreference = "Stop"

function Add-CheckResult {
    param(
        [System.Collections.ArrayList]$List,
        [string]$Name,
        [string]$Status,
        [string]$Path,
        [string]$Detail = ""
    )

    [void]$List.Add([ordered]@{
        name = $Name
        status = $Status
        path = $Path
        detail = $Detail
    })
}

function Test-RequiredPath {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Name,
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        Add-CheckResult -List $List -Name $Name -Status "pass" -Path $Path
        return $true
    }

    Add-CheckResult -List $List -Name $Name -Status "fail" -Path $Path -Detail "Missing required package file."
    [void]$Errors.Add("Missing required package file: $Path")
    return $false
}

function Test-OptionalPrerequisite {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [System.Collections.ArrayList]$Warnings,
        [string]$Name,
        [string]$Path,
        [bool]$Required
    )

    if ($Path -and (Test-Path -LiteralPath $Path)) {
        Add-CheckResult -List $List -Name $Name -Status "pass" -Path $Path
        return
    }

    $message = "Missing prerequisite input: $Name ($Path)"
    if ($Required) {
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $Path -Detail $message
        [void]$Errors.Add($message)
    } else {
        Add-CheckResult -List $List -Name $Name -Status "warn" -Path $Path -Detail $message
        [void]$Warnings.Add($message)
    }
}

function Test-Hash {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [string]$Name,
        [string]$Path,
        [string]$Expected
    )

    if (-not $Expected) {
        Add-CheckResult -List $List -Name $Name -Status "skip" -Path $Path -Detail "No expected SHA256 declared."
        return
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
    if ($actual -eq $Expected.ToLowerInvariant()) {
        Add-CheckResult -List $List -Name $Name -Status "pass" -Path $Path -Detail "SHA256 matched."
    } else {
        $message = "SHA256 mismatch for $Path. Expected $Expected, got $actual."
        Add-CheckResult -List $List -Name $Name -Status "fail" -Path $Path -Detail $message
        [void]$Errors.Add($message)
    }
}

function Get-FileVersionString {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }
    $item = Get-Item -LiteralPath $Path
    return [string]$item.VersionInfo.FileVersion
}

function Test-OnnxRuntimePackage {
    param(
        [System.Collections.ArrayList]$List,
        [System.Collections.ArrayList]$Errors,
        [System.Collections.ArrayList]$Warnings,
        [string]$PackageDll,
        [string]$ExpectedDll,
        [string]$MinimumMajorMinor
    )

    if (-not (Test-Path -LiteralPath $PackageDll)) {
        return
    }

    $packageVersion = Get-FileVersionString -Path $PackageDll
    $packageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PackageDll).Hash.ToLowerInvariant()
    $detail = "FileVersion=$packageVersion; SHA256=$packageHash"
    if ($MinimumMajorMinor -and -not $packageVersion.StartsWith($MinimumMajorMinor + ".", [System.StringComparison]::OrdinalIgnoreCase)) {
        $message = "Package onnxruntime.dll version $packageVersion does not match required $MinimumMajorMinor.x."
        Add-CheckResult -List $List -Name "ONNX Runtime version" -Status "fail" -Path $PackageDll -Detail $message
        [void]$Errors.Add($message)
    } else {
        Add-CheckResult -List $List -Name "ONNX Runtime version" -Status "pass" -Path $PackageDll -Detail $detail
    }

    if ($ExpectedDll -and (Test-Path -LiteralPath $ExpectedDll)) {
        $expectedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExpectedDll).Hash.ToLowerInvariant()
        if ($packageHash -eq $expectedHash) {
            Add-CheckResult -List $List -Name "ONNX Runtime source hash" -Status "pass" -Path $PackageDll -Detail "Matches $ExpectedDll"
        } else {
            $message = "Package onnxruntime.dll hash does not match expected runtime. Expected $expectedHash from $ExpectedDll, got $packageHash."
            Add-CheckResult -List $List -Name "ONNX Runtime source hash" -Status "fail" -Path $PackageDll -Detail $message
            [void]$Errors.Add($message)
        }
    } else {
        $message = "Expected ONNX Runtime source DLL not found: $ExpectedDll"
        Add-CheckResult -List $List -Name "ONNX Runtime source hash" -Status "warn" -Path $ExpectedDll -Detail $message
        [void]$Warnings.Add($message)
    }
}

$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $PackageDir "package_manifest.json"
}

$checks = New-Object System.Collections.ArrayList
$errors = New-Object System.Collections.ArrayList
$warnings = New-Object System.Collections.ArrayList

$requiredPackageFiles = New-Object System.Collections.Generic.List[string]
foreach ($relativePath in @(
    "OpenVisualDropletSorter.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "onnxruntime.dll",
    "opencv_core4.dll",
    "opencv_imgproc4.dll",
    "opencv_imgcodecs4.dll",
    "platforms\qwindows.dll",
    "models\model_registry.json"
)) {
    $requiredPackageFiles.Add($relativePath)
}

$packageRegistryPath = Join-Path $PackageDir "models\model_registry.json"
if (Test-Path -LiteralPath $packageRegistryPath) {
    $packageRegistry = Get-Content -LiteralPath $packageRegistryPath -Raw | ConvertFrom-Json
    foreach ($entry in @($packageRegistry.entries)) {
        foreach ($assetPath in @($entry.model_path, $entry.metadata_path)) {
            if ($assetPath) {
                $requiredPackageFiles.Add("models\" + (Split-Path -Leaf $assetPath))
            }
        }
        foreach ($sidecar in @($entry.model_sidecars)) {
            if ($sidecar.required -and $sidecar.path) {
                $requiredPackageFiles.Add("models\" + (Split-Path -Leaf $sidecar.path))
            }
        }
    }
}
$requiredPackageFiles = @($requiredPackageFiles | Select-Object -Unique)

foreach ($relativePath in $requiredPackageFiles) {
    Test-RequiredPath -List $checks -Errors $errors -Name $relativePath -Path (Join-Path $PackageDir $relativePath) | Out-Null
}

$registryPath = Join-Path $PackageDir "models\model_registry.json"
if (Test-Path -LiteralPath $registryPath) {
    $registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json
    foreach ($entry in @($registry.entries)) {
        $modelName = Split-Path -Leaf $entry.model_path
        $metadataName = Split-Path -Leaf $entry.metadata_path
        if ($modelName) {
            Test-Hash -List $checks -Errors $errors -Name ("registry model hash: " + $entry.registry_entry_id) -Path (Join-Path $PackageDir ("models\" + $modelName)) -Expected $entry.model_sha256
        }
        if ($metadataName) {
            Test-Hash -List $checks -Errors $errors -Name ("registry metadata hash: " + $entry.registry_entry_id) -Path (Join-Path $PackageDir ("models\" + $metadataName)) -Expected $entry.metadata_sha256
        }
        foreach ($sidecar in @($entry.model_sidecars)) {
            if ($sidecar.required) {
                Test-Hash -List $checks -Errors $errors -Name ("registry sidecar hash: " + $entry.registry_entry_id) -Path (Join-Path $PackageDir ("models\" + (Split-Path -Leaf $sidecar.path))) -Expected $sidecar.sha256
            }
        }
    }
    $currentEntry = $registry.entries | Where-Object { $_.state -eq "promoted_current" } | Select-Object -First 1
    if ($currentEntry) {
        Add-CheckResult -List $checks -Name "model registry promoted entry" -Status "pass" -Path $registryPath -Detail ("promoted_current=" + $currentEntry.registry_entry_id)
    } else {
        $message = "No promoted_current entry found in model registry."
        Add-CheckResult -List $checks -Name "model registry promoted entry" -Status "fail" -Path $registryPath -Detail $message
        [void]$errors.Add($message)
    }
}

$packageOnnxRuntimeDll = Join-Path $PackageDir "onnxruntime.dll"
$expectedOnnxRuntimeDll = Join-Path $ExpectedOnnxDir "lib\onnxruntime.dll"
Test-OnnxRuntimePackage -List $checks -Errors $errors -Warnings $warnings -PackageDll $packageOnnxRuntimeDll -ExpectedDll $expectedOnnxRuntimeDll -MinimumMajorMinor $MinimumOnnxRuntimeMajorMinor

$systemDir = [Environment]::SystemDirectory
$nidaqRuntime = Join-Path $systemDir "nicaiu.dll"
$dcamRuntime = Join-Path $systemDir "dcamapi.dll"
Test-OptionalPrerequisite -List $checks -Errors $errors -Warnings $warnings -Name "NI-DAQmx runtime nicaiu.dll" -Path $nidaqRuntime -Required ([bool]$RequireExternalRuntimes)
Test-OptionalPrerequisite -List $checks -Errors $errors -Warnings $warnings -Name "Hamamatsu DCAM runtime dcamapi.dll" -Path $dcamRuntime -Required ([bool]$RequireExternalRuntimes)

if ($RequireInstallerInputs) {
    Test-OptionalPrerequisite -List $checks -Errors $errors -Warnings $warnings -Name "NI-DAQmx installer input" -Path $NiInstaller -Required $true
    Test-OptionalPrerequisite -List $checks -Errors $errors -Warnings $warnings -Name "VC++ Redistributable installer input" -Path $VcRedist -Required $true
}

$manifest = [ordered]@{
    schema_version = "open-visual-droplet-sorter-package-manifest-v1"
    generated_at = (Get-Date).ToString("o")
    package_dir = $PackageDir
    source_root = $SourceRoot
    status = $(if ($errors.Count -eq 0) { "pass" } else { "fail" })
    checks = $checks
    warnings = $warnings
    errors = $errors
}

if ($WriteManifest) {
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ManifestPath -Encoding UTF8
    Write-Host "Package manifest written to: $ManifestPath"
}

foreach ($warning in $warnings) {
    Write-Warning $warning
}

if ($errors.Count -gt 0) {
    foreach ($failure in $errors) {
        Write-Error $failure -ErrorAction Continue
    }
    exit 1
}

Write-Host "Package check passed: $PackageDir"
exit 0
