# OpenDSS v2 functional slice — production stabilization and integration

## Status

**State:** Integration release candidate; independent artifact validation pending  
**Branch:** `codex/functional-orchestrator`  
**Base:** `1e80f3b`  
**Integration owner:** FUNCTIONAL orchestrator, sole writer

The user explicitly authorized completing the recovered production stabilization
diff, final GUI/controller wiring, focused fixes, build and runtime validation,
independent read-only review, commit, and push to `renewal/main`. This instruction
supersedes the former headless-only boundary for this slice. Visual redesign,
dataset rebuilding, model-integrity weakening, and unrelated refactoring remain
out of scope.

## Active coordination ledger

| ID | Owner | State | Requirement and evidence |
|---|---|---|---|
| `UX-WINDOW-002` | User / documentation authority; FUNCTIONAL artifact owner | Authority approved; implementation begins after master/lock publication | Supersedes only the no-restore/no-fixed-minimum portion of `UX-WINDOW-001`. OpenDSS still starts maximized. Restore Down is supported; the restored window uses the existing Qt native/logical minimum-size clamp at 1600 × 900, may grow larger, and never resizes below it. No separate physical-pixel interpretation or acceptance gate is required. |
| `UAT-CONFIG-001` | FUNCTIONAL artifact owner; DESIGN protected-form owner by exact work order | Authority approved; initial-state and migration blockers resolved; implementation begins after master/lock publication | Visible shell/panel/action name is `Configuration`. Add the sole approved Detector Configuration control, `Small-droplet rejection`, whose Set rectangle maps to source pixels. Fractional mapped dimensions round to the nearest whole source pixel before integer area is applied to the existing minimum contour-area threshold. Initial value is exactly `100 px²`; legacy `-1` converts to `100 px²` and is not retained. Setup Profile stores only the current numeric threshold. It remains editable and applies immediately during active Runs, without Run/event/log/Results provenance. Preserve protected detector mechanics and use the smallest direct authoritative-state adapter. |
| `UAT-ROUTE-002` | FUNCTIONAL artifact owner; DESIGN protected-form owner by exact work order | Software PASS restored/closed at `6f2b2d7`; changed-timing HIL remains | Finalize-only decision/pulse behavior from `1f21a03` plus `pulseMutex` ordering corrections `0d7ec96`/`6f2b2d7` and deterministic both-order tests received scoped FINAL ACCEPT. Equality is Unresolved/no pulse; Waste/Stop/pause/fault no late pulse; resolved Hit exactly one callback. No additional software tests or GUI validation. |
| `VAL-TEST-UI-003` | FUNCTIONAL correction owner; VALIDATION acceptance owner | Contained by accepted central harness `3ccdb29` | The prior ambient launch path remains prohibited. `sequence_test_controller_test` may run only through the shared accepted harness/preflight. |
| `UAT-VIEWER-001` | FUNCTIONAL artifact owner; DESIGN protected-form owner | Validation accepted on integrated candidate `7fa5dd0` | Exactly five full-size consumers—Capture, Live, Sequence Test, Sequence Viewer, and Label selected crop—use one shared runtime viewer; grids/thumbnails are excluded. Sequence Viewer controls and `Ctrl`+wheel both clamp `0.3`–`10`. Durable offscreen `ShellSingleImage` passed 1/1 in 1.97 s and found one shared viewer per consumer. Unchanged geometry, pointer-centered zoom, and pan evidence was reused; no Computer Use or GUI repeat was needed. |
| `UAT-TRAIN-001` | FUNCTIONAL artifact owner | Current-machine environment gate passed; production workspace proof remains | Authoritative env-check for `%LOCALAPPDATA%\OpenDSS\training-venv-gpu` exited 0: Python 3.12.10; torch 2.10.0+cu130; torchvision 0.25.0+cu130; CUDA 13/RTX 4070; ORT-GPU 1.25.1 CUDA provider; writable output; no warnings. Evidence: `C:\Users\goals\AppData\Local\Temp\OpenDSS-validation-training-env-check`. Continue production discovery/controller/workspace proof. Fresh internet-connected Python-free-machine installer qualification remains deferred. |
| `VAL-TRAIN-UI-002` | FUNCTIONAL correction owner; VALIDATION acceptance owner | Superseded by broader `VAL-TRAIN-UI-004`; exact build root remains quarantined | Quarantine `C:\Users\goals\codex-builds\opendss-uat-train-current-host`: its `desktop_app\Release\OpenDSS.exe` (SHA prefix/suffix `C1241…835BA`) produced an Entry Point Not Found modal; Qt6Core/Gui 6.11.1 were present while Qt6Qml/Quick were absent, consistent with ambient runtime mixing. No residual process/window remains. Do not repeat the already-green authoritative Training env-check. |
| `VAL-TRAIN-UI-004` | FUNCTIONAL harness/correction owner; VALIDATION acceptance owner | Global stop narrowed by `VAL-TEST-HARNESS-001`; raw launches remain prohibited | The fatal repeat remains the reason every launch path must use the central harness. No raw PowerShell/`Start-Process` product launch is permitted. Every GUI-capable test/product verifier not yet registered remains blocked. |
| `VAL-TEST-HARNESS-001` | FUNCTIONAL harness owner; VALIDATION acceptance owner | PASS on commit `3ccdb29`; controlled reopen only | Static diff and clean-shell evidence accept the shared offscreen harness/preflight. Only its currently wired smoke path and `sequence_test_controller_test` may execute. Each additional GUI-capable test or product verifier requires registration through the same harness/preflight before lane-specific execution. This is not a blanket reopen. |
| `VAL-TEST-HARNESS-ROUTE` | FUNCTIONAL Route owner; VALIDATION acceptance owner | PASS restored at `6f2b2d7`; scoped rereview FINAL ACCEPT | Original registered pair passed cleanly; focused corrected `live_sorting_service_test` passed in 4.84 s after deterministic persistence-fault/pulse ordering coverage. No other path is authorized by this result. |
| `VAL-TEST-HARNESS-TRAIN` | FUNCTIONAL Training owner; VALIDATION acceptance owner | Execution PASS 2/2; focused controller/protocol gate closed | Registered `training_service_protocol_test` passed in 4.28 s and `training_controller_test` in 5.53 s, including self-hosted QCore fake-trainer child modes. Audit found zero relevant parent/child residual processes, windows, dialogs, or crashes. No real Python/backend/product executed. |
| `VAL-TRAIN-BACKEND-001` | FUNCTIONAL probe owner; VALIDATION acceptance owner | FINAL PASS/CLOSED on `3768e22` | Registered real `TrainingService` probe passed in 10.61 s using exact OpenDSS Python `-I -m droplet_trainer train`, CUDA, a genuine stage, and bounded cancel to `Interrupted`. Nine PNG files plus manifest were preserved; registry SHA `6E4251…3B95` and ImageNet SHA `047DCF…4E1F` remained unchanged. Audit found zero relevant residual processes, dialogs, or crashes. Environment/controller/protocol evidence was not repeated and no further current-machine Training backend execution is required. |
| `UAT-TRAIN-INSTALL-001` | FUNCTIONAL installer artifact owner; VALIDATION preflight acceptance owner | Setup/preflight/dry-run infrastructure authorized in parallel; final stamp blocked | Finish the smallest installer setup, preflight, and dry-run infrastructure while program lanes continue. Do not produce redundant large packaging or visibly launch the product. Do not stamp the final installer until the user accepts the almost-final executable. Fresh-machine qualification remains deferred. |
| `VAL-MODEL-005` | FUNCTIONAL Model Test owner; VALIDATION acceptance owner | C++ summary/writer/service FINAL PASS/CLOSED; Python/Dataset gates remain | Summary passed in 0.46 s, writer in 11.05 s, and registered service in 3 s at `0dd0bf3`, with zero residual processes, windows, dialogs, or crashes. Intentional C++ Stop kill alone bypasses the nonzero-exit guard; protocol Stop still requires normal exit. Do not repeat. `MODEL-WHEEL-LOCK-001`, then Python unit and representative-Dataset validation, are the only remaining Model Test gates. |
| `MODEL-WHEEL-LOCK-001` | FUNCTIONAL packaging-correction/deterministic-build owner; VALIDATION scoped-review owner | Conventional test relocation authorized; both candidate hashes rejected; lock/install blocked | Reject `b28f56b9…b85943` and `0783db6b…cf3d`; the latter still packaged all 13 root tests (`D:\b\odmodelwheel\run1`), so its second build was correctly skipped. Authorized writes: existing `training/python/pyproject.toml` and `training/python/droplet_trainer/__init__.py`; move exactly the 13 correction-base files matching `training/python/droplet_trainer/test_*.py` (including `test_model_test_batch_backend.py`) to same basenames under `training/python/tests/`; update only test discovery/invocation references made stale by the move. Preserve test content/import semantics. Do not move/package `publication_experiments` tests, add a custom `build_py` hook/package layer, edit trainer behavior, update lock, install, run a real Dataset, or bypass isolation. Single-source exact version `0.2.0`; explicit production-only whitelist; zero tests/experiments/caches/docs/data in wheel. Require moved source suite PASS, production module/CLI `-I` offline PASS, then two identical clean builds/member review before lock publication and atomic install. |
| `UAT-ROUTE-HIL-001` | VALIDATION HIL owner; FUNCTIONAL final-candidate owner | Sole remaining Route gate | On the final candidate, prove multi-frame Hit output remains 0 before disappearance then emits exactly one configured pulse. Equality, Waste, Stop, and pause emit 0. Finish with Stop/Clear/scalar-zero safety. Reuse unchanged DAQ waveform and Stop-zero evidence; no GUI repeat. |
| `FINAL-UAT-001` | VALIDATION consolidated GUI/HIL owner; FUNCTIONAL frozen-candidate/fix owner | Exhaustive pass authorized after final candidate freeze | In one consolidated pass, exercise every visible control, function, and button. Announce exact Computer Use actions first. Use connected hardware and Chrome oscilloscope `http://169.254.14.81/` in ROLL mode where physical trigger evidence is required. Reproduce and route exact defects to Integration; after correction, retest only the changed seam and reuse unchanged evidence. Record genuine user decisions in this ledger for return while unrelated fixes continue. Add no speculative requirement. Integration and Design must not visibly launch. |
| `SCHED-001` | FUNCTIONAL coordination owner | Active delivery order; controlled parallel execution | Retain every active worker. Continue CONFIG, ROUTE, LABEL, Training, and Model Test integration in parallel with `UAT-TRAIN-INSTALL-001` setup/preflight/dry-run work. Training backend and Model Test C++ gates are closed; do not repeat their evidence. Continue `MODEL-WHEEL-LOCK-001`; all unregistered executable paths remain blocked. VIEWER is accepted. |
| `UAT-MODEL-005` | FUNCTIONAL artifact owner | Provenance authority approved; release green backend `807625a` after master/lock publication | Visible workspace name remains `Model Test`; input is one structured/labeled OpenDSS Dataset. Internal processing uses local Python/PyTorch at `%LOCALAPPDATA%\OpenDSS\training-venv-gpu`, GPU-first with CPU fallback. Trusted package metadata requires `checkpoint_sha256`; legacy local packages missing it migrate automatically/atomically. New `opendss.model_test.v3` summaries record executed checkpoint SHA-256 plus metadata SHA-256; v2 remains readable. Missing/mismatched checkpoint or failed migration blocks execution. Live/Sequence retain ONNX+metadata provenance. No visible UI/action or user batch setting. Preserve per-image order/facts, completed-batch atomic checkpoints, Stop boundary, recovery, and allocation fallback. Reusable backend `807625a` passed 42/42 but remains unconsumed until this authority is integrated. |
| `ASSUMPTION-AUDIT-001` | User / documentation authority | User decisions resolved; implementation seams released after master/lock publication | Model Test naming/input, restored-window units, detector legacy migration/rounding, and Decision Boundary equality behavior are resolved in their owning rows above. No additional product mode, input surface, detector state, physical-pixel gate, or equality fallback may be inferred. |
| `FI-001` | FUNCTIONAL | Automated checks accepted | Production wiring covers Capture, Label, Sequence Viewer, Training, Model Library, Live, Sequence Test, Runs, and Settings. Release GUI: `C:\b\odss-v2-fpath-8435\Release\Desktop_app_v2App.exe`. |
| `CR-001` | FUNCTIONAL | Independent review accepted | `OperationCoordinator` callback invocation and destruction are serialized by a dedicated recursive mutex. The focused test runs 1,000 concurrent release/destruction iterations. |
| `FI-TEST-001` | FUNCTIONAL | Passed | Thirteen affected C++ CTest targets passed 13/13 in `C:\b\odss-v2-sequence-model-8435`; QML shell suite passed 43/43 in `C:\b\odss-v2-fpath-8435`. |
| `FI-MODEL-001` | FUNCTIONAL | Passed | The trusted installed EfficientNet model loaded locally without network fallback and the production probe processed exactly 2,103 frames with DAQ off. |
| `FI-REVIEW-001` | Reviewer | Accepted | Initial three blockers, `CR-001`, camera identity/continuous Stop, `VAL-DAQ-003`, and `VAL-GUI-003` received scoped read-only acceptance. |
| `VG-001` | VALIDATION | Candidate passed; immutable hash pending | Maximized full-area validation at 1707 × 1019 passed factual camera identity, visible frames during streaming, and the corrected DAQ arbitration presentation. Final acceptance awaits the pushed correction hash. |
| `HIL-001` | VALIDATION | Passed for corrected candidate | Real `DCAM:0` streaming preview passed. Real `Dev1/ao0` continuous 5 Vpp/10 kHz Start/Stop/exit passed with exact-zero regression evidence; the competing finite-test action is visibly disabled during continuous ownership. |
| `VAL-DAQ-003` | FUNCTIONAL / VALIDATION | Corrected, reviewed, HIL passed | Live `Send Test Sine Wave` is disabled while continuous output owns DAQ. QML regression and maximized real-HIL evidence pass. |
| `VAL-GUI-003` | FUNCTIONAL / VALIDATION | Corrected, reviewed, HIL passed | Camera acquisition remains full-rate while preview URL publication is bounded to 250 ms so asynchronous QML loads complete. Focused controller evidence and maximized real-DCAM visible-frame validation pass. |

