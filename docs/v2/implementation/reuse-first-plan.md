# OpenDSS v2 Reuse-First Implementation Plan

**Status:** Implementation Plan for Review  
**Created:** 2026-07-21  
**Source baseline:** `docs/v2/`  
**Audit evidence:** `docs/v2/audits/reusable-core-audit.md`

## Purpose and authority

This plan converts the reusable-core audit into small, ordered implementation work packages. The audit is evidence, not an approved implementation decision. Each package requires its own review, characterization evidence, and acceptance decision before later packages may rely on it.

The source-authority order in `docs/v2/README.md` and repository rules in `AGENTS.md` apply throughout. In particular, existing code is reusable implementation evidence, not authority for v2 product behavior, navigation, terminology, application state, scientific policy, or exposed configuration.

Production implementation is outside this planning commit. The future packages below must use these disposition terms consistently:

- **Preserve unchanged:** retain qualified behavior and implementation without behavioral modification.
- **Preserve behind adapter:** retain implementation mechanics and place a typed boundary around them.
- **Extract reusable mechanics:** separate proven technical behavior from its current UI, schema, or policy owner.
- **Replace obsolete application policy:** implement behavior required by the v2 baseline instead of carrying forward v1 policy.
- **Retire after characterization:** remove only after tests and measurements prove required behavior is retained and the removal is separately reviewed.
- **Hardware qualification required:** compilation and software tests are insufficient; representative Windows/vendor-hardware evidence is mandatory.

## Common delivery rules

Each work package is a separate pull request unless its package explicitly identifies a two-PR evidence gate. Every PR must:

1. preserve protected technical assets unless the package supplies characterization, comparison, justification, and rollback evidence;
2. avoid restoring detector, crop, routing, internal timing, training-hyperparameter, manual Python, or manual CPU/GPU controls in the v2 UI;
3. keep UI/QML free of DCAM, NI-DAQmx, ONNX Runtime, and trainer-process calls;
4. keep Predicted Class, Decision, Observed Route, and physical DAQ command as distinct contracts;
5. identify any changed file format or schema and provide versioned fixtures or migration behavior;
6. record Windows, camera, DAQ, GPU, and timing limitations honestly;
7. preserve a rollback point at the last accepted package commit; and
8. update `graphify-out/` after meaningful code changes where the repository graph is in use.

The normal software-only verification baseline is:

```powershell
cmake -S app/runtime -B <build-dir> -DENABLE_NIDAQMX=ON
cmake --build <build-dir> --config Release
ctest --test-dir <build-dir> -C Release --output-on-failure
python -m pytest training/python/droplet_trainer -q
```

Package-specific commands below add targeted tests. A missing vendor SDK, hardware device, GPU provider, fixture, or managed Python environment must be reported as a limitation rather than bypassed with replacement production behavior.

## Ordered work packages

### Current implementation sequence

- **Completed groundwork:** P0-1 detector characterization and neutral-contract work is accepted and must not be revisited or expanded by the next slice.
- **Current slice:** visual-only navigation scaffold and Mock Single Image as bounded by [`current-slice.md`](current-slice.md).
- **First two rounds:** follow [`visual-scaffold-two-round-plan.md`](visual-scaffold-two-round-plan.md): one visual scaffold writer in Round 1, then isolated design/backbone workers after interface freeze in Round 2.
- **Next action:** prepare the bounded Round 1 Qt Design Studio work order with exact forms, mocks, tokens, worktree, and manual-review states.
- **Later:** connect the existing DCAM implementation to the v2 Camera boundary, display a real preview, capture one frame, save one real TIFF, and perform physical-camera qualification.

The approved UI delivery order is:

1. Shell and Mock Single Image
2. Full Capture and Hardware panel
3. Label
4. Sequence Viewer
5. Train
6. Library
7. Model Test
8. Live
9. Sequence Test
10. Results
11. Settings

The existing package identifiers below remain the longer-range reuse inventory. They do not authorize work out of this order or expand the current slice.

### Shared visual-component plan

Create a shared component only in the first slice that has an immediate visual consumer. The planned set is:

```text
StatusHeaderItem
CollapsibleSection
BottomHardwarePanel
DarkImageViewer
CameraActionBar
ErrorMessage
CropThumbnail
SelectedCropSection
ClassesFilterSection
ModelListRow
SelectedModelSection
HitBoundaryOverlay
RunListSection
TrainingPlot
```

The current slice may consume only `StatusHeaderItem`, `CollapsibleSection`, `BottomHardwarePanel`, `DarkImageViewer`, `CameraActionBar`, and `ErrorMessage`. It must not create unused later-slice components.

## P0-1 — Detector contract and consolidation

**Status:** Completed and accepted groundwork.

### Objective

Create one internal v2 droplet-detector contract, place both current implementations behind adapters, and add the evidence needed to select one qualified production implementation. `FastEventDetector` is provisional production because it currently serves desktop Live, Sequence Test, and Droplet Dataset Capture workflows. `EventDetector` remains temporarily for characterization/comparison only.

### Exact files and symbols involved

Current files and symbols:

- `app/runtime/event_detector.h`, `app/runtime/event_detector.cpp`: `EventDetector`, `EventDetectorConfig`, `EventResult`.
- `app/runtime/fast_event_detector.h`, `app/runtime/fast_event_detector.cpp`: `FastEventDetector`, `FastEventConfig`, `FastEventResult`, especially `FastEventResult::fired`.
- `app/runtime/cli_runner.cpp`: detector-mode construction and precise-mode caller hysteresis.
- `app/runtime/desktop_app/pipeline_runner.h`, `pipeline_runner.cpp`: `PipelineRunner`, `PipelineRunner::processFrame`, current fast-detector ownership.
- `app/runtime/desktop_app/dataset_capture_session.h`, `.cpp` and Live/Sequence consumers that receive current detector results.
- `app/runtime/tests/CMakeLists.txt` and detector/direction test targets.

Proposed files and symbols:

- `app/runtime/detection/droplet_detector.h`: `IDropletDetector`, `DetectorFrameContext`, `DropletObservation`, `DetectorReadiness`, `DetectorEventTransition`, `DropletDetectionFrame`.
- `app/runtime/detection/fast_event_detector_adapter.h`, `.cpp`: `FastEventDetectorAdapter`.
- `app/runtime/detection/event_detector_adapter.h`, `.cpp`: `EventDetectorAdapter`, comparison-only hysteresis/state mapping.
- `app/runtime/detection/droplet_detector_factory.h`, `.cpp`: `DropletDetectorFactory::createProduction()` returning the fast adapter with no user/workspace selector.
- `app/runtime/tests/detector_replay_test.cpp`: `DetectorReplayTest`.
- `app/runtime/tests/detector_crop_parity_test.cpp`: `DetectorCropParityTest`.
- `app/runtime/tests/detector_timing_benchmark.cpp`: `DetectorTimingBenchmark`.
- `app/runtime/tests/fixtures/detector/`: versioned raw frames, backgrounds, frame metadata, expected observations, event transitions, and crops.

`DropletDetectionFrame` must decompose the meaning currently compressed into `FastEventResult::fired`. At minimum it must separately represent:

- whether foreground/droplet observations exist in the current frame;
- the observations' regions, centroids, masks or mask references, and source-frame coordinates;
- detector readiness/background state;
- event transition such as none, entered, re-entered, or cleared; and
- stable detector event identity if consumers require multi-frame association.

