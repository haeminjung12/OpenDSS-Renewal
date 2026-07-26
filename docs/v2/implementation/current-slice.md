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
| `FI-REVIEW-001` | Reviewer | Accepted | Initial three blockers, `CR-001`, and the exact camera-identity/continuous-Stop corrections received scoped read-only acceptance. |
| `VG-001` | VALIDATION | Pending external retry | Corrected artifact launch is stable. Repeated physical Escape interruptions prevented the camera identity and continuous Start/Stop HIL rerun before any UI/hardware action. Validate only the pushed accepted commit and exact executable; Integration alone corrects any returned defect. |
| `HIL-001` | VALIDATION | Authorized, pending | The user granted standing hardware authorization. Validation may exercise camera and DAQ after announcing exact actions, including continuous Start/Stop, stop-to-zero, finite immediate sine, delayed pulse, and arbitration. |

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
- Continuous DAQ Stop, fault, and exit teardown release operation ownership only
  after unlocking DAQ state. Focused tests prove re-entrant notification safety,
  task Stop/clear, and an exact scalar zero write for Stop and exit.
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

Commit and push the exact reviewed candidate, then hand its commit hash,
executable, automated evidence, and runtime prerequisites to Validation. Retry
the externally interrupted camera identity and continuous-DAQ HIL gates. Close
the slice only after `VG-001` accepts the artifact or Integration corrects every
exact returned defect.
