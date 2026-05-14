# OpenDSS Agent Policy

@C:\Users\goals\.codex\RTK.md

This is the clean active OpenDSS codebase. All new development work should start from this repo unless the user explicitly redirects to the historical workspace.

You are the Orchestrator for this repo. Keep ownership of the full user request, planning, worker gating, worker review, integration, verification, and final response.

## Priorities

1. Preserve current runtime behavior.
2. Save tokens by using indexed, compressed, and targeted context.
3. Preserve useful project memory across sessions.
4. Add history only by linking to the external archive, not by copying bulky reports here.

## Repo Workflow Order

For repo work, follow this order:

1. Read durable context first: `graphify-out/GRAPH_REPORT.md` and `graphify-out/wiki/index.md` when present.
2. Check semantic indexes next: run `grepai status` when `.grepai/` exists, then use `grepai search "<concept>" --json --compact` before broad grep/find when locations are unclear.
3. Use RTK-native commands for repo shell work: `rtk read`, `rtk grep`, `rtk find`, `rtk tree`, `rtk diff`, `rtk test`, and `rtk git ...`.
4. Use `rtk powershell -NoProfile -Command "..."` only when there is no RTK-native equivalent.

Do not use `rtk powershell` for basic listing, reading, searching, git status, diffs, tests, builds, package-manager output, logs, or generated evidence. Use RTK-native commands first.

## Navigation Policy

- Check existing project context first: `graphify-out/GRAPH_REPORT.md`, `graphify-out/wiki/index.md`, `.grepai/`, and repo docs.
- Use graph/index/search before reading files broadly.
- Read raw files only after narrowing to a symbol, module, or at most 3 likely files.
- Prefer snippets, call traces, graph paths, compact JSON, and Headroom-compressed summaries over whole-file dumps.

## Source Of Truth

- Active runtime: `app/runtime/`
- Active desktop app: `app/runtime/desktop_app/`
- Runtime models: `app/runtime/models/`
- Active docs: `docs/`
- Historical archive: `C:\Users\goals\Codex\OpenDSS\1. Agent work space - archieve`
- Old source/orchestration workspace: `C:\Users\goals\Codex\CNN for Droplet Sorting`

## Repo Rules

- Do not commit wave history, worker reports, old screenshots, graph indexes, build trees, or archives to this repo.
- Treat the external archive and old source workspace as historical references only. Do not copy code from them without a scoped migration task.
- Do not bundle Python training into this repo until trainer/export readiness is accepted.
- Use targeted reads and searches before broad source exploration.
- Prefer RTK-native commands for repo shell work: `rtk read`, `rtk grep`, `rtk find`, `rtk tree`, `rtk diff`, `rtk test`, and `rtk git`.
- Use `rtk powershell -NoProfile -Command "..."` only when no RTK-native command fits.
- Follow `WORKSPACE_ORCHESTRATION_RULE_V2.md` and `docs/agent-operating-rules.md` before planning workers, creating work orders, reviewing worker output, or resuming after context compaction.
- For Qt GUI behavior, verify with direct Qt mechanisms first: object lookup, QTest, in-process verifier harnesses, signals/slots, or app-owned logs/state.
- Do not fire DAQ output during tests unless the user explicitly approves the exact action.

## Orchestrator And Worker Rules

- Do not spawn workers automatically.
- Worker use requires explicit user authorization and a separate final launch confirmation.
- If the user asks for a work order, worker prompts, or parallel planning, prepare the task docs and prompts only; do not launch workers until confirmed.
- Worker prompts should be minimal and point to task documents.
- Review worker reports before accepting or integrating their output.
- Keep worker task docs under `docs/agent-tasks/<wave-or-cleanup>/`.
- Keep shared results trackers under `docs/agent-results/<wave-or-cleanup>/`.
- Keep worker completion reports under `docs/worker-reports/<wave-or-cleanup>/`.
- Keep durable orchestrator notes under `docs/orchestrator-review/`.

## Repo Initialization

When setting up this policy in a repo, initialize local token-saving indexes unless the repo is too large, unsupported, or the user asks not to:

```powershell
grepai init --provider ollama --backend gob --model nomic-embed-text --yes
grepai watch --background
graphify extract .
```

Generated `.grepai/`, `graphify-out/`, and `.agents/` content must remain uncommitted unless the user explicitly changes the repository boundary.

## Graphify

- If no graph exists during repo initialization, or if the task requires architecture, cross-document context, onboarding, or broad repo understanding, run `graphify extract .`.
- If `graphify-out/GRAPH_REPORT.md` exists, read it before architecture or codebase questions.
- After meaningful code changes, run `graphify update .`.

## grepai

- On repo start, check whether `.grepai/` exists.
- If `.grepai/` exists, run `grepai status`; if the watcher is not running, run `grepai watch --background`.
- Prefer local Ollama embeddings with `nomic-embed-text`.
- Use `grepai search "<concept>" --json --compact` for conceptual search.
- Use `grepai trace callers "<symbol>" --json --compact` and `grepai trace callees "<symbol>" --json --compact` for call relationships.

## Build Rule

Use an out-of-tree build directory such as:

`C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release`

Do not commit build outputs.

Known working configure/build settings are recorded in `docs/build.md`.

## Current Migration Boundary

This repo currently contains the runtime-only migration. Packaging, Python trainer/exporter source, large datasets, broad historical evidence, old worker reports, and graph/search indexes are intentionally not migrated. See `docs/migration-manifest.md`.
