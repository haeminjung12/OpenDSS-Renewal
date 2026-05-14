# Orchestrator Memory

Date initialized: 2026-05-14

## Active Workspace

All new work starts in:

`C:\Users\goals\Codex\OpenDSS\0. Codebase`

Historical reference only:

- `C:\Users\goals\Codex\OpenDSS\1. Agent work space - archieve`
- `C:\Users\goals\Codex\CNN for Droplet Sorting`

## Current Baseline

- Clean repo contains the runtime-only migration.
- Packaging is deferred.
- Python trainer/exporter is deferred.
- No DAQ output was fired during migration.
- Safe no-DAQ smoke passed from the clean build after migration.
- Initial Git commit is still pending.

## Orchestrator Rule

Fresh Codex sessions launched in this repo should operate as the Orchestrator.

Workers are optional, user-authorized, narrow executors. Do not spawn workers automatically. Prepare work orders and minimal prompts first, then wait for separate launch confirmation.

## Current Open Items

- Make initial baseline commit when user approves.
- Sequence summary CSV is not created by the current sequence verifier path.
- Python trainer/exporter source is not migrated.
- Packaging is not migrated.
- Shell nav/header retry remains pending from the historical workspace.
- Interactive manual functionality test remained pending from the historical workspace.
