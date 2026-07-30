# OpenDSS v2 Reusable-Core Architecture and Redundancy Audit

**Status:** Report-only audit  
**Audit date:** 2026-07-21  
**Baseline:** `docs/v2/` at Phase 1 commit `b2a505399e6cfa2c61bea12df6297894184bd701`

## 1. Executive summary

The repository contains substantial reusable technical mechanics: both vendor integrations, real-time camera acquisition, the fast detector, ONNX Runtime inference, Python training/export, and the background/atomic persistence utilities should be preserved. Most v2 work is boundary work, not technical replacement: isolate hardware, inference, training, and persistence behind application services, then remove old product policy from their public contracts.

The greatest architecture risk is that `MainWindow`, controller-owned widget state, `AppState`, and `PipelineRunner` jointly own or infer camera, DAQ, active-model, classification, and operation state. Some state is reconstructed from label text. This conflicts with the v2 requirement for one authoritative owner per domain state and makes resource locks and recovery difficult to reason about.

`EventDetector` and `FastEventDetector` are overlapping alternative algorithms, not exact duplicates. `EventDetector` is reachable through the CLI's `precise` mode; `FastEventDetector` is the default CLI detector and the only detector used by the desktop Live, Sequence Test, and Dataset Capture paths. The safe recommendation is to make `FastEventDetector` the provisional canonical implementation behind one public detector contract, retain `EventDetector` temporarily as an internal reference/strategy, and retire or merge it only after output-equivalence, timing, and hardware characterization.

No production C++ or Python file was changed by this audit.

### Safe conclusions now

- Both detector implementations are built and reachable; neither is presently dead code.
- `FrameGrabber` is built but has no construction or call site found outside its own declaration and definition.
- The GUI and CLI use separate DCAM wrappers that duplicate device and SDK lifetime ownership.
- The ONNX classifier supports a variable class count, while binary target-class and routing assumptions are introduced by metadata, registry, pipeline, dataset, and UI layers.
- Dataset Capture still embeds Hit/Waste/Mixed and automatic model-label policy that conflicts with the v2 product model.
- The old workspace, Reports, Validator, and Settings modules are currently constructed; they are obsolete or superseded UI/policy candidates, not unreferenced code.
- `AppState::triggerArmed` has no consumer found and represents a superseded software-arming concept.

### Changes requiring characterization tests

- Any detector retirement, common configuration change, crop-path consolidation, metadata schema change, class/routing change, model-package change, persistence consolidation, or trainer default/quality-gate change.
- Any attempt to move DAQ firing or file writing off the acquisition path, because timing and ordering can change even when output data appears equivalent.
- Any replacement of duplicated tracking, counters, direction logic, image conversion, or background construction.

### Changes requiring hardware qualification

- Consolidating the two DCAM wrappers or changing device, buffer, wait, conversion, thread, or shutdown behavior.
- Changing NI-DAQmx task creation, sample-rate fallback, waveform generation, delays, output routing, task wait, or reset behavior.
- Qualifying detector latency and trigger timing with the real camera/DAQ path.

### Unresolved findings

- The repository has no representative detector fixture corpus proving whether the two detector implementations are scientifically equivalent.
- No hardware-in-the-loop evidence establishes the safe queue depth, latency budget, camera bit-depth conversion, or physical-route timing.
- The intended disposition of the publication-experiment trainer code is unclear; one test conflicts with current initialization validation.
- Runtime evidence is insufficient to determine whether the CLI `precise` detector mode is still required by supported deployments.

## 2. Audit scope and explicit non-goals

The audit covered the reusable and partly reusable modules named in the v2 task: detection, acquisition, DAQ, inference/model loading, Dataset Capture/crops/persistence, Python training and desktop orchestration, application state/shared utilities, and legacy UI-adjacent modules. Evidence was collected from CMake membership, definitions, includes, construction and call sites, configuration use, tests, platform branches, and narrow git history where it clarified detector provenance.

The v2 documents under `docs/v2/` control product behavior and policy. Existing code was treated only as implementation and reuse evidence.

Non-goals were refactoring, deleting, renaming, consolidating, altering public interfaces or CMake targets, changing algorithms, changing vendor integrations, redesigning screens, broad formatting, and proving physical hardware behavior from compilation.

The working tree contained five pre-existing modifications before this task:

- `app/runtime/desktop_app/dataset_workspace_controller.cpp`
- `app/runtime/desktop_app/dataset_workspace_controller.h`
- `app/runtime/desktop_app/image_validation_dialog.cpp`
- `app/runtime/desktop_app/main_window.cpp`
- `app/runtime/desktop_app/workspace_model.cpp`

They were neither staged nor modified by this task. Conclusions involving them are reference evidence from the inspected working tree and should be rechecked against the eventual committed implementation before cleanup begins.

## 3. Repository, build, test, and static-analysis evidence

### Repository state

- Starting branch: `main`
- Starting HEAD: `4630aeb25e67f2fc22deb0324b85af437ceef6aa`
- Audit branch: `chore/v2-design-baseline-and-core-audit`
- Detector history: both detector families entered in the initial clean runtime import (`4347c27`); the only later detector-specific history found was formatting (`1d0fe0e`). History therefore does not establish that either is obsolete.

### Build membership and configuration

`app/runtime/desktop_app/CMakeLists.txt` places `dataset_capture_session.cpp`, `cli_runner.cpp`, `dcam_camera.cpp`, `event_detector.cpp`, `daq_trigger.cpp`, `fast_event_detector.cpp`, `metadata_loader.cpp`, and `onnx_classifier.cpp` in the static `opendss_runtime` target. The desktop target adds the controllers, workers, pipeline, writers, workspaces, and legacy UI-adjacent modules reviewed below.

A normal Release desktop build was attempted with the existing build tree:

```text
cmake --build C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release --config Release --target desktop_app
```

Result: **passed**; `OpenDSS.exe` linked successfully. The existing configuration used Visual Studio 18 2026, Qt 6.10.1, ONNX Runtime GPU, a configured DCAM SDK path, and `ENABLE_NIDAQMX=ON`. This proves compilation/linkage only, not camera or DAQ behavior.

### CTest

The configured build registered three tests. All passed:

| Test | Result |
|---|---|
| `opendss_runtime_metadata_loader_test` | Passed |
| `opendss_event_direction_test` | Passed |
| `opendss_trainer_plot_math_test` | Passed |

CTest summary: **3 passed, 0 failed**. None exercises detector output, hardware, pipeline triggering, crop generation, or persistence recovery.

### Python tests

The full trainer suite was run from the repository root so its repository-relative model fixture resolved. Result: **74 passed, 3 failed, 5 skipped**.

- `publication_experiments/test_phase3b.py::BoundaryTuningTests::test_alpha_validation_and_serialization` failed because the generated configuration uses an initialization value rejected by current `_validate_training_config` (`imagenet` or `checkpoint` required).
- Two `test_env_onnx_runtime.py` cases failed because the test interpreter did not have the `onnxruntime` module.
- The non-publication core suite excluding the environment-specific ONNX Runtime test was also run: **55 passed, 0 failed, 5 skipped**.

