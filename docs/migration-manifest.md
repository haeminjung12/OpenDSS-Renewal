# Migration Manifest

Source repo:

`C:\Users\goals\Codex\CNN for Droplet Sorting`

Clean repo:

`C:\Users\goals\Codex\OpenDSS\0. Codebase`

External archive:

`C:\Users\goals\Codex\OpenDSS\1. Agent work space - archieve`

## Copied Into Clean Repo

- `internal-release/app/runtime/` -> `app/runtime/`
- Orchestrator -> worker workflow rules were migrated and adapted for the clean repo:
  - `WORKSPACE_ORCHESTRATION_RULE_V2.md`
  - `docs/agent-operating-rules.md`
  - `docs/orchestrator-review/orchestrator-memory.md`

Excluded from that copy:

- `.qtcreator/`
- `daq_test_cli.cpp`
- `CONTEXT_SUMMARY.md`
- `README_INTERNAL.md`

## Deferred

- Python trainer/exporter source and training environment files.
- Packaging execution and packaged release artifacts.
- Broad historical evidence.
- Shell nav/header retry work.
- Interactive manual functionality test completion.
- Sequence summary CSV fix.
- Any future cleanup of old runtime code paths not needed by the current build.

## Not Yet Migrated

- Python training code, exporter code, trainer dependencies, and trainer validation evidence.
- Packaging scripts, package-check scripts, installer/release bundles, and portable package output.
- Raw datasets, training datasets, stream test datasets, generated crop evidence, and large evidence logs.
- Historical worker reports, wave tracking docs, orchestration memory, prompts, and task folders.
- Graphify output, grepai indexes, build trees, IDE folders, and cache/generated files.
- Broad old `app/`, `analysis/`, `poster/`, `datasets/`, and archive trees from the source workspace.
- Old one-off test CLIs and internal notes excluded from the runtime copy: `.qtcreator/`, `daq_test_cli.cpp`, `CONTEXT_SUMMARY.md`, and `README_INTERNAL.md`.

## Historical Reference Locations

- Full old workspace: `C:\Users\goals\Codex\CNN for Droplet Sorting`
- External agent/archive folder: `C:\Users\goals\Codex\OpenDSS\1. Agent work space - archieve`

Use those locations for traceability only. New code work should happen in `C:\Users\goals\Codex\OpenDSS\0. Codebase`.
