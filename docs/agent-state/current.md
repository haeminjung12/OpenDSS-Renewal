# OpenDSS Agent State

State format: `1`

- Policy revision: `ODSS-2026-07-30.2`
- Mode: `implementation`
- User-facing Lead: `Implementation Lead`
- Checkpoint branch: `codex/project-policy-reset`
- Checkpoint commit: `cb101cb87c23e1718ea4e97316e052091e194bee`
- Active ID: `P0-1`
- Status: `Planned; implementation is not authorized`
- Dirty paths at checkpoint: `AGENTS.md`; `docs/agent-state/current.md`; `docs/debug/bug-ledger.md`
- Updated: `2026-07-30T16:23:00-05:00`

## Accepted decisions

- P0-1 Detector Characterization and Neutral Contract is the next implementation slice.
- Current desktop/default selection remains `FastEventDetector`.
- Current CLI precise selection remains `EventDetector`.
- No detector algorithm, threshold, routing, DAQ, persistence, camera ownership, or QML change is authorized.
- Fresh-agent handoff is preferred over repeated compaction; this file is authoritative over a compacted transcript.
- Verification uses coherent integration batches, avoids unchanged reruns, and reserves full builds for the integrated batch unless a named early-build risk applies.
- A fresh read-only Plan Guardian checks canonical plan fidelity at integration, closure, merge, and material-scope-change gates.

## Accepted evidence

- Product authority and reading order: `docs/v2/CONTEXT.md`.
- Slice scope and acceptance: `docs/v2/implementation/current-slice.md`.
- Reuse findings: `docs/v2/audits/OpenDSS_v2_Reusable_Core_Audit.md`, F-A01 and F-A02.

## Verification

- Policy maintenance completed on `codex/project-policy-reset`; the checkpoint commit above records the pre-policy base.
- All approved orchestration, continuity, verification-scheduling, plan-fidelity, and debug rule IDs are present.
- Both initializer skills pass schema validation.
- Implementation resume passes; debug resume rejects implementation mode and passes a clean debug-mode fixture.
- Portable policy-kit `0.3.1` passes clean-checkout isolated install, hook, drift, migration, update, rollback, and GitHub validation.
- A fresh Codex session loaded `G-2026-07-30.2`, `ODSS-2026-07-30.2`, `Implementation Lead`, `G-VERIFY-001`, `O-BATCH-001`, `O-RERUN-001`, `O-GATE-001`, and `$opendss-plan-guardian`.
- Production v2 implementation has not started.

## Blockers

- Explicit user authorization is required before P0-1 implementation.
- Representative real-sequence replay and hardware qualification evidence remain unavailable for detector retirement.

## Exact next action

Wait for explicit implementation authorization. Once authorized, characterize both current detectors with deterministic tests and introduce only the smallest neutral boundary required by current consumers.