The deployed legacy training virtual environment existed, but it did not contain `pytest`; therefore the available Miniconda pytest environment executed the suite. The expected new OpenDSS training environment did not exist. Results are useful source-level evidence, not proof of the bundled environment.

### Static analysis and lint

The available Qt/C++ deterministic audit linter scanned 106 C++ and header files and reported 247 candidates. Most were include/style/pattern findings outside this audit's material scope. Relevant signals were unchecked file/JSON error paths, timeout/lifecycle concerns, and dependency coupling in controller/UI code; each was verified against source before being used below. No `QAbstractItemModel`, `QAbstractListModel`, or `QAbstractTableModel` subclass was found, so model-contract review was not applicable. No repository-configured clang-tidy target or comparable configured static-analysis command was found.

## 4. Current technical-core dependency map

```text
CLI
 ├─ DcamCamera ─────────────── DCAM SDK
 ├─ EventDetector (precise)
 ├─ FastEventDetector (default)
 ├─ OnnxClassifier + MetadataLoader
 └─ DaqTrigger ─────────────── NI-DAQmx

Desktop MainWindow / workspace controllers
 ├─ CameraWorkspaceController
 │   └─ CameraWorker (QThread)
 │       └─ DcamController ─── DCAM SDK
 │
 ├─ PipelineRunner
 │   ├─ FastEventDetector
 │   ├─ OnnxClassifier + MetadataLoader
 │   └─ DaqTrigger ─────────── NI-DAQmx
 │
 ├─ DatasetCaptureSession / CollectionPostprocessor
 │   └─ crop files, manifests, labels, counters
 │
 ├─ LiveDataCollectionWriter / LiveLogWriter
 ├─ SequenceSummaryWriter / JsonPersistence
 ├─ ModelRegistryService
 └─ trainer QProcess orchestration
     └─ training/python/droplet_trainer

Cross-cutting ownership today
 ├─ AppState
 ├─ widget/controller-local state and label text
 ├─ PipelineRunner configuration/readiness
 └─ MainWindow composition, locks, routing, and persistence
```

The target v2 boundary should replace the cross-cutting bottom section with authoritative Camera, DAQ, Model/Inference, Dataset, Run/Sequence, Training, and Operation state owners. QML/UI should consume those services and never call vendor SDKs or the trainer directly.

## 5. Findings by module group

### Group A — Droplet detection

#### F-A01 — The detectors are reachable alternative implementations, not exact duplicates

- **Module group:** A — Droplet detection
- **Affected files:** `app/runtime/event_detector.*`, `app/runtime/fast_event_detector.*`, `app/runtime/cli_runner.cpp`, `app/runtime/desktop_app/pipeline_runner.*`
- **Category:** Alternative implementation; overlapping implementation
- **Actual consumers or evidence of no consumers:** `EventDetector` is constructed by the CLI for `--detect-mode precise`. `FastEventDetector` is the CLI default and is owned by `PipelineRunner`, which serves desktop Live, Sequence Test, and Dataset Capture/collection paths. Both are members of `opendss_runtime`.
- **Description:** `EventDetector` is stateless at the frame-call level and uses explicit multi-frame background construction, normalized absolute difference, optional local/self background, Gaussian blur, percentile/Otsu thresholding, morphology, contour filtering, and optional masks. `FastEventDetector` downsamples, uses fixed difference thresholding, box blur and connected components, owns rolling-background and hysteresis/re-fire state, and scales 16-bit input by `/256`.
- **Evidence:** Construction/call search, CMake membership, detector configuration and implementation branches, and current CLI mode dispatch. No history evidence establishes a reference/production designation.
- **v2 impact:** Exposing both would allow workspaces to choose scientific behavior and duplicate detector configuration, contrary to one authoritative public detector contract.
- **Risk of changing it:** High; output, direction, crop timing, trigger timing, and false-positive/false-negative behavior can change.
- **Recommendation:** **Consolidate after characterization**. Use `FastEventDetector` as the provisional canonical/default implementation behind one contract because all desktop workflows already depend on it. Retain `EventDetector` temporarily as an internal reference/strategy until representative comparisons establish whether its Otsu, local-background, morphology, or background-mode behavior is required.
- **Confidence level:** High for use relationships and algorithm differences; medium for eventual retirement.
- **Prerequisite tests or measurements:** Recorded and hardware sequences spanning empty channel, single/multiple droplets, both directions, lighting drift, bubbles/debris, boundary events, 8/16-bit inputs, and restart/re-entry; compare event count, masks, boxes, centroids, direction, crops, trigger decisions, latency percentiles, and CPU load.

#### F-A02 — Detector contracts, state, and tests are fragmented

- **Module group:** A — Droplet detection
- **Affected files:** detector files, `pipeline_runner.*`, `dataset_capture_session.*`, Live/Sequence consumers, `tests/event_direction_test.cpp`
- **Category:** Overlapping implementation; inconsistent data contract; test gap
- **Actual consumers or evidence of no consumers:** The desktop pipeline consumes `FastDetectionResult::fired`, while CLI precise mode supplies separate caller-side hysteresis. Direction is tested independently, but no registered test constructs either detector.
- **Description:** Background ownership, trigger/hysteresis state, output type, mask production, scaling, and configuration are split differently between implementations and consumers. Common types/configuration cannot be safely unified by field matching because equal names do not imply equal semantics.
- **Evidence:** Detector public headers, pipeline dispatch, CLI hysteresis path, and the three registered CTests.
- **v2 impact:** Without one event/result contract, detection, decision, observed route, crops, and physical trigger can remain conflated.
- **Risk of changing it:** High.
- **Recommendation:** **Preserve behind adapter** until characterization defines a public frame/event result contract and fixed qualified configuration. Detector strategy must remain internal, not user- or workspace-selectable.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Contract tests for background readiness, reset, re-entry, multi-frame events, no-detection background update, mask/crop coordinates, bit-depth conversion, and deterministic replay.

### Group B — Camera and acquisition

#### F-B01 — CLI and desktop duplicate DCAM ownership