The contract must not contain physical DAQ behavior, Hit/Waste Decision policy, model inference, workspace state, outlet selection, or a user-editable detector configuration.

### Reusable behavior that must be preserved

- **Preserve behind adapter:** `FastEventDetector` downscaling, thresholding, blur, connected components, rolling background, reset hysteresis, gap re-entry, centroid-shift re-entry, mask/box/centroid output, and current real-time performance.
- **Preserve behind adapter:** `EventDetector` normalized difference, explicit background modes, Otsu/percentile thresholding, local/self-background behavior, morphology, contour filtering, mask/box/centroid output, and precise-mode comparison behavior.
- Preserve source-frame coordinate and crop association semantics used by current desktop workflows.

### Obsolete product policy or structural coupling to remove

- Remove detector selection from any workspace/user-facing configuration path.
- Remove detector-owned or detector-result names that imply a physical trigger or Hit/Waste Decision.
- Prevent duplicated user-editable detector parameters.
- Remove direct knowledge of `FastEventDetector` from desktop workspace consumers; they depend on `IDropletDetector` or a higher application service.

### Proposed target boundary

`IDropletDetector::processFrame(const DetectorFrameContext&) -> DropletDetectionFrame` is an internal technical boundary. Qualified detector parameters are internal configuration/provenance, not product settings. The application pipeline consumes observations and event transitions; separate services perform crop creation, inference, Decision, observed-route recording, and DAQ command resolution.

### Characterization tests required first

Create a representative replay corpus containing:

- empty-channel and background-build sequences;
- single droplets, multiple droplets, both travel directions, edge/border events, re-entry, closely spaced events, and reset gaps;
- illumination drift, bubbles, debris, noise, focus variation, and representative false-positive cases;
- supported 8-bit and 16-bit inputs with source bit-depth metadata;
- frames from Live, Sequence Test, and Droplet Dataset Capture operating conditions; and
- hardware-captured sequences at minimum, nominal, and maximum qualified frame/event rates.

For every sequence compare:

- observation presence, count, masks, bounding boxes, centroids, and source coordinates;
- event entry/re-entry/clear transitions and event identity;
- produced crop coordinates and final crop pixels against golden fixtures;
- false positives, false negatives, missed/duplicate events, and direction inputs; and
- median, p95, p99, and maximum processing time, allocations, and sustained CPU load.

Define acceptable differences before implementation review. A second hardware-trigger qualification must replay the accepted detector result through the later Decision/DAQ path and measure camera-frame-to-physical-output latency and jitter. That hardware test is a prerequisite to removing `EventDetector`, even though physical DAQ behavior is not part of the detector contract.

### Implementation changes

1. Add neutral contract types and explicit event-transition semantics.
2. Add adapters that preserve each detector's current behavior.
3. Route `PipelineRunner` through `DropletDetectorFactory::createProduction()` with fast as the fixed provisional default.
4. Keep the CLI comparison path available only as an internal/test characterization mechanism; do not expose it through v2 UI or workspace state.
5. Add replay, crop-parity, and timing targets and record comparison evidence.
6. Do not delete, merge, or behaviorally rewrite either detector in the first PR.
7. In a separate second PR, remove `EventDetector` from production linkage only after evidence review demonstrates all required behavior is retained. Keep it test-only if comparison history remains useful, or delete it under the approved retirement decision.

### Build and test commands

```powershell
cmake -S app/runtime -B <build-dir> -DENABLE_NIDAQMX=ON
cmake --build <build-dir> --config Release --target opendss_runtime desktop_app detector_replay_test detector_crop_parity_test detector_timing_benchmark
ctest --test-dir <build-dir> -C Release --output-on-failure -R "detector|event_direction"
<build-dir>\Release\detector_timing_benchmark.exe --corpus app/runtime/tests/fixtures/detector
```

### Hardware qualification requirements

**Hardware qualification required** before retirement: supported DCAM camera, representative bit depths/frame rates, real channel conditions, and end-to-end detector-to-NI-DAQmx output measurements. Software replay alone may approve the contract/adapters but not `EventDetector` removal from production linkage.

### Acceptance criteria

- One internal detector contract exists and contains no DAQ, model, Decision, routing, workspace, or user-setting policy.
- Fast is the fixed provisional production implementation.
- Event remains comparison-only and is not selectable by a workspace or user setting.
- `FastEventResult::fired` is not copied into the public v2 contract; its meanings are explicitly decomposed.
- Both adapters pass the agreed replay/crop comparison, and timing results are published with the PR.
- No detector parameters are exposed in v2 UI.
- The first PR deletes neither detector.

### Rollback point

Revert the adapter/contract PR to restore direct `FastEventDetector` ownership. The existing detector implementations and configurations remain intact, so rollback does not require reconstructing algorithms.

### Dependencies on earlier work packages

None. This is the recommended first implementation package.

## P0-2 — Camera-worker blocking and persistence/DAQ separation

### Objective

Ensure that no TIFF, PNG, CSV, or JSON write and no blocking DAQ operation executes on the camera acquisition/processing thread. Introduce bounded handoff queues with explicit overflow, flush, fault, and shutdown behavior while preserving DCAM and NI-DAQmx integration.

### Exact files and symbols involved

Current files and symbols:

- `app/runtime/desktop_app/camera_worker.h`, `.cpp`: `CameraWorker`, record hook and frame loop.
- `app/runtime/desktop_app/camera_workspace_controller.h`, `.cpp`: acquisition lifecycle and pipeline hookup.
- `app/runtime/desktop_app/pipeline_runner.h`, `.cpp`: `PipelineRunner::processFrame`, detector/inference/DAQ call chain.
- `app/runtime/daq_trigger.h`, `.cpp`: `DaqTrigger::fire` and wait/delay behavior.
- `app/runtime/desktop_app/live_data_collection_writer.h`, `.cpp`: `LiveDataCollectionWriter::writeFrame`.
- `app/runtime/desktop_app/live_log_writer.h`, `.cpp`, `sequence_summary_writer.h`, `.cpp`, `json_persistence.h`, `.cpp`.
- `app/runtime/desktop_app/background_task_registry.h`, `.cpp`.

Proposed files and symbols:

- `app/runtime/concurrency/bounded_work_queue.h`: `BoundedWorkQueue<T>`, `OverflowPolicy`, `QueueCloseMode`.
- `app/runtime/desktop_app/frame_processing_dispatcher.h`, `.cpp`: `FrameProcessingDispatcher`, `FrameEnvelope`.
- `app/runtime/desktop_app/persistence_dispatcher.h`, `.cpp`: `PersistenceDispatcher`, `PersistenceCommand`, `FlushReason`.
- `app/runtime/desktop_app/daq_command_dispatcher.h`, `.cpp`: `DaqCommandDispatcher`, `PendingDaqCommand`.
- `app/runtime/tests/bounded_work_queue_test.cpp`, `processing_thread_affinity_test.cpp`, `persistence_flush_test.cpp`.

### Reusable behavior that must be preserved

