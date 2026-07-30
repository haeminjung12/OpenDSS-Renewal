# OpenDSS Agent State

State format: `1`

- Policy revision: `ODSS-2026-07-30.2`
- Mode: `debug`
- User-facing Lead: `Debug Lead`
- Checkpoint branch: `codex/debug-lead`
- Checkpoint commit: `eda6c768795aecdf258ccd34290eac7e703928a2`
- Active ID: `DBG-001`
- Status: `DBG-001 committed and HIL verified; DBG-002 focused fix verified and integration pending`
- Dirty paths at checkpoint: `app/runtime/v2/sequence/image_sequence_capture_service.{h,cpp}`, `app/runtime/tests/image_sequence_capture_service_test.cpp`, `docs/agent-state/current.md`, `docs/debug/bug-ledger.md`
- Updated: `2026-07-30T18:41:45-05:00`

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

## Blockers

- Live droplet recall and GUI/persistence throughput remain unverified; the short HIL scene contained no detected droplets. CoaXPress capacity is a user-authorized mathematical check, not a physical CoaXPress HIL run.
- Installer local fixes have not yet been reconciled with this worktree or a rebuilt installer.

## Exact next action

Pass the corrected combined Plan Guardian gate, commit the verified `DBG-002` three-file fix, and perform the single full Release build/test gate for the first integration batch.
