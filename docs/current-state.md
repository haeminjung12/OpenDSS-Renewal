# Current State

Date: 2026-05-14

Active workspace for all new work:

`C:\Users\goals\Codex\OpenDSS\0. Codebase`

## Accepted Runtime State

- Active app is Qt Widgets/C++ and CMake based.
- Runtime source is `app/runtime/`.
- Desktop app source is `app/runtime/desktop_app/`.
- NI-DAQmx device discovery and Settings > Hardware device selection are accepted.
- Current connected USB-6001 was discovered in the source workspace as `Dev2` with `Dev2/ao0` and `Dev2/ao1`.
- Settings > Hardware includes `DaqDeviceComboBox`.
- No DAQ output was fired during migration.
- The older three-class model is labeled `Cell aggregate model V1 (2026-05-14)`.
- Legacy three-class target class remains `Single`.
- ONNX Runtime package compatibility was fixed before migration: the Release/package path must use the selected local ONNX Runtime DLL, not a stale PATH DLL.

## Known Open Items

- Sequence summary CSV is not created by the current sequence verifier path.
- Python trainer source now lives under `training/python`. CPU validation is planned against the local-only dataset `C:\Users\goals\Codex\CNN for Droplet Sorting\datasets\prepared\droplet_binary_2026-04-30`, and GPU validation remains a separate follow-up wave.
- Packaging is deferred from this first clean cut.
- Shell nav/header retry remains a pending UI task in the historical workspace.
- Interactive manual functionality test remained pending in the historical workspace.
