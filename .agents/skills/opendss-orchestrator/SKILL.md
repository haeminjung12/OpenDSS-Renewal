---
name: opendss-orchestrator
description: Supervise one bounded OpenDSS v2 implementation slice through an exact work order, one implementation subagent, and one read-only review subagent. Use for OpenDSS implementation slices, bounded work orders, continuing the OpenDSS v2 migration, or orchestrating the current approved slice.
---

# OpenDSS v2 Orchestrator

Act as the orchestrator. Do not implement production code directly and do not advance to another slice without explicit user authorization.

## Establish safety

1. Read `AGENTS.md`.
2. Report the repository root, branch, HEAD, staged files, unstaged files, protected dirty files, and whether the current slice belongs on the branch.
3. Do not stage, discard, stash, overwrite, or absorb unrelated changes. Stop if safety cannot be established.

## Load only authorized context

Read in order:

1. `docs/v2/CONTEXT.md`.
2. `docs/v2/implementation/current-slice.md`.
3. Only the exact canonical sections and audit or implementation-plan sections referenced by the current slice.

Report canonical conflicts instead of resolving them silently. Do not broadly reread source or legacy repositories.

## Issue one work order

Create one bounded work order containing:

- slice identity, status, branch, and expected base commit;
- one concise objective;
- an ordered list of exact files and sections to read;
- only directly relevant existing modules and symbols;
- exact permitted files or one narrow directory that requires a proposed file list before editing;
- protected files, mechanics, schemas, and unrelated dirty changes;
- exact in-scope and out-of-scope boundaries from `current-slice.md`;
- lean-code constraints from `AGENTS.md`;
- concrete expected behavior;
- only required build, test, launch, or hardware verification;
- stop conditions for conflicts, protected changes, scope growth, dirty-file overlap, missing dependencies, unavailable hardware qualification, or speculative code;
- a completion report requiring commits, changed files, behavior, verification, hardware exercised, unresolved evidence, and confirmation that out-of-scope work was not started.

## Delegate sequentially

1. Spawn exactly one `opendss-worker` with only the work order, required paths, and necessary repository state. Wait for completion.
2. Spawn exactly one `opendss-reviewer` with only the work order, worker commit or diff range, changed files, concise completion report, and acceptance criteria. Wait for completion.
3. If blockers exist, return only those findings to the same worker, authorize only needed files, rerun relevant verification, and ask the same reviewer to recheck corrected areas.
4. Allow at most two correction cycles. Stop and report blockers that remain.

Do not run multiple implementation workers, paste large logs or specifications into agent prompts, or let an agent select the next slice.

## Report and stop

Report the slice, branch and base, worker and reviewer, commits, changed files, delivered behavior, verification, reviewer disposition, unresolved issues, and readiness for user review. Do not merge, update `current-slice.md`, start another slice, or launch another worker without explicit authorization.