- **Module group:** B — Camera and acquisition
- **Affected files:** `app/runtime/dcam_camera.*`, `app/runtime/desktop_app/dcam_controller.*`, `camera_worker.*`
- **Category:** Overlapping implementation; platform/hardware-specific code
- **Actual consumers or evidence of no consumers:** `DcamCamera` is used by the CLI. `DcamController` is owned by `CameraWorker` for the desktop path.
- **Description:** Both wrappers initialize/uninitialize the DCAM API, open/close a device, allocate/release buffers, manage wait handles, start/stop capture, retrieve frames, and convert/copy data. They differ in consumer contract (`cv::Mat` versus Qt signals/`QImage`), device selection, and buffer defaults.
- **Evidence:** Constructors/destructors/close paths, SDK calls, CMake membership, and construction sites.
- **v2 impact:** Two SDK owners complicate a single Camera resource owner, lock state, and recovery contract.
- **Risk of changing it:** High because SDK lifetime, buffer release, bit depth, device reopen, and thread affinity are hardware-sensitive.
- **Recommendation:** **Preserve behind adapter**. Do not rewrite DCAM mechanics. Define one Camera adapter contract first; consolidate common ownership only after hardware characterization while retaining CLI and Qt conversion adapters if their consumer contracts justify them.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Repeated open/start/stop/reopen, property application, timeout/unplug/failure recovery, 8/16-bit frame fidelity, buffer lifetime, multi-device selection, leak checks, sustained frame rate, and shutdown during capture on supported hardware.

#### F-B02 — `FrameGrabber` appears compiled but unreferenced

- **Module group:** B — Camera and acquisition
- **Affected files:** `app/runtime/desktop_app/frame_grabber.*`, desktop CMake source list
- **Category:** Unreferenced code
- **Actual consumers or evidence of no consumers:** The files are compiled into the desktop target. No construction, member, include, call, reflection lookup, or runtime dispatch reference was found outside their own declaration/definition and target membership.
- **Description:** The class exposes start/stop state but does not implement frame acquisition. Its intended role is now covered by `CameraWorker`/`DcamController`.
- **Evidence:** CMake membership plus repository-wide symbol/reference search.
- **v2 impact:** It is misleading evidence of a second acquisition path and increases ownership ambiguity.
- **Risk of changing it:** Low to medium; external/plugin consumers are not evidenced by this repository.
- **Recommendation:** **Retire after characterization** after one clean build/link and packaging smoke test confirms no generated or external lookup.
- **Confidence level:** High within this repository.
- **Prerequisite tests or measurements:** Clean desktop build, startup smoke test, and confirmation that no downstream plugin/API contract consumes the class.

#### F-B03 — Acquisition, conversion, UI, and recording work share one worker path

- **Module group:** B — Camera and acquisition
- **Affected files:** `camera_worker.*`, `camera_workspace_controller.*`, `workspace_camera.*`, `main_window.cpp`, relevant frame/LUT conversion code
- **Category:** Mixed UI/domain/hardware responsibilities; duplicated conversion/copy; state duplication
- **Actual consumers or evidence of no consumers:** `CameraWorkspaceController` coordinates widgets, worker, app state, pipeline, last frame/metadata, and LUT; `CameraWorker` invokes the record hook in its camera thread and also copies/emits preview frames.
- **Description:** Frame acquisition, conversion, preview delivery, processing, physical output, and recording can occur on the same acquisition path. Camera state also exists in worker/controller flags, `AppState`, widgets, and label-derived logic.
- **Evidence:** Dependency structure and signal/hook calls; `main_window.cpp` reconstructs streaming state from status label text.
- **v2 impact:** Prevents one authoritative Camera state owner and makes locks, backpressure, and contextual recovery non-deterministic.
- **Risk of changing it:** High because extra copies or queueing changes latency and frame lifetime.
- **Recommendation:** **Extract reusable mechanics** into a Camera service plus explicit frame ownership/conversion contract; retain the proven worker/SDK loop behind it.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Thread-affinity assertions, bounded-queue/backpressure tests, frame lifetime and conversion fidelity, preview-versus-processing cadence, shutdown with in-flight frames, and sustained acquisition under recording/DAQ load.

### Group C — DAQ and physical output

#### F-C01 — NI-DAQmx mechanics are reusable, but firing blocks the processing path

- **Module group:** C — DAQ and physical output
- **Affected files:** `app/runtime/daq_trigger.*`, `pipeline_runner.*`, CLI, manual/test trigger paths
- **Category:** Necessary hardware implementation; blocking I/O on processing path
- **Actual consumers or evidence of no consumers:** CLI and `PipelineRunner` own `DaqTrigger`; settings/probe and manual test paths construct additional instances.
- **Description:** `DaqTrigger` provides RAII task cleanup, device discovery, validation, finite sine waveform generation, device-rate fallback, output write/wait, and a final zero sample. `fire()` can sleep for delay and wait for task completion. The desktop calls it from `PipelineRunner::processFrame`, which is reached through the camera worker record hook.
- **Evidence:** Construction sites, compile branches, waveform and wait calls, and camera-worker hook flow.
- **v2 impact:** Vendor mechanics should survive, but blocking physical output can stall acquisition and couples decision timing to frame processing.
- **Risk of changing it:** Very high; waveform, timing, physical routing, and device cleanup are safety/correctness concerns.
- **Recommendation:** **Preserve behind adapter**. Keep NI-DAQmx mechanics intact and define a single DAQ command/status adapter. Any asynchronous execution change requires ordering and timing characterization.
- **Confidence level:** High for code path; unresolved for real-device timing.
- **Prerequisite tests or measurements:** Oscilloscope/logic-analyzer pulse shape and latency, max sustained event rate, queue policy, cancellation/reset, device fault/unplug, rate fallback, and camera-frame continuity during output.

#### F-C02 — DAQ readiness, routing, and obsolete arming policy have multiple owners

- **Module group:** C — DAQ and physical output
- **Affected files:** `app_state.h`, `pipeline_runner.*`, settings/controller code, `main_window.cpp`, sequence/live paths
- **Category:** State duplication; obsolete product-policy branch
- **Actual consumers or evidence of no consumers:** DAQ availability/fault/waveform/status are written across settings, pipeline, `AppState`, and UI. `AppState::triggerArmed` has no reference beyond its declaration. Target/non-target routing and `sortNonTarget` influence firing.
- **Description:** Hardware readiness, software intent, classifier target, outlet direction, and output channel are not separate contracts. UI text is used to infer some DAQ state. Software arming remains as an unused field/concept even though D-002 removes it.
- **Evidence:** Symbol/reference search and state assignment paths.
- **v2 impact:** Hit Class, Hit Outlet Direction, and DAQ Output Channel cannot be independently owned or audited, and superseded arming can leak back into v2.
- **Risk of changing it:** High where routing or hardware output changes; low for deleting the dead field only after interface/build verification.
- **Recommendation:** **Extract reusable mechanics** and move routing to the v2 application layer. Treat `triggerArmed` as a future **Retire as obsolete UI/policy** candidate, not an implementation requirement.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Decision-to-channel truth table, output-disabled/fault state tests, startup/recovery transitions, explicit no-software-arming acceptance test, and physical direction/channel verification.

### Group D — Inference, model loading, and routing coupling

#### F-D01 — ONNX inference is reusable; target selection is not low-level inference

