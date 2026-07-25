# OpenDSS v2 functional slice — accepted headless backend integration

## Status

**Accepted:** July 24, 2026  
**Authorization:** The user explicitly authorized integrating accepted backend tip `3ed8025016a14dcb0b7e7fe5cf6f0c7738f99720` onto accepted GUI/design baseline `ea48ec3078adc068d57dd58e563e24f642289480` in a clean worktree, without requiring GUI completion.  
**Integrated commit:** `0b5bda6bc0373b3fd8927db103d535f388d9c5f3`

The merge was conflict-free and has exactly those two parents. Independent review confirmed an empty remerge diff: the result is the direct union of the accepted GUI and backend parents with no manual resolution delta.

## Integrated functional baseline

The accepted headless backend lineage now supplies:

- authoritative application state and operation/resource ownership;
- Camera and Single Image service contracts;
- Dataset, Sequence, Run, and model-package artifact contracts;
- Image Sequence and Droplet Dataset Capture services;
- Dataset Label and Sequence Viewer services;
- Training, model activation/loading, and Model Test services;
- software Sequence Test, Run persistence/recovery, and Results repository state;
- bounded Live sorting with silent dropped-frame continuation and factual integrity metadata;
- DAQ-LIVE binding with DAQ Output OFF by default, approved `Dev1/ao0`, 1 kHz, 5 ms, 0 ms delay, and fixed 5 Vpp mapped to 2.5 V peak; and
- application Settings persistence for storage root and Text Size.

These are backend/service contracts. They are not yet production GUI/controller wiring.

## Protected integration boundary

- Accepted `*.ui.qml` forms, ordinary QML wrappers, `MockDatas`, visual assets/tokens, `qds.cmake`, generated content CMake, `.qmlproject`, and `.qtds` files match the `ea48ec3` parent exactly.
- Backend paths match the `3ed8025` parent exactly.
- Protected `live_data_collection_writer.*` changes are inherited unchanged from the previously accepted backend lineage.
- No shared dirty working-tree file was staged, overwritten, absorbed, or cleaned.

## Validation

- Independent merge review: no findings.
- Fresh Qt 6.11.1 MSVC/Ninja configure: passed with the qualified Qt, vcpkg/OpenCV/Protobuf, ONNX Runtime, and DCAM SDK paths; NI-DAQmx was disabled.
- Focused targets built: `daq_service_test`, `live_sorting_service_test`, `operation_coordinator_test`, and `settings_repository_test`.
- Focused CTest result: 4/4 passed.
- No GUI was launched and no physical hardware was accessed.

## Out of scope

- GUI/controller or ordinary-QML integration;
- any accepted visual-form alias, signal, property, state, layout, token, or asset change;
- physical DAQ output, camera operation, or other HIL;
- Setup Profile schema/persistence;
- model-export or protected vendor-mechanics changes;
- broad legacy, Python, GUI, or hardware test suites; and
- selection or implementation of another slice.

## Next gate

This slice is complete. A later slice requires explicit user authorization and must name its exact interface and write boundaries. The simplest accepted integration is the direct two-parent merge plus these durable records; no integration-specific source, adapter, compatibility path, or framework was added.