- **Preserve unchanged:** DCAM capture/device/buffer mechanics and `CameraWorker`'s qualified acquisition loop.
- **Preserve unchanged:** `DaqTrigger` NI-DAQmx waveform, validation, rate fallback, final-zero, and cleanup behavior.
- **Extract reusable mechanics:** existing image/log/summary writers, `QSaveFile`/retry behavior, and proven background-task joining.
- Preserve event/frame ordering, timestamps, file names, current crop/image contents, and error diagnostics unless a separately versioned schema change is approved.

### Obsolete product policy or structural coupling to remove

- Remove synchronous writer calls and `DaqTrigger::fire` from the acquisition-thread call stack.
- Remove unbounded per-run image/write accumulation.
- Remove implicit “keep accepting work” behavior during Pause, Stop, fault, or exit.
- Do not add user controls for internal queue size, detector timing, pulse timing, or overflow policy.

### Proposed target boundary

The acquisition thread produces immutable/ref-counted `FrameEnvelope` objects and performs only bounded nonblocking handoff. Processing produces neutral persistence commands and already-resolved DAQ commands. Dedicated persistence and DAQ execution contexts own blocking calls. Queue capacity and overflow behavior are fixed qualified configuration with observable counters and typed faults.

### Characterization tests required first

- Thread-affinity probes proving current and target call locations.
- Golden ordering and file-content fixtures for TIFF, PNG, CSV, JSON, live log, and sequence summary.
- Slow-disk and full-disk fault injection.
- Artificially slow DAQ execution, burst events, maximum camera rate, and simultaneous preview/processing/writing.
- Queue saturation for each approved policy: reject-newest, reject-oldest, stop-operation, or fault; choose one policy per queue and document why.
- Pause, Stop, camera fault, DAQ fault, writer fault, and application-exit tests with in-flight commands.
- Long-run memory and queue-depth measurement.

### Implementation changes

1. Add bounded queues and typed overflow/fault counters.
2. Change the camera hook to enqueue work without performing file or DAQ I/O.
3. Move current writer invocations into `PersistenceDispatcher` without changing their qualified file mechanics.
4. Move `DaqTrigger::fire` invocation into `DaqCommandDispatcher` without rewriting NI-DAQmx behavior.
5. Define flush semantics: Pause drains accepted work and then pauses; Stop drains or records an explicit incomplete result; fault stops acceptance and attempts bounded safe flush; exit performs bounded shutdown with an incomplete-operation record if needed.
6. Surface queue/fault state to the future operation owner, not widget labels.

### Build and test commands

```powershell
cmake --build <build-dir> --config Release --target desktop_app bounded_work_queue_test processing_thread_affinity_test persistence_flush_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "bounded_work_queue|thread_affinity|persistence_flush"
ctest --test-dir <build-dir> -C Release --output-on-failure
```

### Hardware qualification requirements

**Hardware qualification required** for sustained DCAM acquisition with real NI-DAQmx output and representative disk throughput. Measure dropped frames, queue peaks, output latency/jitter, shutdown behavior, and file completeness at nominal and worst qualified rates.

### Acceptance criteria

- No TIFF, PNG, CSV, or JSON write appears on the acquisition/processing thread in thread-affinity tests.
- No blocking NI-DAQmx call appears on the acquisition thread.
- Every queue is bounded and has documented, tested overflow behavior.
- Pause, Stop, fault, and exit flush semantics are deterministic and tested.
- Existing DCAM and NI-DAQmx integration mechanics are preserved.
- File/event ordering and qualified output contents match golden fixtures.

### Rollback point

Revert to the P0-1 accepted pipeline. Keep dispatchers behind a single integration commit so the old synchronous call path can be restored without changing writer or vendor implementations.

### Dependencies on earlier work packages

Depends on P0-1 result semantics so processing and event transitions can be queued without carrying the ambiguous `fired` field.

## P0-3 — Authoritative application state and operation ownership

### Objective

Establish exactly one authoritative owner for Camera state/settings, DAQ state/settings, Active Model/package state, Dataset state, Sequence state, the current long-running operation/resource locks, Training execution, Run persistence, and application preferences. Widgets, labels, and controls become projections and command sources only.

### Exact files and symbols involved

Current files and symbols:

- `app/runtime/desktop_app/app_state.h`: `AppState` and its camera, DAQ, model, target, arming, and operation-related fields.
- `app/runtime/desktop_app/app_context.h`, `.cpp`: `AppContext`.
- `app/runtime/desktop_app/app_types.h`.
- `app/runtime/desktop_app/main_window.h`, `.cpp`: current composition and label-derived state.
- `camera_workspace_controller.*`, `dataset_workspace_controller.*`, `settings_workspace_controller.*`, `reports_workspace_controller.*`, `validator_workspace_controller.*`, and `workspace_camera.*`, `workspace_dataset.*`, `workspace_model.*`, `workspace_settings.*`, `workspace_reports.*`, `workspace_validator.*`; Live and Sequence orchestration currently remains in `main_window.*`.
- `pipeline_runner.*`, `background_task_registry.*`, persistence/training launch paths.

Proposed files and symbols:

- `app/runtime/v2/state/domain_state.h`: typed immutable snapshots `CameraState`, `DaqState`, `ModelPackageState`, `DatasetState`, `SequenceState`, `TrainingState`, `RunState`, `PreferencesState`.
- `app/runtime/v2/state/application_state_store.h`, `.cpp`: `ApplicationStateStore`, snapshot publication only.
- `app/runtime/v2/operation/operation_coordinator.h`, `.cpp`: `OperationCoordinator`, `OperationKind`, `ResourceLock`, `OperationLease`, `OperationFault`.
- `app/runtime/v2/application_services.h`, `.cpp`: `ApplicationServices`, the composition root for one owner per domain.
- Domain owner interfaces: `ICameraService`, `IDaqService`, `IActiveModelService`, `IDatasetService`, `ISequenceService`, `ITrainingService`, `IRunRepository`, `IPreferencesService`.
- `app/runtime/tests/application_state_store_test.cpp`, `operation_coordinator_test.cpp`, `state_projection_test.cpp`.

### Reusable behavior that must be preserved

- **Extract reusable mechanics:** existing resource lifecycle, operation progress, cancellation, fault, persistence, settings serialization, and controller signals where their semantics match v2.
- Preserve camera/DAQ fault detail, active-model persistence, sequence/run identity, and recoverable-operation evidence.
- Preserve technical preferences that the approved product model still permits; do not automatically preserve every existing setting.

### Obsolete product policy or structural coupling to remove

- Replace duplicated authoritative-looking fields in `AppState`, controllers, widgets, and pipeline configuration.
- Eliminate state inferred from label strings, button text, selected tabs, controls, or widget visibility.
- Retire software-arming, target/sort-nontarget, promotion/validation, old-workspace, and manual compute/environment state where superseded by the v2 baseline.
- Prevent widget-local Camera, DAQ, Active Model, Dataset, Sequence, Run, Training, or operation state from becoming authoritative.

### Proposed target boundary

Each domain service owns its mutable state and publishes typed immutable snapshots. `ApplicationStateStore` aggregates snapshots for presentation but does not create a second mutable owner. `OperationCoordinator` alone grants resource locks and owns the current long-running operation. UI sends typed commands and observes snapshots.

### Characterization tests required first