- **Module group:** D — Inference, model loading, and routing coupling
- **Affected files:** `app/runtime/onnx_classifier.*`, `metadata_loader.*`, `pipeline_runner.*`
- **Category:** Reusable mechanics mixed with obsolete product policy
- **Actual consumers or evidence of no consumers:** CLI and desktop pipeline load metadata and classify crops. `PipelineRunner` maps class results to `shouldTrigger` and DAQ firing.
- **Description:** `OnnxClassifier` handles session/provider setup, tensor preprocessing, normalization, inference, and argmax for an arbitrary output count. `MetadataLoader::ResolveTargetClassId` defaults binary class `1` or legacy `Single`; pipeline fields include target class/label, `sortNonTarget`, compute device, and CUDA flags.
- **Evidence:** Metadata parsing/defaults, classifier output sizing, pipeline configuration and trigger logic.
- **v2 impact:** Predicted Class, Decision, and Observed Route remain conflated, and Class 1/target assumptions can become v2 policy.
- **Risk of changing it:** High for preprocessing/provider/output semantics; medium for relocating policy with contract tests.
- **Recommendation:** **Preserve behind adapter** for ONNX and metadata mechanics; **Extract reusable mechanics** from target/routing logic into a v2 ModelPackage/Inference service plus separate Decision/Routing service.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Golden preprocessed tensors, logits/probabilities and argmax across CPU/GPU, multi-class metadata, class order/display labels, threshold boundary cases, and explicit Predicted Class/Decision/Observed Route contract tests.

#### F-D02 — Model registry mechanics are coupled to binary promotion/validation policy

- **Module group:** D — Inference, model loading, and routing coupling
- **Affected files:** `app/runtime/desktop_app/model_registry_service.*`, model/registry types and desktop consumers
- **Category:** Reusable mechanics mixed with superseded product policy
- **Actual consumers or evidence of no consumers:** Model workspace/training/activation paths use the service for package discovery, validation, copying, hashes, status, and activation.
- **Description:** Package discovery/copy/hash and model metadata mechanics are reusable. Binary target-class assumptions, promotion/validation status and quality policy, and explicit post-save activation reflect the old model. Current trainer integration verifies that a saved model stays inactive until confirmation, while D-003 requires automatic activation on save.
- **Evidence:** Registry fields/status logic, desktop activation path, trainer-save verification, and model metadata defaults.
- **v2 impact:** Old validation/promotion and target policy can silently govern v2 model lifecycle.
- **Risk of changing it:** High for package integrity and activation rollback; medium for replacing policy after lifecycle tests.
- **Recommendation:** **Extract reusable mechanics** into a model package/registry service and **Retire as obsolete UI/policy** the superseded promotion/manual-activation policy after v2 lifecycle characterization.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Atomic package install, hash failure, duplicate ID/version, multi-class metadata, save-and-auto-activate, activation rollback, missing/corrupt model, and active-model persistence/recovery.

#### F-D03 — Classifier reuse currently depends on serialized access and implicit output assumptions

- **Module group:** D — Inference, model loading, and routing coupling
- **Affected files:** `onnx_classifier.*`, `pipeline_runner.*`, metadata/result types
- **Category:** Thread-safety risk; inconsistent data contract; unreferenced compatibility fields
- **Actual consumers or evidence of no consumers:** Current desktop access is serialized by pipeline ownership/mutex and camera flow. `PipelineConfig::useCuda` was found only as a fallback read, with no assignment by current consumers. `PipelineRunner::toLowerAscii` and `DcamCamera::isOpened` have declarations/definitions but no call site found.
- **Description:** `classify` is logically const but reuses mutable buffers, so concurrent calls would race. Output class count is not strongly checked against metadata class count before routing. Hidden global readiness-verifier state governs CUDA qualification/fallback.
- **Evidence:** Member mutability, session/result logic, mutex/use sites, and symbol/reference searches.
- **v2 impact:** A future service or concurrent workspace use could introduce silent races or class-map errors; compatibility fields can perpetuate manual compute policy.
- **Risk of changing it:** Medium to high.
- **Recommendation:** **Investigate further** before removing compatibility members; preserve serialized inference or introduce per-session buffers, validate output/metadata contracts, and keep automatic provider policy below the UI.
- **Confidence level:** High for references and mutability; medium for external consumers.
- **Prerequisite tests or measurements:** Concurrent-call stress test if concurrency is intended, output-count mismatch tests, provider fallback/qualification fixtures, and clean build/API-consumer search before member retirement.

### Group E — Dataset Capture, crop creation, and persistence

#### F-E01 — Dataset Capture mechanics are inseparable today from obsolete binary capture policy

- **Module group:** E — Dataset Capture, crop creation, and persistence
- **Affected files:** `app/runtime/dataset_capture_session.*`, dataset controllers/types, pipeline consumers
- **Category:** Obsolete product-policy branch mixed with reusable mechanics
- **Actual consumers or evidence of no consumers:** Desktop Dataset Capture uses the session and its HitOnly/WasteOnly/Mixed modes, counters, labels, crop copies, and manifest persistence.
- **Description:** Session/directory creation, crop transfer/hash, counters, and durable manifest mechanics are reusable. Hit/Waste/Mixed modes, model-linked automatic labels, class `0=waste`/`1=hit`, and Hit/Waste/Exclude schemas contradict v2 Dataset Capture behavior.
- **Evidence:** Capture-mode enum, label assignment, mixed-mode prediction path, manifest fields, and desktop callers.
- **v2 impact:** v2 Dataset Capture could inherit hidden classification and binary labels that are explicitly out of scope.
- **Risk of changing it:** High for existing dataset compatibility and recovery; medium for policy removal with migration fixtures.
- **Recommendation:** **Extract reusable mechanics** and replace the surrounding schema/policy in a separate reviewed task. Do not discard crop, hash, counter, or recovery mechanics.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Existing manifest round-trip and recovery, unlabeled v2 capture, multi-class/neutral schema, crop hash/fidelity, interrupted session, duplicate/retry, and compatibility/migration fixtures.

#### F-E02 — Crop and persistence paths overlap, and live disk I/O can block acquisition

- **Module group:** E — Dataset Capture, crop creation, and persistence
- **Affected files:** `collection_postprocessor.*`, `live_data_collection_writer.*`, `live_log_writer.*`, `sequence_summary_writer.*`, `json_persistence.*`, `dataset_capture_session.*`, crop helpers in CLI/pipeline
- **Category:** Overlapping implementation; blocking I/O; duplicated atomic persistence; unbounded collection
- **Actual consumers or evidence of no consumers:** Live collection writes frames/CSV from the camera record hook; postprocessing and pipeline/CLI create crops; session code persists manifests; sequence/live paths use separate writers.
- **Description:** Crop square/resize/write behavior exists in several paths with different surrounding schemas. `JsonPersistence` uses `QSaveFile` and retry mechanics, while `DatasetCaptureSession` independently uses temp/rename/remove logic. Live TIFF/CSV writes run synchronously on the acquisition hook. Full-capture and log vectors can grow for the duration of a run before flushing.
- **Evidence:** Writer call chains, save/append calls, crop functions, atomic replacement implementations, and buffer/vector ownership.
- **v2 impact:** Timing stalls, inconsistent crops/schemas, memory growth, and divergent recovery semantics can undermine Dataset and Run records.
- **Risk of changing it:** High because ordering, file format, checksums, and frame/event association can change.
- **Recommendation:** **Consolidate after characterization**. Preserve proven writer/queue/atomic mechanics; define one crop contract and one persistence primitive, then move bounded background writing off processing paths without changing qualified output.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Golden crop bytes/coordinates, writer schema snapshots, interruption/power-loss simulation, replacement failure, queue backpressure/drop policy, long-run memory, disk saturation, event/frame ordering, and throughput under camera load.

