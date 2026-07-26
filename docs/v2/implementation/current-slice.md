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
| `UX-WINDOW-001` | User / documentation authority | Approved | Fixed minimum and restored-window resolution requirements are removed. All GUI, Qt Design Studio, visual, and Computer Use validation uses the maximized window's full available work area only; agents do not restore or resize to a fixed test resolution. Documentation-only authority change; no production or generated file is modified. |
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
- Hardware Configuration displays the factual production camera controller
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