- State-transition captures for startup, camera open/stream/stop/fault/recovery, DAQ ready/fault/disabled, model activation, dataset open/capture/close, sequence start/pause/stop, training start/cancel/fail/save, and run flush/recovery.
- Resource-conflict matrix for Camera, DAQ, Model, Dataset, Sequence, Training, and Run operations.
- Tests that deliberately desynchronize widget text/control values from service state and prove behavior follows the service.
- Persistence/restart fixtures for active model and application preferences.
- Queued/stale signal ordering and owner-destruction tests.

### Implementation changes

1. Add typed domain snapshots and operation/resource contracts.
2. Compose one service instance per domain in `ApplicationServices`.
3. Route state changes through domain commands and `OperationCoordinator` leases.
4. Adapt legacy controllers incrementally to observe snapshots while preserving current UI until P1-7.
5. Remove label/control-derived decisions after each domain migration has parity tests.
6. Keep `AppState` as a temporary read-only compatibility projection if required, then retire it after all consumers migrate.

### Build and test commands

```powershell
cmake --build <build-dir> --config Release --target desktop_app application_state_store_test operation_coordinator_test state_projection_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "application_state|operation_coordinator|state_projection"
ctest --test-dir <build-dir> -C Release --output-on-failure
```

### Hardware qualification requirements

Hardware is not required for pure store/coordinator tests. **Hardware qualification required** before declaring Camera and DAQ state migrations complete, particularly fault/recovery, disconnect, shutdown, and lock-release transitions.

### Acceptance criteria

- Exactly one mutable owner exists for each named domain.
- Exactly one `OperationCoordinator` owns long-running operation/resource locks.
- Widget text, labels, controls, and visibility are not queried as state authorities.
- Conflicting operations fail through typed lock results with contextual recovery information.
- Restart/persistence and fault transitions pass fixtures.
- Temporary compatibility projections are read-only and have explicit retirement issues.

### Rollback point

Retain adapter boundaries between legacy controllers and the new owners. Revert one domain migration at a time to the P0-2 accepted implementation; do not maintain two writable owners as a fallback.

### Dependencies on earlier work packages

Depends on P0-1 neutral detector results and P0-2 explicit processing/persistence/DAQ execution boundaries.

## P1-1 — Inference mechanics separated from routing and product policy

### Objective

Preserve ONNX Runtime loading, preprocessing, provider handling, inference, and class output while separating them from target-class defaults, Hit/Waste Decision policy, route observation, and physical output.

### Exact files and symbols involved

Current files and symbols:

- `app/runtime/onnx_classifier.h`, `.cpp`: `OnnxClassifier`, preprocessing buffers, provider/session setup, `classify`.
- `app/runtime/metadata_loader.h`, `.cpp`: metadata parsing and `ResolveTargetClassId`.
- `app/runtime/desktop_app/pipeline_runner.h`, `.cpp`: `PipelineConfig`, `targetClassId`, `targetLabel`, `sortNonTarget`, `useCuda`, classification-to-DAQ logic.
- `app/runtime/desktop_app/model_registry_service.h`, `.cpp`.
- shared result/routing fields in `app_types.h` and live/sequence writers.

Proposed files and symbols:

- `app/runtime/inference/inference_engine.h`: `IInferenceEngine`, `InferenceInput`, `InferenceResult`, `ClassScore`.
- `app/runtime/inference/onnx_inference_adapter.h`, `.cpp`: `OnnxInferenceAdapter` wrapping `OnnxClassifier`.
- `app/runtime/model/model_package_metadata.h`, `.cpp`: neutral class/input/normalization metadata.
- `app/runtime/v2/decision/decision_service.h`, `.cpp`: `DecisionService`, `DecisionResult`.
- `app/runtime/v2/routing/observed_route.h`: `ObservedRoute`, including `Unresolved`.
- `app/runtime/tests/onnx_preprocessing_golden_test.cpp`, `inference_contract_test.cpp`, `decision_routing_separation_test.cpp`.

### Reusable behavior that must be preserved

- **Preserve behind adapter:** ONNX session/provider creation, preprocessing, normalization, tensor layout, inference, and output extraction.
- **Preserve behind adapter:** neutral metadata class order/display labels/input shape/normalization parsing.
- Preserve automatic CPU/GPU provider qualification/fallback mechanics and diagnostics, subject to packaged-environment tests.

### Obsolete product policy or structural coupling to remove

- Replace `ResolveTargetClassId` binary Class 1/`Single` defaults as low-level inference behavior.
- Move `targetClassId`, `targetLabel`, `sortNonTarget`, confidence-routing, Hit/Waste, outlet, and DAQ decisions out of `OnnxClassifier`, metadata parsing, and `PipelineRunner` inference boundary.
- Do not expose manual CPU/GPU selection in v2 UI.
- Do not assume all models are binary or Class 1 is the target.

### Proposed target boundary

`IInferenceEngine` returns only model identity, predicted class, class scores, and inference diagnostics. `DecisionService` applies v2 application policy using the active package and qualified thresholds. Observed Route is recorded later from physical evidence. DAQ receives a resolved physical command only after Decision/routing mapping.

### Characterization tests required first

- Golden preprocessing tensors and model outputs for supported images.
- CPU/GPU output parity and automatic provider fallback/diagnostic tests.
- Binary and multi-class packages with nonnumeric class IDs and reordered classes.
- Output-count versus metadata-count mismatch, corrupt metadata, missing labels, and unsupported input shape.
- Confidence boundary and no-decision cases proving inference output is unchanged while Decision varies independently.
- Serialized and concurrent access tests before changing the current mutable-buffer/serialized-use assumption.

### Implementation changes

1. Add neutral inference and class-score contracts.
2. Wrap `OnnxClassifier` without rewriting its mechanics.
3. Split neutral package metadata from routing/promotion fields.
4. Move Decision policy and Observed Route types above inference.
5. Change `PipelineRunner` or its successor to orchestrate separate detector, crop, inference, Decision, route-record, and DAQ-command steps.
6. Retire unused compatibility fields only after construction/API searches and tests.

### Build and test commands

```powershell
cmake --build <build-dir> --config Release --target opendss_runtime desktop_app onnx_preprocessing_golden_test inference_contract_test decision_routing_separation_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "metadata|onnx|inference|decision_routing"
```

### Hardware qualification requirements

No camera/DAQ hardware is needed for contract tests. Windows GPU/ONNX Runtime qualification is required for provider selection, fallback, packaged DLL loading, performance, and CPU/GPU output parity.

### Acceptance criteria

- Inference supports the metadata-declared class count and never supplies target/Hit/Waste/DAQ policy.
- Predicted Class, Decision, and Observed Route are separate typed results.
- Existing qualified preprocessing and inference outputs match golden fixtures.
- Provider selection is automatic and observable, not a user preference.
- Low-level inference APIs contain no workspace state.

### Rollback point

Keep `OnnxClassifier` intact behind the adapter. Revert orchestration to the P0-3 accepted service graph without reconstructing ONNX mechanics.

### Dependencies on earlier work packages

Depends on P0-3 authoritative Active Model and operation state. Uses P0-1 detector observations and P0-2 processing dispatch.

## P1-2 — Droplet Dataset Capture mechanics separated from legacy labeling policy

### Objective

Preserve qualified detection/crop/session/hash/persistence mechanics while replacing HitOnly/WasteOnly/Mixed capture modes, automatic model-linked labels, and fixed Hit/Waste/Exclude schemas with the v2 Dataset contract.