### Group F — Python training and desktop orchestration

#### F-F01 — Trainer implementation and export are reusable behind a TrainingService

- **Module group:** F — Python training and desktop orchestration
- **Affected files:** `training/python/droplet_trainer/**`, desktop QProcess/progress/environment/model-save integration
- **Category:** Reusable mechanics mixed with orchestration/UI policy
- **Actual consumers or evidence of no consumers:** Desktop code launches the package through QProcess/JSONL; Python CLI/config/dataset/train/export/validation modules are covered by the trainer test suite.
- **Description:** Dataset loading, deterministic configuration, training, progress events, ONNX export, and model metadata generation are substantial reusable mechanics. Desktop launch/environment/progress/save logic is spread through UI/controllers rather than one TrainingService.
- **Evidence:** Python entry points/imports, desktop process launch and JSONL parsing, model save flow, and 55-pass core test run.
- **v2 impact:** UI could call the trainer directly and own process/environment/model lifecycle, contrary to the application-layer boundary.
- **Risk of changing it:** High for reproducibility, export compatibility, progress/error protocols, and recovery.
- **Recommendation:** **Preserve behind adapter**. Keep the trainer; move process launch, bundled-environment verification, progress, cancellation, and save/activation into a TrainingService.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Known-dataset deterministic run, cancel/restart, malformed JSONL, process crash, missing environment, CPU/GPU auto selection, ONNX parity, metadata/package integrity, and save/auto-activation rollback.

#### F-F02 — Training defaults and lifecycle expose superseded policy in multiple layers

- **Module group:** F — Python training and desktop orchestration
- **Affected files:** Python config/metadata/validation/train modules, `dataset_workspace_controller.*`, `main_window.cpp`, settings/model UI
- **Category:** Duplicated configuration defaults; obsolete UI/policy; compatibility/fallback implementation
- **Actual consumers or evidence of no consumers:** Desktop exposes Python path, Auto/CPU/GPU selection, and editable hyperparameter JSON; Python owns another set of defaults and binary schema aliases. Metadata includes promotion fields and a target-1/waste-0 promotion gate.
- **Description:** Manual environment, compute, and hyperparameter controls conflict with v2 fixed Faster/More Accurate choices and automatic device selection. Binary promotion/validation and manual post-save activation are old product policy. `MODEL_COLLAPSE_DETECTED` guards nonfinite/constant/missing-class output; that is a technical artifact-integrity check and should not be deleted merely because promotion policy is obsolete.
- **Evidence:** Settings keys and controls, Python defaults/metadata validation, trainer failure codes, and save/activation path. Full suite result also exposes one publication configuration rejected by current initialization validation.
- **v2 impact:** Hidden or restored expert controls could reintroduce prohibited settings; removing all validation could instead allow corrupt artifacts.
- **Risk of changing it:** High.
- **Recommendation:** **Extract reusable mechanics**. Keep technical integrity diagnostics, centralize qualified defaults in the TrainingService/package, and **Retire as obsolete UI/policy** manual environment/device/hyperparameter, binary promotion, and manual-activation surfaces when v2 acceptance tests exist.
- **Confidence level:** High for current coupling; medium for the intended publication-experiment disposition.
- **Prerequisite tests or measurements:** Faster/More Accurate golden configs, automatic provider selection, binary and multi-class training, collapse/corrupt-artifact rejection, metadata compatibility, publication fixture decision, and automatic activation/rollback.

### Group G — Application state, shared types, and utilities

#### F-G01 — Domain state has multiple authoritative-looking owners

- **Module group:** G — Application state, shared types, and utilities
- **Affected files:** `app_state.h`, `app_context.*`, `main_window.cpp`, workspace controllers, `pipeline_runner.*`
- **Category:** State duplication; mixed UI/domain/hardware responsibilities
- **Actual consumers or evidence of no consumers:** Camera/DAQ/model/routing state is read and written by `MainWindow`, controllers, pipeline, widgets and `AppState`; camera/DAQ values are also inferred from status labels.
- **Description:** `AppState` contains active model, target class, sort-nontarget, streaming and DAQ fields but is not the sole owner. `AppContext` holds options/paths rather than domain state. Controller raw dependencies and widget-local state remain authoritative in practice.
- **Evidence:** Assignments/reads and label-text reconstruction in `main_window.cpp`; controller dependency structures.
- **v2 impact:** Violates single-owner domain state, makes locks/resource recovery contextual, and risks stale widget-local copies.
- **Risk of changing it:** High because state transitions touch every workspace and hardware lifecycle.
- **Recommendation:** **Consolidate after characterization** into explicit domain services/state machines; UI must observe state and issue commands, not infer or own it.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Startup, open/close, operation lock, fault/recovery, workspace navigation, active-model change, dataset/run/sequence ownership, and stale-event ordering tests.

#### F-G02 — Shared types and trackers encode historical cross-layer policy

- **Module group:** G — Application state, shared types, and utilities
- **Affected files:** `app_types.h`, live/sequence stats and event tracking, direction types/helpers
- **Category:** Types shared across layers; overlapping implementation; obsolete terminology
- **Actual consumers or evidence of no consumers:** Pipeline, live, sequence, UI, writers, and reports share these types. Live and Sequence paths maintain related but separate event/hysteresis/direction/counter logic.
- **Description:** `app_types.h` mixes options/domain results with Qt UI images/maps. Event/result structures combine labels, trigger intent/result, and later Hit/Waste/Unknown route interpretation. Separate trackers can diverge. `Unknown` does not express the v2 `Unresolved` observed-route contract.
- **Evidence:** Type members and consumer/writer schemas; tracker implementations and direction test.
- **v2 impact:** Predicted Class, Decision, and Observed Route cannot evolve independently, and old Hit/Waste semantics leak across layers.
- **Risk of changing it:** High for file formats and acceptance output.
- **Recommendation:** **Consolidate after characterization** into narrow domain contracts and one shared event tracker where behavior is equivalent; keep UI conversion types at the boundary.
- **Confidence level:** High for coupling; medium for tracker equivalence.
- **Prerequisite tests or measurements:** Event lifecycle replay, both directions, unresolved/ambiguous routes, counter consistency across Live and Sequence, serialization snapshots, and UI conversion boundary tests.

