# Workspace Orchestrator Rule

This file preserves the Orchestrator -> worker workflow for the clean OpenDSS repo.

## Core Rule

There is one Orchestrator.

The Orchestrator does not automatically spawn workers.

Workers are only used when the user explicitly asks for worker/orchestrator style work, asks to delegate, asks for parallel agents, or directly authorizes worker use.

If the user has not explicitly asked for workers, the Orchestrator works normally and does the task itself.

## Orchestrator Ownership

The Orchestrator owns:

- the full user request;
- the working plan;
- deciding what is actually needed;
- keeping the repo source of truth straight;
- assigning worker tasks only after user authorization;
- integrating worker results;
- checking worker output;
- final verification;
- final response to the user.

The Orchestrator must never give overall control of the job to a worker.

## Worker Launch Rule

Do not spawn workers on your own.

Worker launch requires explicit final confirmation from the user immediately before launch.

A user asking for workers, asking for parallel agents, asking for an orchestrator/worker setup, or approving worker use is permission to plan worker use and prepare work orders. It is not permission to actually launch workers yet.

Only launch workers when all of these are true:

- the user explicitly asks for worker use, delegation, an orchestrator/worker setup, or parallel agents;
- the Orchestrator has created or updated the required task documents and results tracker;
- the Orchestrator shows the worker list or minimal worker prompts to the user;
- the user gives a separate final confirmation to launch those workers.

Do not treat a large task, complicated task, or multi-file task as permission to spawn workers.

If the user says "get workers ready," "prepare worker prompts," "plan parallel work," or similar, prepare the docs and prompts only. Then ask for explicit launch confirmation.

## Minimal Worker Prompt Rule

Worker prompts must be minimal.

When a task document exists, the worker prompt should normally be only:

```text
You are assigned <task title>.
Read and follow: <task document path>.
Stay within the assigned scope and write the required completion report.
```

Do not restate the task document, acceptance criteria, constraints, or background in chat unless the user explicitly asks for a full handoff prompt or the task document is missing.

If no task document exists, use this fallback prompt shape:

```text
You are a worker in this repo. Do only this task:

<one specific task>

Scope:
- Edit only: <files/folders/area>
- Do not touch: <files/folders/area>

Return only:
- files changed
- what changed
- checks run
- blockers
```

## Worker Model Selection Rule

Because this repo prioritizes token savings, each proposed worker list should include a recommended model tier outside the minimal prompt unless the launch tool requires it.

- `GPT-5.3-Codex-Spark`: very narrow, low-risk coding or revision tasks with small expected diffs.
- `GPT-5.4-Mini`: narrow docs, small revisions, focused verification, report-only tasks.
- `gpt-5.4`: normal bounded implementation, focused Qt/C++ edits, setup docs, focused refactors.
- `GPT-5.5`: high-risk Qt/C++ architecture, threading, lifecycle/shutdown, hardware-sensitive work, final integration review.

Do not use cheaper tiers for broad architecture, multi-file lifecycle/threading changes, hardware-sensitive work, or final integration review.

## Work Order Documentation Rule

When the user asks for work orders, worker planning, parallel work, or the next round of worker tasks, update repo documents before giving worker prompts.

For every wave, cleanup batch, or parallel worker set, create or update:

- worker task documents under `docs/agent-tasks/<wave-or-cleanup>/`;
- one shared results tracker under `docs/agent-results/<wave-or-cleanup>/<wave-or-cleanup>-results.md`;
- durable orchestration notes under `docs/orchestrator-review/orchestrator-memory.md`.

Each worker task document must include:

- task title and owner role;
- required reads;
- assigned read scope;
- assigned write scope;
- hard constraints;
- implementation or audit requirements;
- validation expectations;
- required completion report path;
- blockers or dependency order.

The shared results tracker must include:

- wave or cleanup purpose;
- assigned task document paths;
- expected worker report paths;
- worker status table;
- parallel/dependency order;
- consolidated blockers;
- orchestrator review status for each report;
- final wave gate.

Parallel workers must not edit the shared results tracker directly. Each worker writes an individual completion report under `docs/worker-reports/<wave-or-cleanup>/`. The Orchestrator reviews those reports, updates the shared results tracker, and records accepted, needs revision, blocked, or deferred status.

Do not mark a wave or cleanup complete until every expected report is reviewed and accepted, or explicitly deferred with a written caveat in the results tracker.

## Worker Scope Rules

Every worker must have a bounded scope.

Good scopes:

- one file;
- one folder;
- one subsystem;
- one doc section;
- one verification command;
- one focused investigation.

Bad scopes:

- the whole repo;
- the whole feature;
- vague cleanup;
- broad architecture decisions.

If the worker scope cannot be written in one or two lines, the task is too broad.

## Critical Path Rule

The Orchestrator keeps the critical path.

Do not delegate the one task that the whole job is blocked on. If workers are authorized, give workers side tasks that can run independently while the Orchestrator continues the main work.

## Integration Rule

Worker output is not automatically accepted.

The Orchestrator must check:

- did the worker stay in scope?
- did the worker touch only assigned files?
- did the worker make the requested change?
- did the worker report checks?
- did the worker introduce conflicts?
- does the result fit the repo style?

The Orchestrator integrates only after checking.

## Direct Patch Exception Rule

Default rule: the Orchestrator does not do worker implementation work during an orchestrated worker wave.

The Orchestrator may make a direct patch only when all of these are true:

- the user explicitly tells the Orchestrator to patch or fix the blocker directly;
- the patch is narrow and blocking active worker progress;
- stopping to spawn or revise a worker would create more confusion than the direct fix;
- the Orchestrator documents the exception in the wave results tracker and orchestrator memory;
- the Orchestrator updates affected worker task notes before workers resume;
- the Orchestrator runs the smallest relevant verification for the patch.

This exception does not allow the Orchestrator to take over a worker's full assignment, broaden scope, redesign the feature, or silently do implementation because it seems faster.

## Qt GUI Verification Rule

Qt GUI behavior must be tested through direct Qt mechanisms first:

- direct Qt object lookup and property/state assertions;
- `QTest` events;
- in-process Qt verifier harnesses;
- signal/slot observation;
- app-owned logs/state exposed by Qt objects.

Do not use PowerShell, UIA, coordinate-click scripts, screen scraping, or external click automation as the primary GUI interaction method.

PowerShell is permitted only as a shell wrapper through RTK for launching the executable, configuring environment variables, building/running a Qt verifier, collecting logs, or inspecting generated evidence. It must not perform GUI interactions.

If a worker cannot use direct Qt verification, the worker must stop, report the blocker, and ask for an Orchestrator decision before substituting an external GUI automation method.

Every GUI verification report must include the direct Qt method used and the evidence path or asserted Qt object state.

## Final Handoff Checklist

Before saying the orchestration rule is ready, confirm the markdown includes:

- one Orchestrator owns the job;
- no automatic worker spawning;
- explicit user authorization required for workers;
- separate final user confirmation required before launching workers;
- minimal worker prompt;
- narrow worker scope;
- Orchestrator keeps critical path;
- Orchestrator checks and integrates worker output;
- job-done behavior preserves the workflow.

## Non-Negotiable

The Orchestrator is the accountable operator of the job.

Workers are optional, user-authorized, narrow executors.
