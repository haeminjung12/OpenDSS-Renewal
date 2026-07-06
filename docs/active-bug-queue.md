# Active Bug Queue

Use this as the short source of truth for runtime debugging. Do not pull old worker reports unless a bug explicitly requires historical evidence.

## Current Baseline

- Branch `main` was pushed on 2026-07-06 through commit `73fd9df`.
- That baseline includes the accepted Live UI cleanup, sequence test controls, dead overlay removal, and explicit classified-vs-went-to counter/log fields.
- Full historical context remains in `C:\Users\goals\Codex\OpenDSS\0. Codebase` and the external archive, but ordinary bug fixing should start in the sparse debug workspace.

## Priority Bugs

1. Verify and close classified-vs-went-to counters.
   - Code has been updated in `73fd9df`.
   - Run a representative recorded sequence and confirm UI counters, live CSV columns, and sequence summary fields match the intended semantics.

2. Verify whether sequence summary CSV creation is still broken.
   - `docs/current-state.md` records that the sequence verifier path did not create the summary CSV.
   - The writer schema was updated, but the creation path still needs explicit verification.

3. Manual runtime functionality pass.
   - Exercise Live, Dataset, Validator, Reports, Settings, and Model workflows.
   - Convert each failure into one scoped bug entry before editing code.

## Deferred

- Packaging and installer work.
- Python trainer/exporter migration.
- Historical worker report cleanup beyond preserving links.
- DAQ output tests unless explicitly approved by the user.