#### F-G03 — Main composition and background-task error handling are structural risks

- **Module group:** G — Application state, shared types, and utilities
- **Affected files:** `main_window.cpp`, `background_task_registry.*`, `app_utils.*`, `app_options.*`, `app_paths.*`
- **Category:** Mixed responsibilities; error propagation; generic utility accumulation; obsolete workspace option
- **Actual consumers or evidence of no consumers:** `MainWindow` composes hardware, pipeline, datasets, models, trainer, navigation and persistence. Background tasks are launched for deferred work. `initialWorkspace` still accepts old workspace names. `app_utils` mixes path, widget, time and chart helpers.
- **Description:** `BackgroundTaskRegistry` is a useful RAII join/reap mechanism, but uncaught task exceptions can terminate the process and task results/errors have no structured return. `MainWindow` and generic utilities accumulate unrelated application policy. Old startup workspace options can bypass the v2 startup contract if exposed beyond testing.
- **Evidence:** Constructor/composition responsibilities, task lambda/thread handling, option parsing and utility contents.
- **v2 impact:** Error recovery, operation state, navigation, and service lifetimes cannot be isolated cleanly.
- **Risk of changing it:** Medium to high.
- **Recommendation:** **Extract reusable mechanics** from background task and utility code; move domain work to services. Treat old workspace startup policy as **Retire as obsolete UI/policy** unless retained as an explicit test-only mechanism.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Task exception/cancel/shutdown, no-use-after-free, startup route acceptance, service lifetime teardown, and utility-consumer migration tests.

### Group H — Legacy UI-adjacent modules

#### F-H01 — Legacy screens are active but should not define v2

- **Module group:** H — Legacy UI-adjacent modules
- **Affected files:** Reports/Validator controllers and workspaces, `workspace_*`, `image_validation_dialog.*`, old Settings surfaces, `main_window.*`
- **Category:** Obsolete v1 UI; superseded product policy; reference-only behavior
- **Actual consumers or evidence of no consumers:** These modules are present in the desktop target and are constructed/composed by `MainWindow`; therefore they are not dead code.
- **Description:** Reports is superseded by Results/Runs; Validator contains potentially reusable model-test execution and summaries but old naming/composition; Dataset/Model/Camera/Settings workspaces expose old navigation, target/promotion/device/hyperparameter policies; image-validation components duplicate old review interactions.
- **Evidence:** CMake source membership, construction sites, navigation/workspace dispatch, and controller dependencies.
- **v2 impact:** Porting screens wholesale would reintroduce superseded navigation, terminology, editable controls, and product states.
- **Risk of changing it:** Medium for UI retirement; high where technical process, persistence, or model-test mechanics are embedded.
- **Recommendation:** **Retire as obsolete UI/policy** for the screens and navigation after v2 replacement acceptance; **Extract reusable mechanics** only for run-folder access, model-test execution/results, qualified validation mechanics, and proven persistence helpers.
- **Confidence level:** High.
- **Prerequisite tests or measurements:** Consumer-by-consumer extraction inventory, v2 screen acceptance, model-test result parity, saved-artifact compatibility, and clean navigation/startup/build tests before retirement.

| Legacy module | Current evidence | Classification for v2 |
|---|---|---|
| Reports workspace/controller | Built and composed; provides report/run browsing behavior | **Obsolete v1 UI**; run-folder/open-artifact mechanics are **reusable technical logic to extract** |
| Validator workspace/controller | Built and composed; launches validation/model-test flows | **Superseded product policy**; execution/progress/result mechanics are **reusable technical logic to extract** |
| `image_validation_dialog.*` and validation widget | Used by current validation/review flow | **Obsolete v1 UI** and **reference-only behavior** until v2 Model Test acceptance is fixed |
| `workspace_camera.*` | Current desktop camera surface | **Obsolete v1 UI**; no authority over v2 Camera behavior |
| `workspace_dataset.*` | Current Dataset Capture/training surface | **Obsolete v1 UI** with **superseded product policy** |
| `workspace_model.*` | Current model registry/activation surface | **Obsolete v1 UI** with **superseded product policy** |
| Old Settings surfaces | Current technical/product settings controls | **Superseded product policy**; device diagnostics may be **reusable technical logic to extract** |
| Other old `workspace_*` composition | Built and reachable through old navigation | **Safe future retirement candidate** after v2 replacement and consumer-by-consumer verification |
| `main_window.*` navigation/composition | Constructs and coordinates the current application | **Safe future retirement candidate** only after service extraction and v2 shell acceptance; not a source of v2 policy |

## 6. Detector-specific consolidation analysis

| Question | Evidence-based assessment |
|---|---|
| Which workflows use each? | `EventDetector`: CLI `precise` mode only. `FastEventDetector`: CLI default plus desktop Live, Sequence Test, and Dataset Capture/collection through `PipelineRunner`. |
| Is either unused? | No. Both are built and have reachable construction/call sites. |
| Input/output differences | Both consume frames/background information, but use different result/config types. Fast returns internal `fired` state; precise detection leaves event hysteresis to the caller. |
| Background building | Event supports explicit mean/median/max/min/local/self modes. Fast starts from mean background and can update a rolling background only while no detection/trigger is active. |
| Scaling/threshold | Event normalizes float absolute difference and uses percentile clipping plus Otsu. Fast downsamples and uses a fixed threshold; its 16-bit conversion uses `/256`. |
| Morphology/segmentation | Event uses Gaussian blur, configurable open/close morphology and contours. Fast uses box blur and connected components with real-time-oriented filtering. |
| State/hysteresis/tracking | Event is per-call/stateless; CLI adds simple reset hysteresis. Fast owns reset frames, trigger state, gap re-entry and centroid-shift re-fire behavior. |
| Equivalent outputs? | Unproven. No representative replay fixtures or detector contract tests exist. Algorithm differences make equivalence unlikely without measured bounds. |
| Behavior unique to one? | Event: Otsu, local/self and alternate background modes, morphology/contours. Fast: rolling background, internal hysteresis and re-fire tracking, downscaled real-time path. |
| Can common types/config be unified now? | A public frame/event contract can be designed, but field-level config merging is unsafe until semantic characterization. Qualified internal constants need not be user-editable. |
| Canonical recommendation | `FastEventDetector` should be provisional canonical/default because it is the existing desktop production path and contains real-time state/tracking mechanics. This is a use/evidence conclusion, not proof of superior scientific accuracy. |
| EventDetector disposition | Retain temporarily as an internal reference/alternate behind the same contract. Retire only if replay and hardware evidence show that its unique modes add no qualified requirement; otherwise merge required behavior or retain it as an internal strategy. |

**Specific recommendation:** retain two internal strategies temporarily behind one authoritative public detector interface, with `FastEventDetector` selected by fixed qualified application configuration. Do not expose strategy or duplicate detector settings to workspaces. After deterministic replay, performance, and hardware qualification, choose one of two outcomes: retire `EventDetector`, or merge/retain only its evidenced unique behavior. The repository does not currently justify direct deletion.