## Delivered behavior

- Production GUI controller and context wiring replaces mock-only runtime paths.
- Camera profiles support asynchronous acceptance/readback; capture uses a global
  workflow gate and production collection/dataset roots.
- Sequence Viewer preserves native 1:1 frame dimensions.
- Training and Model Library enforce trusted local weight/checkpoint metadata and
  suppress production mock rows and mock empty/error data.
- Live scheduling, finite immediate DAQ test output, preserved delayed event
  output, continuous hardware Start/Stop, suppression, and stop-to-zero are
  separated and covered by focused fake-output tests.
- Configuration displays the factual production camera controller
  identity instead of an illustrative fallback.
- Live camera acquisition retains every latest frame while preview URL updates
  are bounded so a visible asynchronous preview remains present during streaming.
- Continuous DAQ Stop, fault, and exit teardown release operation ownership only
  after unlocking DAQ state. Focused tests prove re-entrant notification safety,
  task Stop/clear, and an exact scalar zero write for Stop and exit.
- Competing finite DAQ test output is visibly unavailable while continuous output
  owns the DAQ resource.
- Sequence Test completion/recovery, production Runs notes, Settings diagnostics,
  and native folder opening are wired.

## UAT authority-first continuation

`UAT-CONFIG-001`, `UAT-ROUTE-002`, and `UAT-VIEWER-001` are approved bounded slices. Publication of the matching `ODSS-DES-002` and consolidated lock is their dependency and does not itself modify production, QML, generated, detector, routing, viewer, or persistence code.

After publication:

1. DESIGN may receive exact protected-form ownership for Configuration, Decision Boundary, and viewer presentation only through nonoverlapping exact work orders.
2. FUNCTIONAL remains accountable for the runnable artifact and integrates the smallest direct paths to the existing detector threshold, operational routing owner, and shared full-size viewer presentation.
3. No worker may add width/height rejection, a second detector state, Run provenance for in-run threshold changes, boundary coordinate persistence/history, detector-mechanics replacement, a second reference-line feature, thumbnail zoom, or duplicated hot-path viewer JavaScript.

## Protected boundaries

- Preserve qualified DCAM, NI-DAQmx, detector/crop, ONNX, trainer, export, and
  persistence mechanics except for the bounded stabilization changes above.
- Do not rebuild datasets, introduce network fallback, accept untrusted weights,
  weaken checkpoint metadata validation, or restore production mock data.
- Do not broaden `CR-001` into a notification framework or Live service refactor.
- Integration remains the sole writer; Validation and review remain read-only.

## Next gate

Push the exact reviewed correction, hand its immutable commit hash and unchanged
executable path to Validation, and close the slice after `VG-001` records final
acceptance against that hash.
