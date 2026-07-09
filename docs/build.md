# Build

Use an out-of-tree build directory. The active source root is `app/runtime`, and the desktop executable is built from
`app/runtime/desktop_app`.

## General configure

Accepted local build directory:

`C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release`

```powershell
cmake -S "C:\Users\goals\Codex\OpenDSS\0. Codebase\app\runtime" -B "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" `
  -G "Visual Studio 17 2022" -A x64 `
  -D CMAKE_PREFIX_PATH="C:\Qt\6.10.1\msvc2022_64" `
  -D ONNXRUNTIME_DIR="C:\onnxruntime-gpu" `
  -D DCAM_SDK_DIR="C:\Users\goals\Codex\CNN for Droplet Sorting\archive\python_pipeline\Hamamatsu_DCAMSDK4_v25056964\dcamsdk4" `
  -D NIDAQMX_INCLUDE_DIR="C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\include" `
  -D NIDAQMX_LIBRARY="C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc\NIDAQmx.lib" `
  -D ENABLE_NIDAQMX=ON `
  -D BUILD_QT_GUI=ON
```

## Local desktop build

```powershell
cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target desktop_app
```

This produces:

`C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe`

## Hardware-required GUI verifiers

These verifiers require the relevant camera/DAQ hardware to be connected. The Live View manual-trigger verifier fires
one configured finite DAQ output after app-owned Qt assertions pass.

```powershell
rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --verify-camera-workspace
rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --verify-daq-settings
rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --verify-live-view-manual-trigger
```

## No-hardware build and test path

For CI or local verification without NI hardware attached, keep the GUI enabled, disable NI-DAQmx, and build the
desktop target plus the hardware-free metadata loader test.

```powershell
cmake -S "C:\Users\goals\Codex\OpenDSS\0. Codebase\app\runtime" -B "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" `
  -G "Visual Studio 17 2022" -A x64 `
  -D CMAKE_PREFIX_PATH="C:\Qt\6.10.1\msvc2022_64" `
  -D ONNXRUNTIME_DIR="C:\onnxruntime-gpu" `
  -D DCAM_SDK_DIR="C:\Users\goals\Codex\CNN for Droplet Sorting\archive\python_pipeline\Hamamatsu_DCAMSDK4_v25056964\dcamsdk4" `
  -D ENABLE_NIDAQMX=OFF `
  -D BUILD_QT_GUI=ON

cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target desktop_app
cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target opendss_runtime_metadata_loader_test
ctest --test-dir "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" -C Release --output-on-failure
```

`ENABLE_NIDAQMX=OFF` avoids a CI dependency on NI-DAQmx headers, libraries, drivers, or attached NI devices. The
desktop app still expects a valid DCAM SDK install at configure/build time because the accepted runtime currently links
against `dcamapi`.

## Runtime deploy checks

```powershell
Test-Path "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\platforms\qwindows.dll"
Test-Path "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\onnxruntime.dll"
```

## Generator note

On 2026-05-14 this machine did not have the `Visual Studio 17 2022` CMake generator installed. A configure and
Release build succeeded with `Visual Studio 18 2026` in
`C:\Users\goals\Codex\OpenDSS\build-opendss-clean-verify-vs18`.

Use the same dependency paths and replace the generator with:

```powershell
-G "Visual Studio 18 2026" -A x64
```