## 7. Reuse matrix

| Module group | Files | Current consumers | Current responsibility | Reusable mechanics | Obsolete product-policy coupling | Redundancy/overlap | Recommended v2 disposition | Required characterization tests | Hardware dependency | Risk | Confidence |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Detection | `event_detector.*`, `fast_event_detector.*` | CLI; desktop pipeline | Background subtraction, segmentation, events | Both algorithm implementations; Fast real-time state | Selectable modes/config if product-exposed | Alternative algorithms and result/state contracts | Consolidate after characterization | Golden replay, event/crop/trigger parity, latency | Camera/real-time path | High | High use; medium retirement |
| Camera | `dcam_camera.*`, `dcam_controller.*`, `camera_worker.*` | CLI; desktop | SDK/device/buffer/capture/conversion | DCAM integration and acquisition loop | Workspace/widget ownership | Duplicate SDK owners and conversions | Preserve behind adapter | Open/stream/reopen/fault/fidelity/load | DCAM hardware | Very high | High |
| Acquisition stub | `frame_grabber.*` | No consumer found | Start/stop placeholder | None evidenced beyond simple state | None | Role overlaps worker by name | Retire after characterization | Clean build/start/plugin check | No direct | Low-medium | High in-repo |
| DAQ | `daq_trigger.*`, pipeline/settings paths | CLI; Live; Sequence; probe/test | Waveform/task/output | NI-DAQmx mechanics, validation, cleanup | Arming, target/non-target and outlet coupling | Multiple readiness/owner paths | Preserve behind adapter | Waveform, latency, rate, fault/recovery | NI hardware | Very high | High code; unresolved hardware |
| Inference | `onnx_classifier.*`, `metadata_loader.*` | CLI; pipeline | Preprocess/session/infer/class map | ONNX mechanics and metadata parsing | Target class 1/Single and routing defaults | Provider/readiness and class policy across layers | Preserve behind adapter | Golden tensors/output, multi-class, CPU/GPU | GPU qualification | High | High |
| Model registry | `model_registry_service.*` | Model/trainer desktop flows | Package discovery/copy/hash/status/activation | Package integrity mechanics | Promotion, binary target, manual activation | Metadata/default generation overlaps trainer | Extract reusable mechanics | Package failure, save/activate/rollback | No physical hardware | High | High |
| Dataset/crops | `dataset_capture_session.*`, `collection_postprocessor.*` | Dataset Capture, collection | Sessions, crops, manifests, labels | Crop/hash/session/recovery mechanics | Hit/Waste/Mixed, auto labels, binary schema | Multiple crop/resize/write paths | Extract reusable mechanics | Golden crops, unlabeled schema, migration/recovery | Camera data fixtures | High | High |
| Persistence | live/sequence writers, `json_persistence.*` | Live, Sequence, Dataset | Image/log/summary/JSON writes | QSaveFile/retry, writer/queue patterns | Old Hit/Waste/Unknown schemas | Atomic replacement and counters duplicated | Consolidate after characterization | Power loss, ordering, queue/load, schema snapshots | Disk/load; camera timing | High | High |
| Training | `training/python/droplet_trainer/**` | Desktop QProcess; CLI/tests | Train, progress, export, metadata | Training/export and integrity diagnostics | Binary promotion, manual compute/hyperparameters | Defaults/metadata generated in Python and desktop | Preserve behind adapter | Determinism, cancel, parity, multi-class, environment | GPU optional | High | High |
| State/types | `app_state.h`, `app_types.h`, trackers | Main/controller/pipeline/writers | Cross-workspace state and contracts | Selected domain structures | Target/sort/arming/old route terminology | Multiple owners and trackers | Consolidate after characterization | State transition/event/serialization replay | Indirect hardware | High | High |
| Background/utilities | `background_task_registry.*`, `app_utils.*`, options/paths | Main/controllers | Tasks and assorted helpers | RAII task join/reap; path/time helpers | Old startup workspace option | Generic responsibility accumulation | Extract reusable mechanics | Exception/shutdown/startup tests | No direct | Medium | High |
| Legacy UI | Reports/Validator/workspaces/dialog/Settings/MainWindow | Desktop | v1 navigation and screens | Isolated model-test/run-folder/persistence helpers | Extensive superseded v1 policy | Duplicate old dialogs/workspace state | Retire as obsolete UI/policy | v2 acceptance and extracted-logic parity | Some camera/DAQ surfaces | Medium-high | High |

## 8. Legacy product-policy coupling inventory

| Coupling | Current location/evidence | v2 conflict | Disposition |
|---|---|---|---|
| Selectable detector modes/configuration | CLI mode and separate detector config models | One public detector contract and fixed qualified configuration | Keep strategies internal; no workspace selector |
| Software arming | `AppState::triggerArmed`, related UI wording | D-002 removes software arming | Retire obsolete field/UI after verification |
| Target class / Class 1 defaults | metadata resolver, registry, pipeline, trainer metadata | Hit Class is application policy; models may be multi-class | Move to Decision/Routing service |
| `sortNonTarget` | app/pipeline configuration | Conflates decision, outlet direction, and physical channel | Replace with explicit v2 contracts |
| Hit/Waste/Mixed Dataset Capture | `DatasetCaptureSession` and workspace controls | Dataset Capture must not auto-classify/auto-label | Preserve mechanics; replace schema/policy |
| Hit/Waste/Exclude dataset schema | capture/postprocess/trainer defaults | Fixed binary labeling is not v2 product policy | Version schemas and use neutral class identities |
| Promotion/validation gates | registry and Python metadata validation | Superseded validation/promotion product flow | Retain only evidenced artifact-integrity checks |
| Manual Python path/environment | Settings and dataset/trainer controller | Bundled/managed TrainingService boundary | Remove product surface after environment tests |
| Manual CPU/GPU choice | desktop and pipeline/trainer configuration | Automatic selection required | Keep qualified fallback internal |
| Editable hyperparameter JSON | desktop trainer controls/settings | Only Faster/More Accurate user choices | Centralize fixed qualified profiles |
| Manual post-save model activation | trainer/registry/UI flow | D-003 requires saved model to become active | Implement auto-activation with rollback later |
| Separate Reports/Validator navigation | legacy workspaces/MainWindow | Superseded v2 IA and Results/Model Test structure | Retire UI; extract technical logic only |
| UI text as hardware state | MainWindow/controller logic | One authoritative state owner required | Replace with typed Camera/DAQ service state |
| Hit/Waste/Unknown route fields | live/sequence types and writers | Predicted Class, Decision, Observed Route must be separate; route can be Unresolved | Version and replace domain contracts after fixtures |

## 9. Suspected dead or unreferenced code inventory

These are candidates, not deletion instructions.

