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
| `UX-WINDOW-002` | User / documentation authority; FUNCTIONAL artifact owner | Authority approved; implementation begins after master/lock publication | Supersedes only the no-restore/no-fixed-minimum portion of `UX-WINDOW-001`. OpenDSS still starts maximized. Restore Down is supported; the restored application window enforces an exact minimum of 1600 × 900 logical px, may grow larger, and never resizes below it. Validation may test maximized state and restored state at exactly 1600 × 900 or larger, never below. |
| `UAT-CONFIG-001` | FUNCTIONAL artifact owner; DESIGN protected-form owner by exact work order | Authority approved; implementation begins only after master/lock publication | Visible shell/panel/action name is `Configuration`. Add the sole approved Detector Configuration control, `Small-droplet rejection`, whose Set rectangle maps to source pixels and applies its area to the existing minimum contour-area threshold. The current threshold alone is saved in Setup Profile. It remains editable and applies immediately during active Runs, without Run/event/log/Results provenance. Preserve protected detector mechanics and use the smallest direct authoritative-state adapter. |
| `UAT-ROUTE-002` | FUNCTIONAL artifact owner; DESIGN protected-form owner by exact work order | Authority approved; implementation begins only after master/lock publication | `Decision Boundary` is operational. Explicit Set arms one source-image X/Y click; ordinary clicks are inert; the horizontal segment runs from that point to the RIGHT EDGE only. Top/Bottom selects Hit side, Reset clears, and Start is blocked while unset. Live and Sequence Test own independent state. Coordinates remain workspace-local and never enter profiles, Runs, events, logs, Results, exports, or history. |
| `UAT-VIEWER-001` | FUNCTIONAL artifact owner; DESIGN protected-form owner only if exact work order requires it | Authority approved; implementation begins only after master/lock publication | Shared full-size image viewers use cursor-centered `Ctrl`+wheel zoom from fit-relative `1.0`, clamped `0.3`–`10`, with ordinary scrolling/panning. Grids and thumbnails are excluded. Navigation is presentation-only and cannot change source coordinates, artifacts, detector values, or operational geometry. Reuse one production component only when materially simplest; do not duplicate hot-path JavaScript. |
| `UAT-TRAIN-001` | FUNCTIONAL artifact owner | Authority approved; implementation continues after master/lock publication | Replace embedded/offline Python payload expectations with a signed internet bootstrap: exact CPython `3.12.10`, the authoritative 37-wheel hash-locked environment, full hash verification, `%LOCALAPPDATA%\OpenDSS\training-venv-gpu` provisioning, authoritative environment check, and visible atomic failure. Acceptance uses fresh internet-connected Python-free Windows. Training after installation remains local with no runtime network fallback; trained-model runtime requirements do not change. |
| `SCHED-001` | FUNCTIONAL coordination owner | Active delivery order | Retain every active worker. Make the current-machine Training workspace functional ASAP using exactly `%LOCALAPPDATA%\OpenDSS\training-venv-gpu`. Defer only external fresh-machine installer qualification; do not defer current-machine runtime integration or bounded installer implementation. CONFIG, ROUTE, VIEWER, and LABEL remain active. |
| `UAT-MODEL-005` | FUNCTIONAL artifact owner | Corrected authority approved; highest-priority parallel backend lane after corrected master/lock publication | Training and Dataset Validation/Model Test execute through local Python/PyTorch at `%LOCALAPPDATA%\OpenDSS\training-venv-gpu`, automatic GPU-first with CPU fallback; C++ ONNX batching authority is revoked. Live and Sequence real-time sorting retain ONNX Runtime and default CPU. Existing Model Test semantics, artifacts, hashes, atomic partial persistence, and recovery remain unchanged. Automatically select the largest qualified memory-safe Python batch; expose no user batch setting. Preserve per-image order/facts; count progress only after an atomic completed-batch checkpoint; Stop starts no later batch; reduce batch size on pre-inference allocation failure. |
| `UAT-CONFIG-BLK-002` | User decision | Blocked only for initial Small-droplet rejection state | Protected `FastEventConfig::minArea == -1.0` means automatic, but the current master expects a concrete displayed/saved source-pixel area. Authority must choose either an exact initial pixel-area value or an `Automatic` presentation and exact concrete-resolution rule. Do not infer a value or create a second detector state. Other CONFIG work and all other lanes continue. |
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