### Exact files and symbols involved

Current files and symbols:

- `app/runtime/dataset_capture_session.h`, `.cpp`: `DatasetCaptureSession`, capture-mode enum, labels, counters, manifest persistence.
- `app/runtime/desktop_app/collection_postprocessor.h`, `.cpp`: crop creation and default schema.
- `app/runtime/desktop_app/live_data_collection_writer.*`, `json_persistence.*`, `dataset_workspace_controller.*`, and dataset fields in `app_types.h`.
- Crop creation in `pipeline_runner.*` and `cli_runner.cpp`.
- `app/runtime/desktop_app/dataset_workspace_controller.h`, `.cpp` as a legacy consumer; existing user modifications to these files must be resolved separately before an implementation PR touches them.

Proposed files and symbols:

- `app/runtime/v2/dataset/dataset_capture_service.h`, `.cpp`: `DatasetCaptureService`, `DatasetCaptureState`.
- `app/runtime/v2/dataset/dataset_manifest_v2.h`, `.cpp`: `DatasetManifestV2`, `CapturedCrop`, `CaptureProvenance`.
- `app/runtime/crops/crop_service.h`, `.cpp`: `CropService`, `CropRequest`, `CropArtifact`.
- `app/runtime/tests/crop_golden_test.cpp`, `dataset_manifest_v2_test.cpp`, `dataset_capture_recovery_test.cpp`.

### Reusable behavior that must be preserved

- **Extract reusable mechanics:** session directories, crop copy/write, hashes, counters, temporary-file recovery, and provenance.
- **Consolidate after characterization:** square crop, resize, coordinate mapping, and image encoding currently repeated across pipeline, CLI, and postprocessor.
- Preserve source-frame/event association and interrupted-session evidence.

### Obsolete product policy or structural coupling to remove

- Replace HitOnly, WasteOnly, and Mixed modes.
- Remove model inference and automatic model-linked labeling from Droplet Dataset Capture.
- Replace fixed Hit/Waste/Exclude and Target/Non-target schemas with a versioned neutral v2 dataset schema.
- Remove workspace-owned counters/state after P0-3 migration.

### Proposed target boundary

`DatasetCaptureService` consumes detector observations plus source-frame/crop provenance and writes unlabeled capture artifacts through the persistence dispatcher. `CropService` owns qualified crop geometry/encoding. Dataset labels, if introduced by an approved later workflow, are separate records and not generated by capture.

### Characterization tests required first

- Golden source-frame-to-crop coordinate and pixel comparisons for all current crop paths.
- Existing manifest/hash round-trip, interruption, duplicate/retry, and recovery fixtures.
- Unlabeled multi-class-neutral v2 manifests.
- Compatibility fixtures for opening old datasets without rewriting them silently.
- Long-running bounded-write/backpressure tests from P0-2.

### Implementation changes

1. Add versioned neutral dataset/crop/provenance contracts.
2. Centralize qualified crop generation behind `CropService` after parity evidence.
3. Adapt current session mechanics to v2 manifests and background persistence.
4. Remove automatic inference/Hit/Waste label assignment from the v2 capture path.
5. Keep old-schema reading isolated as compatibility code if required; do not make it v2 write policy.

### Build and test commands

```powershell
cmake --build <build-dir> --config Release --target desktop_app crop_golden_test dataset_manifest_v2_test dataset_capture_recovery_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "crop|dataset_manifest|dataset_capture"
```

### Hardware qualification requirements

Recorded frames are sufficient for crop/schema tests. Camera qualification is required for sustained Droplet Dataset Capture, correct frame/event/crop association, queue bounds, pause/stop/fault flush, and real bit-depth fidelity.

### Acceptance criteria

- V2 Droplet Dataset Capture does not invoke inference or assign automatic model labels.
- No HitOnly/WasteOnly/Mixed product mode exists in the v2 path.
- Crop bytes/geometry and persistence mechanics pass golden/recovery fixtures.
- V2 manifests are neutral, versioned, and carry provenance.
- Old datasets are read only through explicit compatibility behavior.

### Rollback point

Keep the legacy capture reader/path available outside v2 until migration acceptance. Revert the v2 service adapter to the P1-1 accepted state without rewriting existing datasets.

### Dependencies on earlier work packages

Depends on P0-1 observations, P0-2 persistence dispatch, and P0-3 Dataset/operation ownership. It should follow P1-1 so absence of inference in capture is enforced by the service graph.

## P1-3 — Training orchestration separated from legacy desktop policy

### Objective

Preserve the Python training/export implementation while moving environment verification, launch, progress, cancellation, profiles, save, and failure recovery behind one TrainingService. Replace manual Python path, manual CPU/GPU choice, editable hyperparameters, binary promotion policy, and manual post-save activation in the v2 path.

### Exact files and symbols involved

Current files and symbols:

- `training/python/droplet_trainer/cli.py`, `train.py`, `metadata.py`, `env.py`, export/model modules, tests, and publication experiments.
- Desktop trainer launch/JSONL/progress/environment/save code in `dataset_workspace_controller.*`, `main_window.cpp`, model/settings controls.
- `model_registry_service.*` activation/package calls.

Proposed files and symbols:

- `app/runtime/v2/training/training_service.h`, `.cpp`: `TrainingService`, `TrainingRequest`, `TrainingProfile`, `TrainingJobState`.
- `app/runtime/v2/training/trainer_process_adapter.h`, `.cpp`: `TrainerProcessAdapter`, managed-environment verification and JSONL protocol.
- `app/runtime/v2/training/training_profiles.h`, `.cpp`: fixed `Faster` and `MoreAccurate` qualified profiles.
- `app/runtime/tests/training_service_protocol_test.cpp`, `training_profile_test.cpp`, `training_recovery_test.cpp`.
- Python fixtures/tests for profile serialization, cancellation, export parity, and metadata/package handoff.

### Reusable behavior that must be preserved

- **Preserve unchanged:** Python dataset loading, training loop, deterministic seeding, progress events, cancellation cooperation, ONNX export, and technical artifact-integrity checks such as nonfinite/constant/missing-class output detection.
- **Preserve behind adapter:** QProcess/JSONL protocol and bundled-environment verification where characterized.
- Preserve model/preprocessing metadata required for reproducible inference.

### Obsolete product policy or structural coupling to remove

- Replace manual Python executable/virtual-environment selection.
- Replace manual CPU/GPU selection with automatic qualified provider policy.
- Replace editable hyperparameter JSON with fixed Faster/More Accurate profiles.
- Remove binary target/promotion/validation product policy while retaining technical artifact-integrity validation.
- Replace manual post-save activation with the approved v2 save-and-activate lifecycle through P1-6.

### Proposed target boundary

UI issues `TrainingService::start(TrainingRequest)` using an approved profile and observes `TrainingJobState`. `TrainerProcessAdapter` owns the managed environment and process protocol. Python receives a complete internal config; UI never owns environment paths, device choice, or hyperparameters.

### Characterization tests required first

- Known-dataset deterministic runs and golden profile serialization.
- Faster/More Accurate resource/time/quality evidence on qualified systems.
- CPU/GPU automatic selection and exported-model output parity.
- Cancel, process crash, malformed JSONL, missing environment, disk failure, and application restart.
- Technical collapse/corrupt-artifact cases separated from obsolete promotion decisions.
- Resolve the failing publication-experiment initialization fixture before relying on that suite; do not weaken validation merely to make the test pass.