| Candidate | Evidence | Classification | Recommendation | Confidence / caveat |
|---|---|---|---|---|
| `FrameGrabber` | Built, but no include/construction/call/reflection use outside own files found | Unreferenced code | Retire after characterization | High in repository; confirm no external/plugin API |
| `AppState::triggerArmed` | Declaration found; no read/write consumer | Obsolete product-policy field | Retire as obsolete UI/policy | High |
| `PipelineRunner::toLowerAscii` | Declaration/definition only | Unreferenced code | Investigate further, then remove in cleanup task | High in repository |
| `DcamController::readProps` | Declaration/definition only | Unreferenced code | Investigate further; hardware/API check before removal | High in repository |
| `DcamCamera::isOpened` | Declaration/definition only | Unreferenced code | Investigate further; public/external API check first | High in repository |
| `PipelineConfig::useCuda` | Fallback read found, no assignment found | Compatibility/fallback field | Investigate further | Medium; downstream consumers may initialize it |
| `EventDetector` | CLI `precise` construction/calls and target membership | Alternative implementation, not dead | Preserve during characterization | High |
| Old workspace modules | Constructed and dispatched by `MainWindow` | Active obsolete v1 UI/policy, not dead | Retire only after v2 replacement | High |

No file is classified as “useless.” Files compiled into a target but without in-repository calls remain subject to generated, downstream, packaging, or platform-use verification.

## 10. Structural and correctness risks

1. **Acquisition-path blocking:** DAQ firing and live file writes can execute from the camera worker record hook. A slow device or disk can delay acquisition.
2. **Multiple state owners:** App state, widget state, controller flags, pipeline state, and label text overlap for Camera, DAQ, model, operation, and routing state.
3. **Hardware ownership duplication:** CLI and desktop each own full DCAM SDK/device lifetime; settings/probe/manual paths create separate DAQ owners.
4. **Unbounded run memory:** Full-capture/log collections can grow until stop/flush. Safe duration depends on frame size/rate and is not characterized.
5. **Background task exceptions:** uncaught exceptions in registered task bodies can terminate the process; task error/results are not propagated as domain state.
6. **Inference concurrency assumption:** mutable classifier buffers are safe only while access remains serialized.
7. **Implicit class/routing contracts:** output count, metadata classes, target class, trigger decision, and physical route are not independently validated/recorded.
8. **Divergent event/crop/persistence implementations:** multiple trackers, crops, counters and atomic-write paths can produce subtly different results and recovery behavior.
9. **UI/core coupling:** raw widget/controller dependencies and `MainWindow` composition make resource locks, error recovery, and lifetime boundaries hard to test without the UI.
10. **Silent/fallback behavior:** provider and DAQ-rate fallbacks are technically useful but need typed diagnostics/provenance so v2 can distinguish automatic recovery from qualified operation.

## 11. Characterization-test backlog

| Priority | Test package | Required evidence before change |
|---|---|---|
| P0 | Detector replay corpus | Both detectors on identical raw frames/backgrounds; masks, boxes, centroids, event boundaries, direction, crops, trigger decisions and latency |
| P0 | Camera lifecycle HIL | Open/start/stop/reopen, properties, timeout/unplug, shutdown, bit depth and sustained frame fidelity |
| P0 | DAQ HIL | Command-to-waveform latency, channel/direction truth table, rate fallback, faults, cancellation, reset and acquisition continuity |
| P0 | Pipeline contract | Predicted Class vs Decision vs Observed Route, including Unresolved, multi-class and disabled/faulted DAQ |
| P1 | Persistence fault injection | QSaveFile/temp replacement, interrupted session, partial/corrupt files, retries, recovery and ordering |
| P1 | Bounded writer load | Slow disk, long runs, queue saturation, drop/block policy, memory ceiling and shutdown flush |
| P1 | Crop golden fixtures | Coordinate transform and byte/image parity across pipeline, postprocessor and dataset session |
| P1 | Model package lifecycle | Hash/schema/provider failures, duplicate versions, save-auto-activate and rollback |
| P1 | Trainer service | Managed environment verification, Faster/More Accurate profiles, cancel/crash/restart, JSONL protocol and ONNX parity |
| P1 | State ownership | Startup, locks, workspace transition, fault/recovery and stale queued events with one owner per domain |
| P2 | Legacy retirement | Clean build/startup and v2 acceptance after each old workspace/controller consumer is removed |
| P2 | Candidate dead API | Downstream/generated/plugin scan and link/package smoke test before deletion |

## 12. Recommended consolidation order

1. Freeze typed v2 domain contracts for Camera state, DAQ state/command, detector frame/event result, Model/Inference result, Decision, Observed Route, Dataset, Sequence, Run, Training operation, and application operation locks.
2. Add replay and fault fixtures around current behavior before moving ownership.
3. Introduce Camera and DAQ adapter boundaries around the existing vendor implementations; do not rewrite SDK mechanics.
4. Introduce ModelPackage/Inference and TrainingService boundaries around ONNX and Python mechanics; separate target/routing and product lifecycle policy.
5. Put both detectors behind one internal contract with Fast as provisional default; execute replay/performance/HIL characterization before choosing retirement or merge.
6. Version new neutral Dataset/Run/Sequence schemas and adapt the proven crop/writer/atomic persistence mechanics.
7. Move DAQ and disk work to characterized bounded execution paths, preserving event ordering and measured timing.
8. Establish authoritative domain owners and migrate controllers/UI to command/observe them; remove label-derived and widget-local authoritative state.
9. Extract the small reusable technical pieces from Validator, Reports, Settings, and old workspaces.
10. Retire obsolete UI/policy and confirmed unreferenced code in small, separately reviewed commits with build, fixture, and rollback evidence.

This order deliberately defers deletion until contracts and characterization evidence make equivalence and rollback observable.

## 13. Items requiring physical hardware or Windows qualification

- DCAM SDK initialization/global lifetime, device enumeration/indexing, buffer allocation/release, wait timeouts, properties, 8/16-bit conversion, frame lifetime, disconnect/reconnect, and shutdown.
- Sustained camera acquisition while detector, ONNX, preview conversion, file writing, and DAQ output are active.
- NI-DAQmx discovery, task cleanup, voltage/channel validation, sample-rate fallback, waveform shape, final-zero behavior, delays, wait completion, device faults, and physical outlet direction.
- End-to-end detector-to-physical-output latency and jitter for both directions and representative droplet rates.
- CUDA/ONNX Runtime provider qualification on the packaged Windows environment, including automatic fallback diagnostics and output parity.
- The bounded queue depth and backpressure policy needed to avoid lost or reordered frames/events under real camera and disk throughput.
- Vendor/runtime behavior cannot be inferred from the successful Release build; no camera or NI hardware was exercised during this audit.

## 14. Production files changed during the audit

**None.**

The only Phase 2 repository change is this report under `docs/v2/audits/`. No C++, header, Python, CMake, runtime configuration, test, fixture, or generated production file was edited.
