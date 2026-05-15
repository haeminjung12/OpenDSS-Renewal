# Dependencies

This repo is the clean runtime-only OpenDSS codebase. The active build root is `app/runtime`, and the desktop app is
`app/runtime/desktop_app`.

## Required build-time dependencies

- CMake 3.19 or newer
- MSVC toolchain for the selected Visual Studio generator
- Qt 6 Widgets
- OpenCV with `core`, `imgproc`, and `imgcodecs`
- ONNX Runtime
- Hamamatsu DCAM SDK headers and import library

## Optional build-time dependencies

- NI-DAQmx C API headers and import library
  - Controlled by `ENABLE_NIDAQMX`
  - When disabled or unavailable, the build uses the stub trigger path

## Runtime dependencies

- Qt runtime deployment for the desktop executable
- `onnxruntime.dll` beside the desktop executable
- OpenCV runtime DLLs reachable from the process `PATH`
- Hamamatsu DCAM runtime/driver installed on machines that use camera features
- NI-DAQmx runtime/driver installed only for NI-enabled trigger use

## Current local paths

These are the accepted local paths recorded for this workspace:

- Qt: `C:\Qt\6.10.1\msvc2022_64`
- ONNX Runtime: `C:\onnxruntime-gpu`
- DCAM SDK: `C:\Users\goals\Codex\CNN for Droplet Sorting\archive\python_pipeline\Hamamatsu_DCAMSDK4_v25056964\dcamsdk4`
- NI-DAQmx include: `C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\include`
- NI-DAQmx library: `C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc\NIDAQmx.lib`

## CI assumptions

The Phase 6 Windows workflow is intentionally hardware-free:

- no physical camera is required
- no NI device is required
- `ENABLE_NIDAQMX=OFF` is expected

The current accepted code still requires a Windows runner with Qt, OpenCV, ONNX Runtime, and the Hamamatsu DCAM SDK
installed, because the desktop target and runtime library still compile and link against `dcamapi`.