### Implementation changes

1. Add TrainingService and process adapter around the existing trainer.
2. Define qualified profiles and remove v2 UI access to raw config.
3. Move process/environment/progress/cancel state into the authoritative Training owner.
4. Separate artifact-integrity results from product promotion/activation policy.
5. Hand a successfully saved package to P1-6 for atomic activation/rollback.

### Build and test commands

```powershell
cmake --build <build-dir> --config Release --target desktop_app training_service_protocol_test training_profile_test training_recovery_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "trainer|training"
python -m pytest training/python/droplet_trainer -q
```

### Hardware qualification requirements

No camera/DAQ hardware is required. Windows managed-environment and GPU qualification is required for environment discovery, automatic device selection, cancellation, packaging, and CPU/GPU parity.

### Acceptance criteria

- V2 UI exposes only Faster and More Accurate choices.
- No v2 control accepts Python paths, virtual environments, CPU/GPU choice, or hyperparameter JSON.
- Python training/export mechanics and technical integrity failures retain characterized behavior.
- Training execution has one authoritative owner and typed progress/failure/cancel state.
- Successful package save hands off to atomic activation; failures preserve the prior active model.

### Rollback point

Keep the legacy trainer launch path outside v2 until the service passes protocol/environment tests. Revert the v2 adapter without changing Python training implementation or previously saved packages.

### Dependencies on earlier work packages

Depends on P0-3 Training/operation ownership, P1-2 v2 Dataset contracts, and the P1-6 package interface agreed in advance. Final auto-activation acceptance depends on P1-6.

## P1-4 — Camera/DCAM ownership consolidation

### Objective

Provide one Camera service/device contract and one authoritative Camera state owner while preserving the current DCAM SDK integration. Consolidate duplicated CLI/desktop SDK ownership only where hardware characterization proves equivalence.

### Exact files and symbols involved

Current files and symbols:

- `app/runtime/dcam_camera.h`, `.cpp`: `DcamCamera`.
- `app/runtime/desktop_app/dcam_controller.h`, `.cpp`: `DcamController`, `readProps` candidate.
- `app/runtime/desktop_app/camera_worker.h`, `.cpp`: `CameraWorker`.
- `camera_workspace_controller.*`, `workspace_camera.*`, frame/LUT/image conversion code.
- `app/runtime/desktop_app/frame_grabber.h`, `.cpp`: unreferenced retirement candidate.

Proposed files and symbols:

- `app/runtime/camera/camera_device.h`: `ICameraDevice`, `CameraDeviceConfig`, `CameraFrame`, `CameraProperties`.
- `app/runtime/camera/dcam_device_adapter.h`, `.cpp`: `DcamDeviceAdapter` around qualified DCAM mechanics.
- `app/runtime/v2/camera/camera_service.h`, `.cpp`: `CameraService` implementing `ICameraService`.
- `app/runtime/camera/frame_conversion.h`, `.cpp`: characterized conversion boundary.
- `app/runtime/tests/camera_adapter_contract_test.cpp`, `frame_conversion_golden_test.cpp` plus HIL runner.

### Reusable behavior that must be preserved

- **Preserve behind adapter:** DCAM initialization/uninitialization, device/wait handles, properties, buffers, capture, timeouts, release, and error detail.
- Preserve qualified acquisition-thread lifecycle and source bit depth.
- Preserve required CLI access through the same camera contract or a thin non-Qt adapter.

### Obsolete product policy or structural coupling to remove

- Remove duplicate authoritative SDK/device ownership where equivalence is proven.
- Remove widget/controller dependencies from reusable acquisition code.
- Remove state inferred from UI labels/controls.
- Retire `FrameGrabber` only after downstream/generated/plugin checks and clean build/start evidence.

### Proposed target boundary

`CameraService` alone owns Camera state/settings and delegates device mechanics to `ICameraDevice`. `DcamDeviceAdapter` is the only production DCAM SDK owner per process. Frame conversion is explicit at the consumer boundary, not repeated inside UI controllers.

### Characterization tests required first

- CLI and desktop wrapper comparison for device selection, properties, buffer count, timeouts, frame contents, errors, and shutdown.
- Golden 8/16-bit conversion and frame-lifetime tests.
- Repeated open/start/stop/reopen, apply settings, unplug/fault/recover, and exit-during-capture.
- Sustained frame rate and memory/resource leak measurement.

### Implementation changes

1. Add camera device/service contracts around current mechanics.
2. Migrate desktop worker to `CameraService` without changing P0-2 thread/queue rules.
3. Migrate CLI to the shared device contract after parity evidence.
4. Consolidate duplicated SDK lifetime code only after HIL review.
5. Retire `FrameGrabber` in a separate cleanup commit after its no-consumer evidence is revalidated.

### Build and test commands

```powershell
cmake --build <build-dir> --config Release --target opendss_runtime desktop_app camera_adapter_contract_test frame_conversion_golden_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "camera_adapter|frame_conversion"
```

### Hardware qualification requirements

**Hardware qualification required** on each supported Hamamatsu/DCAM configuration for lifecycle, properties, bit depth, frame rate, disconnect/recovery, buffer ownership, resource cleanup, and application shutdown.

### Acceptance criteria

- One production DCAM device owner exists per process.
- CLI and desktop consume a common Camera contract without forcing Qt UI types into the core.
- Qualified frame/device behavior matches HIL evidence.
- Camera state/settings have one authoritative owner.
- No QML/UI code calls DCAM.
- `FrameGrabber` is not retired until its separate evidence gate passes.

### Rollback point

Retain old wrappers unchanged until each consumer migration is accepted. Revert consumer adapters independently; never partially merge SDK lifetime code without a working prior owner.

### Dependencies on earlier work packages

Depends on P0-2 acquisition-thread separation and P0-3 Camera ownership. It may run after P1-1/P1-2 if those packages need stable frame contracts.

## P1-5 — DAQ ownership consolidation

### Objective

Provide one DAQ service/adapter contract and one authoritative DAQ state/settings owner while preserving NI-DAQmx mechanics. Separate physical output execution from predicted class, Decision, observed route, workspace, and UI state.

### Exact files and symbols involved

Current files and symbols:

- `app/runtime/daq_trigger.h`, `.cpp`: `DaqTrigger`, configuration, discovery, waveform, `fire`.
- `app/runtime/desktop_app/pipeline_runner.h`, `.cpp`.
- Settings/probe/manual test code and Live/Sequence call paths.
- DAQ fields in `app_state.h`, `app_types.h`, `main_window.cpp` and controllers.
- `daq_command_dispatcher.*` introduced by P0-2.

Proposed files and symbols:

- `app/runtime/daq/daq_output.h`: `IDaqOutput`, `DaqOutputConfig`, `DaqCommand`, `DaqExecutionResult`.
- `app/runtime/daq/nidaqmx_output_adapter.h`, `.cpp`: `NidaqmxOutputAdapter` around `DaqTrigger`.
- `app/runtime/v2/daq/daq_service.h`, `.cpp`: `DaqService` implementing `IDaqService`.
- `app/runtime/tests/daq_adapter_contract_test.cpp`, `daq_command_order_test.cpp` plus HIL runner.

