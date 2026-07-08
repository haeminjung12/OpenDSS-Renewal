# DCAM Live Viewer (Hamamatsu)

Qt desktop application for live viewing, capture control, and offline review of Hamamatsu cameras via the DCAM SDK.
This build also integrates the C++ event-detection + ONNX inference + NI-DAQ trigger pipeline (see the Pipeline tab).

## What it does
- Streams a live camera feed with zoom/pan and scrollbars
- Shows real-time stats (resolution, FPS, dropped frames, readout speed)
- Lets you set resolution presets or custom sizes, binning (incl. independent), exposure (ms), bit depth (8/12/16), and readout speed
- Records frames to disk as timestamped TIFF sequences with a save progress dialog
- Captures a single frame to TIFF on demand
- Writes capture metadata to `capture_info.txt` (fps, resolution, exposure, etc.)
- Logs to `session_log.txt` (cleared on startup)
- Viewer window for saved sequences with slider, recent folders, and keyboard navigation
- Viewer-only mode if the camera fails to initialize at startup
- Pipeline tab: load ONNX + metadata, run event detection + classification, and fire NI-DAQ triggers

Tested with Hamamatsu ORCA-Fusion C14440

## Viewer controls
- Left/Right: previous/next frame
- Ctrl+Left/Right: jump 5 frames
- PageUp/PageDown: jump 10 frames

## Build
- Qt: `C:\Qt\6.10.1\msvc2022_64`
- DCAM SDK headers/libs are expected in `Hamamatsu_DCAMSDK4_v25056964`
- OpenCV + ONNX Runtime (for inference pipeline)

The GUI builds as `OpenVisualDropletSorter.exe` and is the main entry point for the project.

## Pipeline setup
- Place `squeezenet_final_new_condition.onnx` (and `metadata.json`) under `app/runtime/models`
  or browse to them in the Pipeline tab.
- Build with MSBuild (Release)

## Standalone package
Use `package_portable.ps1` to create a portable package outside the repo and deploy Qt runtimes with `windeployqt`.
The package now includes `training/python/` beside the executable so Trainer and Validator workflows can use the migrated trainer source, Windows wrapper scripts, requirements files, and Windows training README from the packaged layout.
The portable package does not bundle Python, PyTorch, CUDA, or a trainer `venv`; users install those externally by running the packaged `training/python/scripts/windows/*.ps1` helpers from a user-managed environment.
The portable package relies on the system DCAM runtime (no bundled `dcamapi.dll`).
