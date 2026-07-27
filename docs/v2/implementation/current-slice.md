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
| `UAT-ROUTE-002` | FUNCTIONAL artifact owner; DESIGN protected-form owner by exact work order | Authority approved; current implementation acceptance retracted pending track-end correction | `Decision Boundary` is operational. Explicit Set arms one source-image X/Y click; ordinary clicks are inert; the horizontal segment runs from that point to the RIGHT EDGE only. Top/Bottom selects Hit side, Reset clears, and Start is blocked while unset. All in-frame positions are tracking-only. Classification occurs only when the track ends/disappears, using final Y against boundary Y. Clicked X is not an operational threshold. Exact final-Y equality is `Unresolved`, is not forced to Hit/Waste, and emits no Hit pulse. Independent state and zero coordinate persistence remain. |
| `VAL-TEST-UI-003` | FUNCTIONAL correction owner; VALIDATION acceptance owner | Blocks GUI-capable Route tests only | User-visible launch came from FUNCTIONAL Route build/test chain `cmake` PID 58944 → `C:\b\odroute002\desktop_app\tests\Release\sequence_test_controller_test.exe` PID 66088 with ambient PATH and no offscreen setting; the chain exited. Validation did not launch it. Identify the owning worker/root cause and suspend GUI-capable Route tests until durable offscreen environment configuration and clean zero-window proof are integrated. Other Route implementation and non-GUI tests may continue. |
| `UAT-VIEWER-001` | FUNCTIONAL artifact owner; DESIGN protected-form owner | Validation accepted on integrated candidate `7fa5dd0` | Exactly five full-size consumers—Capture, Live, Sequence Test, Sequence Viewer, and Label selected crop—use one shared runtime viewer; grids/thumbnails are excluded. Sequence Viewer controls and `Ctrl`+wheel both clamp `0.3`–`10`. Durable offscreen `ShellSingleImage` passed 1/1 in 1.97 s and found one shared viewer per consumer. Unchanged geometry, pointer-centered zoom, and pan evidence was reused; no Computer Use or GUI repeat was needed. |
| `UAT-TRAIN-001` | FUNCTIONAL artifact owner | Current-machine environment gate passed; production workspace proof remains | Authoritative env-check for `%LOCALAPPDATA%\OpenDSS\training-venv-gpu` exited 0: Python 3.12.10; torch 2.10.0+cu130; torchvision 0.25.0+cu130; CUDA 13/RTX 4070; ORT-GPU 1.25.1 CUDA provider; writable output; no warnings. Evidence: `C:\Users\goals\AppData\Local\Temp\OpenDSS-validation-training-env-check`. Continue production discovery/controller/workspace proof. Fresh internet-connected Python-free-machine installer qualification remains deferred. |
| `VAL-TRAIN-UI-002` | FUNCTIONAL correction owner; VALIDATION acceptance owner | Superseded by broader `VAL-TRAIN-UI-004`; exact build root remains quarantined | Quarantine `C:\Users\goals\codex-builds\opendss-uat-train-current-host`: its `desktop_app\Release\OpenDSS.exe` (SHA prefix/suffix `C1241…835BA`) produced an Entry Point Not Found modal; Qt6Core/Gui 6.11.1 were present while Qt6Qml/Quick were absent, consistent with ambient runtime mixing. No residual process/window remains. Do not repeat the already-green authoritative Training env-check. |
| `VAL-TRAIN-UI-004` | FUNCTIONAL harness/correction owner; VALIDATION acceptance owner | FATAL global executable-launch stop | Integration Training smoke launched quarantined `OpenDSS.exe` PID 35664 from PowerShell PID 66124 with `QT_QPA_PLATFORM=minimal` and produced a visible no-platform-plugin dialog. Validation terminated both; no Validation launch occurred. Suspend every OpenDSS executable and GUI-capable test launch across all workers. Centralize one harness that preflights the actual `qoffscreen` plugin before launch, uses a sanitized isolated runtime, and proves zero HWNDs/dialogs/crashes/residual processes. Only after that proof may backend or workspace executable validation resume. Source edits, builds, static checks, and genuinely non-launching tests may continue. |
| `SCHED-001` | FUNCTIONAL coordination owner | Active delivery order; executable launches globally suspended | Retain every active worker. Continue source integration and non-launching checks for CONFIG, ROUTE, LABEL, Training, and Model Test. No OpenDSS executable or GUI-capable test may launch until `VAL-TRAIN-UI-004` passes. The current-machine Training environment remains qualified; defer only external fresh-machine installer qualification. VIEWER is accepted. |
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