### Reusable behavior that must be preserved

- **Preserve behind adapter:** NI-DAQmx discovery, validation, task creation/cleanup, waveform generation, sample-rate fallback, voltage/channel handling, delays, wait behavior, final zero, and diagnostics.
- Preserve P0-2 bounded execution/ordering/flush behavior.

### Obsolete product policy or structural coupling to remove

- Replace software-arming concepts.
- Remove target class, sort-nontarget, Hit/Waste, workspace, and widget state from DAQ APIs.
- Separate Hit Class, Hit boundary calibration, and DAQ Output Channel mapping in the application layer before constructing `DaqCommand`.
- Remove UI text as a DAQ readiness/fault authority.

### Proposed target boundary

`DaqService` owns readiness/settings/fault state. `IDaqOutput` accepts an already-resolved channel plus qualified waveform profile/correlation identity. It never receives model class, Hit/Waste label, workspace state, or user detector settings.

### Characterization tests required first

- Configuration validation and compile-branch contract tests without hardware.
- Command ordering, queue overflow, cancellation, pause/stop/fault/exit behavior from P0-2.
- Truth-table fixtures mapping approved Decision/outlet direction/channel policy into neutral `DaqCommand` outside the adapter.
- Real-device waveform, voltage, duration, sample-rate fallback, delay, final-zero, fault, disconnect, reset, latency, and jitter measurements.

### Implementation changes

1. Add neutral DAQ output/result contracts.
2. Wrap `DaqTrigger` without rewriting NI-DAQmx calls.
3. Make `DaqService` the sole owner and connect it to P0-2 dispatch.
4. Move routing/channel resolution to the application Decision/routing layer.
5. Remove legacy arming/readiness duplicates after state transition/HIL acceptance.

### Build and test commands

```powershell
cmake --build <build-dir> --config Release --target opendss_runtime desktop_app daq_adapter_contract_test daq_command_order_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "daq_adapter|daq_command"
```

### Hardware qualification requirements

**Hardware qualification required** on supported NI hardware with oscilloscope or logic analyzer evidence for waveform, channel, polarity/direction, latency/jitter, rate fallback, burst behavior, fault recovery, and final safe output state.

### Acceptance criteria

- One DAQ owner exists and no UI/QML/vendor-independent service constructs NI tasks directly.
- No blocking DAQ call executes on the acquisition thread.
- DAQ contracts contain no class/Hit/Waste/workspace/software-arming policy.
- Channel/direction mapping and physical output pass software truth tables and HIL evidence.
- Existing NI-DAQmx mechanics remain characterized and rollback-capable.

### Rollback point

Keep `DaqTrigger` unchanged behind the adapter and preserve the P0-2 dispatcher seam. Revert service ownership to the P0-3/P0-2 accepted boundary without changing waveform code.

### Dependencies on earlier work packages

Depends on P0-2 dispatch, P0-3 authoritative DAQ/operation state, and P1-1 Decision separation.

## P1-6 — Model registry and v2 package policy

### Objective

Preserve model package discovery, copy, hash, integrity, metadata, and rollback mechanics while replacing binary target/promotion/manual-activation policy with the approved v2 package lifecycle, including automatic activation after successful save.

### Exact files and symbols involved

Current files and symbols:

- `app/runtime/desktop_app/model_registry_service.h`, `.cpp`: `inspectModelPackage`, `saveTrainedModelArtifacts`, `activateModelRegistryEntry`, `evaluateActiveModelReadiness`, and related package/registry helpers.
- `app/runtime/metadata_loader.h`, `.cpp`.
- `app/runtime/desktop_app/json_persistence.*`, `app_paths.*`, `app_options.*`, `workspace_model.*`, and model orchestration in `main_window.*`.
- Trainer metadata/package generation in `training/python/droplet_trainer/metadata.py` and export/save modules.
- Active-model fields in `app_state.h` and current save/use-trained-model flow.

Proposed files and symbols:

- `app/runtime/v2/model/model_package_repository.h`, `.cpp`: `ModelPackageRepository`.
- `app/runtime/v2/model/model_package_validator.h`, `.cpp`: `ModelPackageValidator`, technical integrity only.
- `app/runtime/v2/model/active_model_service.h`, `.cpp`: `ActiveModelService`, `activateAtomically`, `rollbackActivation`.
- `app/runtime/v2/model/model_package_v2.h`, `.cpp`: `ModelPackageV2`, versioned neutral metadata and provenance.
- `app/runtime/tests/model_package_repository_test.cpp`, `model_activation_rollback_test.cpp`, `model_package_compatibility_test.cpp`.

### Reusable behavior that must be preserved

- **Extract reusable mechanics:** discovery, package copy/install, hashes, identity/version handling, metadata parsing, atomic persistence, corrupt/missing-file diagnostics, and prior-model rollback.
- Preserve inference-required preprocessing/class metadata and training provenance.

### Obsolete product policy or structural coupling to remove

- Replace Class 1/Single fixed-target assumptions.
- Replace promotion/validation status as product lifecycle policy while preserving technical package-integrity validation.
- Replace manual “Use trained model” activation after successful save; D-003 requires the saved model to become active.
- Remove model activation authority from widgets/controllers.

### Proposed target boundary

`ModelPackageRepository` owns installed packages; `ModelPackageValidator` checks schema/files/hashes/runtime compatibility without deciding scientific promotion; `ActiveModelService` alone owns active package state and performs atomic save/install/activate with rollback.

### Characterization tests required first

- Current package discovery/copy/hash fixtures and malformed/missing package cases.
- Binary and multi-class packages, nonnumeric IDs, class reordering, and metadata version compatibility.
- Duplicate ID/version, partial copy, disk failure, corrupt hash, incompatible runtime, and restart recovery.
- Save-and-auto-activate success and activation failure rollback to the prior model.
- Old-package read compatibility without silently rewriting policy fields.

### Implementation changes

1. Add versioned neutral package and repository contracts.
2. Isolate technical integrity validation from product policy.
3. Make `ActiveModelService` the P0-3 authoritative owner.
4. Connect P1-3 successful save to atomic install/activate.
5. Adapt P1-1 inference to acquire the active package through the service.
6. Retire old promotion/manual-activation UI policy after end-to-end acceptance.

### Build and test commands

```powershell
cmake --build <build-dir> --config Release --target desktop_app model_package_repository_test model_activation_rollback_test model_package_compatibility_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "metadata|model_package|model_activation"
python -m pytest training/python/droplet_trainer -q
```

### Hardware qualification requirements

No camera/DAQ hardware is required. Windows CPU/GPU runtime qualification is required before a package is considered loadable/activatable on a qualified system.

### Acceptance criteria

- Package integrity mechanics pass current and v2 fixtures.
- Technical validation contains no fixed target/promotion Decision policy.
- A successfully saved model becomes active atomically.
- Failed install/load/activation leaves the previous active model usable and records the failure.
- Active Model state has one owner and survives restart.

### Rollback point

Preserve the prior active package and registry database until new activation completes. The PR can revert to the accepted P1-1 registry adapter while leaving installed package files untouched.

### Dependencies on earlier work packages

Depends on P0-3 Active Model ownership, P1-1 neutral inference/package metadata, and P1-3 trainer handoff. It can proceed in parallel with P1-4/P1-5 after interfaces stabilize.

