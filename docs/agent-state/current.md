# OpenDSS Agent State

State format: `1`

- Policy revision: `ODSS-2026-07-30.1`
- Mode: `debug`
- User-facing Lead: `Debug Lead`
- Checkpoint branch: `codex/debug-lead`
- Checkpoint commit: `5ce3a838446adfefac8180d611069741fe54eff5`
- Active ID: `DBG-INTAKE`
- Status: `Ready for bug intake`
- Dirty paths at checkpoint: `docs/agent-state/current.md`
- Updated: `2026-07-30T15:50:00-05:00`

## Accepted decisions

- The Debug Lead is the sole user-facing agent for OpenDSS debugging.
- Workers and validators report only to the Debug Lead and do not edit canonical state or the bug ledger.
- Normally use no more than two internal workers and one validator concurrently.
- Every accepted bug receives one stable `DBG-*` ID before production changes.
- Fresh-agent handoff is preferred over repeated compaction; this file and verified Git evidence are authoritative.

## Accepted evidence

- Debug authority and safety rules: `AGENTS.md`.
- Canonical bug records: `docs/debug/bug-ledger.md`.
- Product authority and protected assets: `docs/v2/CONTEXT.md` and the protected-asset section of `AGENTS.md`.

## Verification

- Debug worktree was created from OpenDSS policy commit `5ce3a838446adfefac8180d611069741fe54eff5`.
- Branch is `codex/debug-lead`.
- No production files have been changed.

## Blockers

- The user’s bug descriptions and reproduction details have not yet been entered into the bug ledger.

## Exact next action

Receive the bug list, assign stable IDs, triage by impact and independence, reproduce the highest-priority bug, and spawn up to two internal workers only for independent investigations.
