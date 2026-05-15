# Desktop Architecture

## Entry points

- `app/runtime/CMakeLists.txt`
  - top-level runtime build
  - enables CTest
  - adds `desktop_app/`
- `app/runtime/desktop_app/main.cpp`
  - process entry point
  - delegates app setup into the refactored desktop modules

## Desktop composition

The active desktop executable is `OpenVisualDropletSorter.exe`. The refactored desktop side is organized around:

- `app_options.*`
  - command-line parsing and startup flags
- `app_paths.*`
  - runtime/project/model/output path resolution
- `app_context.*`
  - shared app state passed across the desktop layer
- `main_window.*`
  - top-level window ownership, workspace registration, and major signal wiring

## Workspace controllers

Behavior for the large GUI areas is split by workspace:

- `camera_workspace_controller.*`
- `model_workspace_controller.*`
- `dataset_workspace_controller.*`
- `reports_workspace_controller.*`
- `settings_workspace_controller.*`
- `validator_workspace_controller.*`

These controllers sit over the existing `workspace_*.cpp` layout builders. The current structure keeps the layout
factories and local widget naming stable while moving more behavior out of the old monolithic entry path.

## Extracted desktop support modules

- `app_types.h`
  - shared structs and desktop-side record types
- `app_utils.*`
  - path, formatting, and helper logic
- `crash_handler.*`
  - session log tee, pruning, and crash/log hooks
- `model_registry_service.*`
  - model registry loading and selection helpers
- `live_log_writer.*`
  - live run CSV and record emission
- `sequence_summary_writer.*`
  - sequence summary output helpers

## Runtime boundary

Shared non-GUI runtime code is built as `opendss_runtime` and linked into the desktop executable. Phase 5 placed the
cross-cutting runtime pieces into that library rather than keeping them compiled only through the GUI target.

Current runtime-library sources include:

- `cli_runner.cpp`
- `dataset_capture_session.cpp`
- `dcam_camera.cpp`
- `event_detector.cpp`
- `daq_trigger.cpp`
- `fast_event_detector.cpp`
- `metadata_loader.cpp`
- `onnx_classifier.cpp`

## Tests and no-hardware verification

- `app/runtime/tests/runtime_metadata_loader_test.cpp`
  - CTest-covered no-hardware executable
  - validates metadata loading and target resolution behavior

The existing GUI verifier flags remain useful for manual validation, but the Phase 6 CI path uses the hardware-free
CTest target and keeps NI access disabled.