## P1-7 — V2 QML application shell and first vertical slice

### Objective

Create the v2 QML shell and shared Data > Capture workspace from the approved information architecture and interaction/state specification, then deliver one vertical slice through typed application services. Do not port old widget navigation or product policy. Capture has one shared live Camera preview and three fixed, independently collapsible right-panel sections. The first production vertical slice establishes the shared Camera boundary and preview; individual capture operations remain separately bounded.

### Exact files and symbols involved

Current reference-only/legacy files:

- `app/runtime/desktop_app/main_window.h`, `.cpp`.
- `workspace_camera.*`, other `workspace_*`, Reports/Validator controllers, Settings surfaces, and `image_validation_dialog.*`.
- `camera_workspace_controller.*` as technical-flow evidence, not a v2 UI authority.

Proposed files and symbols:

- `app/runtime/v2_qml/CMakeLists.txt` and v2 application target.
- `app/runtime/v2_qml/main.cpp`: service composition only.
- `app/runtime/v2_qml/qml/Main.qml`, `ApplicationShell.qml`, approved navigation component(s), `CaptureWorkspace.qml`, shared operation/fault/recovery components.
- `app/runtime/v2/presentation/application_shell_controller.h`, `.cpp`: `ApplicationShellController`.
- `app/runtime/v2/presentation/camera_presentation_model.h`, `.cpp`: `CameraPresentationModel`, typed commands/properties projected from `ICameraService` and `OperationCoordinator`.
- `app/runtime/tests/qml_shell_smoke_test.cpp`, `camera_presentation_model_test.cpp`, `qml_no_vendor_dependency_test.cpp`.

### Reusable behavior that must be preserved

- **Extract reusable mechanics:** current camera preview delivery, allowed camera settings application, error diagnostics, operation progress/lock information, and contextual recovery actions.
- Preserve qualified Camera service, frame conversion, and P0-2 threading behavior behind presentation adapters.
- Use old UI only as reference evidence for technical hooks; do not preserve its navigation or widget-local state.

### Obsolete product policy or structural coupling to remove

- Replace old `MainWindow` navigation/composition and old workspace surfaces in the v2 target.
- Do not expose Reports/Validator/old Settings navigation, software arming, detector selection/settings, target/sort controls, manual compute/Python/hyperparameter controls, or old product states.
- Do not let QML call DCAM, NI-DAQmx, ONNX Runtime, persistence writers, or the trainer.
- Do not use QML control text/selection/visibility as application state.

### Proposed target boundary

QML binds only to presentation models that expose immutable service snapshots and typed commands. `ApplicationShellController` handles approved navigation and contextual recovery. `CameraPresentationModel` delegates all resource/state work to `ICameraService` and `OperationCoordinator`.

### Characterization tests required first

- Screen/navigation/state acceptance fixtures derived directly from the approved IA and low-fidelity specification.
- Presentation-model tests for startup, connect, stream, stop, fault, lock conflict, recovery, and stale queued updates.
- QML import/dependency test rejecting vendor SDK and trainer dependencies.
- Preview lifetime/cadence tests proving QML cannot retain invalid frame memory or block acquisition.
- Side-by-side acceptance review against the consolidated design specification, retaining its status as `Consolidated Draft for Review`.

### Implementation changes

1. Add a separate v2 QML executable/target so the current desktop app remains the rollback path.
2. Build the approved shell/navigation and shared state/recovery surfaces.
3. Bind the shared Capture preview exclusively through `CameraPresentationModel`; do not give individual Capture sections separate Camera owners.
4. Integrate P0-3 operation locks and P1-4 Camera service.
5. Add UI acceptance and no-vendor-dependency tests.
6. Do not retire the old desktop target in this package; future workspace slices replace it incrementally under separate review.

### Build and test commands

```powershell
cmake -S app/runtime -B <build-dir> -DENABLE_NIDAQMX=ON
cmake --build <build-dir> --config Release --target opendss_v2_qml qml_shell_smoke_test camera_presentation_model_test qml_no_vendor_dependency_test
ctest --test-dir <build-dir> -C Release --output-on-failure -R "qml_shell|camera_presentation|no_vendor_dependency"
ctest --test-dir <build-dir> -C Release --output-on-failure
```

### Hardware qualification requirements

Software/offscreen QML tests cover shell and state. **Hardware qualification required** for live preview, camera settings, connect/start/stop, fault/recovery, sustained cadence, and exit while streaming on supported DCAM hardware. DAQ behavior is not part of this first vertical slice.

### Acceptance criteria

- Shell and Camera workspace match the controlling v2 IA/interaction documents; the consolidated visual specification remains a draft for review.
- QML has no vendor SDK, trainer, writer, detector-parameter, or direct persistence dependency.
- Camera state and operations come only from authoritative services.
- Widget/control text is never queried as state.
- Resource locks, fault state, and contextual recovery are visible and tested.
- The legacy desktop target remains available as the rollback path.

### Rollback point

The v2 QML application is a separate target. Disable/remove that target from packaging and continue using the accepted legacy desktop executable without reverting service packages.

### Dependencies on earlier work packages

Depends on P0-1 through P0-3 and P1-4. It may use P1-1/P1-5/P1-6 service snapshots for shell status only after those packages are accepted; the first Camera vertical slice must not block on Dataset, Training, or DAQ UI completion.

## Package dependency sequence

```text
P0-1 Detector contract/adapters/evidence
  └─ P0-2 Bounded processing, persistence, and DAQ handoff
      └─ P0-3 Authoritative state and operation ownership
          ├─ P1-1 Inference vs Decision/routing separation
          │   ├─ P1-2 Droplet Dataset Capture v2 boundary
          │   ├─ P1-5 DAQ ownership
          │   └─ P1-6 Model package/activation policy
          ├─ P1-3 TrainingService ────────────────┘
          └─ P1-4 Camera/DCAM ownership
              └─ P1-7 V2 QML shell + Camera vertical slice
```

P1 packages may overlap only where their contracts are already accepted and they do not create two authoritative owners. Hardware-dependent acceptance remains sequential at the affected boundary.

## Next implementation slice

P0-1 is completed. The previous **Qt Quick/QML v2 Shell and Mock Single Image** baseline established the generated shell but its Single Image-only Capture composition is superseded by the amended D-014 Capture decision. No production Camera continuation is authorized until the shared Capture workspace visual baseline is reviewed. The corrected baseline should:

- add a separate Qt Quick/QML v2 executable beside the legacy application;
- implement the approved shell, navigation, status header, workspace host, operation-side panel, and shared Hardware panel presentation;
- start at Data > Capture with Single Image, Image Sequence, and Droplet Dataset Capture headings fixed and all three sections collapsed;
- allow multiple idle sections to expand, divide remaining body height evenly, and scroll each expanded body independently;
- keep an active section expanded while the other headings remain visible but disabled, and keep Completed, Interrupted, or Failed results expanded until manually collapsed;
- use one minimal mock authoritative state owner and a narrow fake Camera projection for deterministic visual review without hardware or production persistence; and
- keep real DCAM preview, real TIFF capture, long-running capture mechanics, and physical-camera qualification for separately authorized later slices.

The exact scope, constraints, verification, and acceptance criteria are defined in [`current-slice.md`](current-slice.md). This planning update does not authorize implementation.
