# OpenDSS Agent State

State format: `1`

- Policy revision: `ODSS-2026-07-30.2`
- Mode: `debug`
- User-facing Lead: `Debug Lead`
- Checkpoint branch: `codex/debug-lead`
- Checkpoint commit: `94067b759e631e4e965ba433ad9cf8e497dec9d0`
- Active ID: `DBG-001`
- Status: `DBG-001 and DBG-002 integrated; Release build passed and 51/53 tests pass; closure held on unrelated DBG-010 and DBG-011 gate blockers`
- Dirty paths at checkpoint: `app/runtime/tests/CMakeLists.txt`, `app/runtime/tests/model_load_activation_test.cpp`, `app/runtime/tests/model_test_controller_test.cpp`, `app/runtime/tests/model_test_service_test.cpp`, `app/runtime/tests/runs_results_controller_test.cpp`, `docs/agent-state/current.md`, `docs/debug/bug-ledger.md`
- Updated: `2026-07-30T20:09:26-05:00`

## Accepted decisions

- The Debug Lead is the sole user-facing agent for OpenDSS debugging.
- Workers and validators report only to the Debug Lead and do not edit canonical state or the bug ledger.
- Normally use no more than two internal workers and one validator concurrently.
- Every accepted bug receives one stable `DBG-*` ID before production changes.
- Fresh-agent handoff is preferred over repeated compaction; this file and verified Git evidence are authoritative.
- Verification uses coherent integration batches, avoids unchanged reruns, and reserves full builds for the integrated batch unless a named early-build risk applies.
- A fresh read-only Plan Guardian checks canonical plan fidelity at integration, closure, merge, and material-scope-change gates.
- `DBG-001` and `DBG-002` form the first integration batch; run the full build and required tests only after both fixes are complete, while allowing narrow reproduction checks needed to resolve named uncertainties.
- Shared policy `G-2026-07-30.3` and delegation rule `G-MODEL-001` are adopted for all remaining work.
- User-authoritative detector invariant: every acquired image must pass through the event detector in order before first/last occurrence, trajectory, crop, or ONNX-routing decisions. Sampled preview delivery is not valid detector input; optional sequence persistence is a separate concern.
- The first batch will use a current-code headless characterization harness, informed by the proven legacy `sequence_headless` pipeline, to measure acquisition, detector, and persistence throughput independently; it must not become a parallel production implementation.
- `DBG-010` and `DBG-011` were discovered by the combined Release gate and are not silently added to the `DBG-001`/`DBG-002` production-fix scope.

## Accepted evidence

- Debug authority and safety rules: `AGENTS.md`.
- Canonical bug records: `docs/debug/bug-ledger.md`.
- Product authority and protected assets: `docs/v2/CONTEXT.md` and the protected-asset section of `AGENTS.md`.

## Verification

