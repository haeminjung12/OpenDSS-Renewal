# Agent Operating Rules

These rules apply to Orchestrator and worker activity in the clean OpenDSS repo.

## Startup Rule

When Codex is launched from this repo, it should treat itself as the Orchestrator.

Before planning workers, reviewing worker output, or resuming after context compaction, read:

1. `AGENTS.md`
2. `WORKSPACE_ORCHESTRATION_RULE_V2.md`
3. `docs/agent-operating-rules.md`
4. `docs/current-state.md`
5. `docs/migration-manifest.md`

If an active wave exists, also read its results tracker under `docs/agent-results/<wave-or-cleanup>/` before assigning, reviewing, or summarizing worker work.

## Orchestrator Role

The Orchestrator coordinates and reviews. It owns:

- user request interpretation;
- work-order creation;
- worker scope and dependency management;
- worker launch gating;
- report review;
- integration decisions;
- final verification and user response.

The Orchestrator must not automatically spawn workers. Worker launch requires explicit user authorization plus a separate final confirmation after task docs and minimal prompts are shown.

## Worker Assignment Documents

Create one Markdown task document for each worker assignment under:

```text
docs/agent-tasks/<wave-or-cleanup>/
```

Each task document should include:

- task title and owner role;
- minimal context and required reads;
- assigned read scope;
- assigned write scope;
- hard constraints;
- acceptance criteria;
- validation expectations;
- required completion report path;
- known blockers and dependency order.

## Minimal Worker Prompt

When a task document exists, use:

```text
You are assigned <task title>.
Read and follow: <task document path>.
Stay within the assigned scope and write the required completion report.
```

Do not paste long task briefs into worker chat unless the user explicitly asks for a full handoff prompt or the task document is missing.

## Results Files

Each worker wave or cleanup batch needs one shared results file under:

```text
docs/agent-results/<wave-or-cleanup>/
```

The results file should record:

- wave name and date;
- assigned task document paths;
- expected worker report paths;
- worker completion status;
- orchestrator review status: `pending`, `accepted`, `needs revision`, `blocked`, or `deferred`;
- consolidated blockers and decisions needed;
- final wave gate.

Workers do not edit the shared results tracker. The Orchestrator updates it after reading worker reports.

## Worker Reports

Each worker writes a Markdown completion report under:

```text
docs/worker-reports/<wave-or-cleanup>/
```

Use this report structure:

```text
# Worker Report: <Task Name>

Agent: <Agent Role>
Date: <YYYY-MM-DD>
Assigned Scope: <files/folders owned>

## Summary

Briefly state what was completed.

## Files Changed

- path/to/file - what changed

## Files Read

- path/to/file - why it was read

## Decisions Needed

- Decision or question for Orchestrator, or none

## Dependencies and Blockers

- Dependency, blocker, or none

## Acceptance Criteria Check

- [ ] Criterion 1
- [ ] Criterion 2

## Validation Performed

- Command/check performed
- Result

## Risks or Follow-Up

- Risk or follow-up item, or none
```

If no files changed, write `None` under Files Changed.

## Scope Rules

Agents must stay inside their assigned ownership area.

Do not:

- edit another worker's owned files;
- refactor broad areas outside the assigned task;
- replace active runtime behavior without a staged compatibility plan;
- treat archived code as active source;
- rename model classes or change metadata schema without Orchestrator approval;
- change packaging assumptions while packaging is deferred;
- add bundled Python or training environment assumptions.

If a task requires crossing ownership boundaries, stop and write a coordination note instead of editing.

## Active Source Boundaries

- Active runtime source: `app/runtime/`
- Active desktop app: `app/runtime/desktop_app/`
- Runtime models: `app/runtime/models/`
- Active docs: `docs/`
- Orchestration docs: `WORKSPACE_ORCHESTRATION_RULE_V2.md`, `docs/agent-operating-rules.md`, `docs/orchestrator-review/`
- Historical archive: `C:\Users\goals\Codex\OpenDSS\1. Agent work space - archieve`
- Old source/orchestration workspace: `C:\Users\goals\Codex\CNN for Droplet Sorting`

Historical locations are reference only. Do not copy from them without a scoped migration task.

## Qt Skill Rule

The active OpenDSS desktop app is Qt Widgets/C++ and CMake based, not QML and not `.ui` Designer based. Default to Qt/C++ practices and direct Qt verification unless the task explicitly names QML.

## Qt GUI Verification Rule

Qt GUI behavior must be verified with direct Qt mechanisms first:

- direct Qt object lookup and property/state assertions;
- `QTest` events;
- in-process Qt verifier harnesses;
- signal/slot observation;
- app-owned logs/state exposed by Qt objects.

Workers must not use PowerShell, UIA, coordinate clicks, screen scraping, or external click scripts as the primary GUI interaction method. PowerShell is allowed only as an RTK shell wrapper to launch the executable, configure environment variables, build/run a Qt verifier, collect logs, or inspect generated evidence.

If direct Qt verification is impossible, stop and report the blocker. Do not substitute external GUI automation without an explicit Orchestrator decision.

## Build And Smoke Rules

Use the clean repo build guidance in `docs/build.md`.

Default out-of-tree build:

```text
C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release
```

Run safe no-hardware tests only unless the user explicitly approves DAQ output.

Do not run packaging work unless packaging is explicitly brought back into scope.

## Orchestrator Review Expectations

The Orchestrator reviews and confirms:

- scope compliance;
- file ownership compliance;
- acceptance criteria;
- validation evidence;
- whether completed worker reports are recorded in the wave results file;
- whether decisions need to be added to `docs/decisions.md`;
- whether follow-up tasks should be split out.

The Orchestrator may reject or send back work that:

- crosses ownership boundaries without approval;
- lacks a completion report;
- changes architecture without a decision note;
- overclaims capabilities;
- changes packaging/trainer assumptions while those areas are deferred;
- leaves outputs disorganized.
