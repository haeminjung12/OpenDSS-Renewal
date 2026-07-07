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

## Trainer Migration Wave

The clean repo has an approved Python trainer migration boundary for a CPU-focused wave:

- `training/python/droplet_trainer/`
- `training/python/pyproject.toml`
- `training/python/README-windows-training.md`
- `training/python/requirements/`
- `training/python/scripts/windows/`

This migration does not include local datasets, crops, venvs, `best_runs`, or generated `*.onnx`, `*.pth`, and `*.mat` outputs. GPU execution validation is a separate follow-up wave, and packaging remains deferred.

## Not Yet Migrated

- Legacy exporter code, trainer dependencies installed into local venvs, and trainer validation evidence remain outside the clean repo boundary.
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