- Debug worktree contains the ordered unread-frame acquisition fix at commit `eda6c768795aecdf258ccd34290eac7e703928a2`.
- Branch is `codex/debug-lead`.
- A fresh Codex session loaded `G-2026-07-30.2`, `ODSS-2026-07-30.2`, `Debug Lead`, `DBG-INTAKE`, `G-VERIFY-001`, `O-WORKER-BUILD-001`, `O-GATE-001`, and `$opendss-plan-guardian`; `$opendss-debug-init` reported ready.
- Bug intake evidence was reviewed from `E:\OpenDSS\OpenDSS-problem-summary.md`, `C:\Users\goals\Downloads\DataCapture`, and `C:\Users\goals\Downloads\New folder`.
- Nine stable bugs were triaged as `DBG-001` through `DBG-009`; the first integration batch is awaiting its single full build/test gate.
- `DBG-001` evidence reconciles 1,087 retained TIFF frames and 16 crop records. Approximately 90.5% of raw camera delivery IDs are absent before the dataset writer; its queue rejection and consumer failure counters are zero.
- `DBG-002` is reproduced: Image Sequence queue rejection is logged and counted, but the rejected offer returns success and a nonempty sequence can still persist `status: completed`.
- A fresh read-only Plan Guardian passed the protected camera-boundary investigation. It confirmed `CameraController::acceptFrame` emits `frameReady` before preview coalescing and that Dataset Capture processes each received frame, so the next boundary is how acquisition frames reach `CameraController`.
- `DBG-001` root cause is accepted: a 16 ms `CameraService` timer requests only DCAM's newest frame, so a roughly 662 fps source is sampled at roughly 62.5 fps. The observed 9.45% delivery ratio matches both datasets; the omitted images never reach any detector consumer.
- Vendor HCImageLive evidence establishes 2304×2304, 8-bit, Fast, 1.0 ms exposure at 63.33 fps as the `DBG-002` equivalent-settings attempted-throughput baseline.
- The `DBG-001` headless Release characterization fails before production modification: 259/270 full frames and 257/2,800 ROI frames reached the detector, matching the accepted newest-only polling defect.
- After the fix, Release service-path characterization passes exact acquired/service/detector counts: 270/270/270 at 2304×2304 and 2,800/2,800/2,800 at 1152×288, with zero gaps, ordering errors, pixel-ID errors, or pre-service coalescing.
- Physical USB HIL drained and detector-completed 3,554/3,554 ROI frames at 710.858 fps and 316/316 full frames at 63.309 fps with zero gaps, duplicates, ordering errors, or DCAM overrun. Average full-frame detector service time was 7.804 ms, mathematically supporting 128.140 fps versus the 89.1 fps CoaXPress specification and 100 fps headroom target.
- The isolated `DBG-002` fix passes its focused Release build and regression test; it reports queue rejection immediately and finalizes the affected sequence as Failed with factual attempted/saved/rejected counts and rates.
- `DBG-002` is integrated at commit `c17e72e393116f05430644c293450cba1921ea74`.
- Integration build repairs are committed at `ebf1a0e3a59058bb8baa5f990f2a93a981760ffd` (preserve the camera CLI and DAQ test seams) and `94067b759e631e4e965ba433ad9cf8e497dec9d0` (propagate the NI-DAQmx runtime link).
- The previously started combined build was stopped at the user's request before storage consolidation; no completed full-build or full-test result is accepted from that attempt.
- A fresh combined Plan Guardian returned `PASS` against `eda6c76`, `c17e72e`, `ebf1a0e`, and `94067b7`.
- The integrated Release build at `C:\b\odss-debug-lead-hil` passed, including `OpenDSS.exe`, camera/DCAM, sequence, detector, ONNX, and DAQ-linked targets.
- The first full CTest run passed 44/53 tests and exposed nine failures unrelated to the `DBG-001`/`DBG-002` commits. Narrow test-contract repairs and restoration of ignored model assets from exact locked hashes resolved seven failures.
- The final full CTest run passed 51/53 tests in 128.39 seconds. All camera, detector, crop, dataset capture, sequence capture/reporting, Live, Sequence, DAQ, ONNX conversion, Model Library, Training, and repaired Results tests passed.
- Qualified ignored assets were restored locally with exact metadata hashes: pretrained EfficientNet checkpoint `242ED863...1C5`, pretrained MobileNet checkpoint `0542C495...E35`, ImageNet EfficientNet weights `7F5810BC...4934`, and ImageNet MobileNet weights `047DCFF4...F4E1F`.
- Canonical Python-backed tests were characterized using reversible junctions from `%LOCALAPPDATA%\OpenDSS\python-3.12.10` and `training-venv-gpu` to their preserved `validation-backup-20260729T130643Z-cc618378` directories. These junctions remain present because automated removal was policy-blocked; they are evidence scaffolding, not an installer fix.

## Blockers

- Live droplet recall and GUI/persistence throughput remain unverified; the short HIL scene contained no detected droplets. CoaXPress capacity is a user-authorized mathematical check, not a physical CoaXPress HIL run.
- Installer local fixes have not yet been reconciled with this worktree or a rebuilt installer.
- `DBG-010`: the real Model Test Python handshake rejects the locked pretrained checkpoint because it lacks the top-level `class_ids` required by the current backend; this is a protected model/inference boundary and needs separately authorized correction.
- `DBG-011`: the representative production `dataset.json` hash is `58500D72...9233B`, not the audited `E6BCEA0F...F1C72`; provenance must be restored or explicitly re-audited before changing the expected hash.

## Exact next action

Run a fresh Plan Guardian against the post-gate test-only diff and new bug records. Do not close `DBG-001`/`DBG-002` or alter protected Model Test behavior until the user chooses whether to authorize `DBG-010` and how to resolve `DBG-011` provenance.
