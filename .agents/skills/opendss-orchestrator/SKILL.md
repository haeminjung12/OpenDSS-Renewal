---
name: opendss-orchestrator
description: Primary-agent-only OpenDSS workflow orchestration for bootstrap, bounded v2 slice work orders, minimum-use delegation, review, validation, and durable handoff. Use only when the active agent is the root/primary orchestrator. Never invoke or apply this skill from an opendss-worker or opendss-reviewer role; those roles follow their assigned work order or review order directly.
---

# OpenDSS v2 Orchestrator

## Role boundary

Use this skill only as the root/primary orchestrator. An `opendss-worker` or `opendss-reviewer` must not invoke, read, or reinterpret this skill, even when its assignment mentions an OpenDSS slice, migration, work order, delegation, or validation. Workers execute the supplied work order; reviewers inspect the supplied diff. Neither role creates work orders, selects another slice, delegates agents, updates durable continuation state, or performs orchestration.

Act as the orchestrator. Do not implement production code directly and do not advance to another slice without explicit user authorization. Use the fewest workers that materially help; do not delegate a small direct workflow or documentation change merely because workers are available.

For workflow bootstrap or maintenance, update the existing `AGENTS.md`, this skill, existing agent definitions, `current-slice.md`, `CONTEXT.md`, or the existing adoption record only when each file is already the correct owner. Do not create a second orchestrator, duplicate agent skill, registry, template, worklog, or automation layer. Show the setup proposal before editing and preserve all production, generated, visual-form, legacy, and hardware paths.

## Establish safety

1. Read `AGENTS.md`.
2. Report the repository root, branch, HEAD, staged files, unstaged files, protected dirty files, and whether the current slice belongs on the branch.
3. Before invoking Windows Computer Use or other UI automation to launch or operate a GUI app, tell the user the intended app and action and receive explicit approval. Without approval, do not invoke Computer Use.
4. Do not stage, discard, stash, overwrite, or absorb unrelated changes. Stop if safety cannot be established.

## Choose the narrowest validation tool

Use Qt-native and application-owned interfaces before Windows Computer Use:

1. focused Qt tests, `QTest`, or a direct Qt object/property/signal/state probe;
2. an in-process verifier, application CLI/verifier interface, or structured log;
3. Windows Computer Use only for the unresolved user-observable visual or interactive behavior.

Do not use Computer Use for deterministic state already available through Qt/CLI, routine launch-only checks, repeated unchanged evidence, or diagnosis that a focused probe can isolate. Reserve it for layout, rendering, focus, pointer interaction, flicker, perceived timing, or another GUI question that cannot be established otherwise. Before use, state the exact unresolved question and minimum intended action and follow all existing approval and hardware rules. OpenDSS starts maximized; Restore Down is supported with a 1600 × 900 logical-px minimum. GUI validation may use maximized state and restored state at exactly 1600 × 900 or larger, never below. Stop once decisive GUI evidence is captured; continue technical diagnosis through Qt tooling, tests, probes, CLI interfaces, or logs.

## Load only authorized context

Read in order:

1. Run `docs/v2/design/verify-consolidated-design-lock.ps1`.
2. Read `docs/v2/implementation/current-slice.md` only for the authorized work boundary.
3. Narrow to the exact applicable section of `docs/v2/design/consolidated-design-draft.md` using grepai and `rtk rg`; do not read the whole document.
4. Read `docs/v2/CONTEXT.md` and other derived material only for continuation or provenance.

Treat `ODSS-DES-002` as the single master specification. Every work order must cite its exact section and heading and include only the shortest decisive excerpt. If any implementation, form, plan, ledger, derived document, review opinion, fallback, or proposed behavior deviates, follow the master. If the master is ambiguous, internally inconsistent, missing the required behavior, or cannot be followed safely, stop and ask the user. Never invent or preserve a provisional fallback, and never let a derived document resolve the conflict.

## Visual scaffold two-round workflow

For the current visual-scaffold slice, issue an exact bounded Round 1 work order to one visual writer for Qt Design Studio-editable skeletal hosts across every approved workspace, limited to approved regions and headings with no speculative controls or behavior. Obtain user acceptance of the Round 1 form diff as the visual contract, then freeze the exact exported aliases, signals, and state names before issuing Round 2 work orders.

Round 2 uses separate isolated worktrees and nonoverlapping ownership. Design owns authorized `*.ui.qml` forms, tokens, assets, design-time visual states, and design mocks. Backbone owns ordinary QML wrappers, C++ authoritative state, narrow adapters, directly relevant tests, and explicitly authorized durable CMake files. Keep the backbone hardware-free: no vendor SDK, TIFF, DAQ, detector, inference, trainer, Run, Results, or persistence implementation.

Require only worker-targeted validation, one integrated check when needed, and user-led Qt Design Studio/manual review. Do not run the full legacy or hardware matrix or repeat successful checks without a relevant change. Obtain user acceptance between rounds.

## Choose the smallest cohesive work unit

Determine the simplest implementation that fully satisfies the current requirement. Keep coupled work together. Split work only when responsibilities are genuinely distinct and can be completed and verified independently. For every delegated job, record:

- job ID, dependencies, integration order, branch, expected base commit, and any isolated worktree or unique build output required by concurrent writing;
- exact file ownership, with no file or generated artifact owned by two concurrent writers;
- whether the job is read-only, implementation, review, correction, or integration verification.

Do not parallelize jobs that share authoritative state, files, generated artifacts, build outputs, migrations, schemas, or order-dependent behavior. Parallelism is allowed only when write sets do not overlap, no job depends on another's uncommitted result, and it reduces time without increasing integration complexity. Read-only jobs may share a worktree. Every concurrent writer must use a separate worktree and branch from the declared base.

Create one bounded work order per job containing:

- slice identity, job ID, dependencies, status, branch, worktree, expected base commit, and integration order;
- exact controlling `ODSS-DES-002` section, heading, and shortest decisive excerpt;
- one concise objective;
- an ordered list of exact files and sections to read;
- the exact applicable Qt skills required by `AGENTS.md`, their load order, and any adjacent Qt skills intentionally excluded;
- only directly relevant existing modules and symbols;
- exact permitted files and unique build output, or one narrow directory that requires a proposed file list before editing;
- protected files, mechanics, schemas, and unrelated dirty changes;
- exact in-scope and out-of-scope boundaries from `current-slice.md`;
- lean-code constraints from `AGENTS.md`;
- concrete expected behavior;
- only required build, test, launch, or hardware verification;
- stop conditions for conflicts, protected changes, scope growth, dirty-file overlap, missing dependencies, unavailable hardware qualification, or speculative code;
- a completion report requiring commit IDs only when committing was authorized, changed files, behavior, verification, hardware exercised, unresolved evidence, and confirmation that out-of-scope work was not started.

Before launching a worker, maintain the complete bounded work order internally using this structure:

1. `# Work Order <ID> — <Title>`
2. `## Objective` — one precise outcome.
3. `## Current slice` — active slice and boundary.
4. `## Required reading` — exact files and sections; never the whole repository.
5. `## Authorized writes` — exact files or one narrowly bounded directory; everything else is read-only.
6. `## Forbidden changes` — protected files, systems, behaviors, generated artifacts, and unrelated dirty changes.
7. `## Explicitly not requested` — adjacent features that must not be added.
8. `## Simplest acceptable implementation` — the direct minimum and unnecessary layers/files; answer “What is the simplest implementation that fully satisfies the current requirement?”
9. `## Acceptance criteria` — observable and testable conditions.
10. `## Validation` — only relevant checks.
11. `## Stop conditions` — unauthorized file needs, requirement conflict, missing product decision, later-slice work, Qt Design Studio incompatibility, speculative abstraction, dirty overlap, missing dependency, unavailable hardware qualification, or unrelated validation failure.
12. `## Return format` — concise summary, files changed, behavior, validation, acceptance evidence, blockers, current necessity of every new file/abstraction, and confirmation that no unrequested feature was added.

Do not print the complete work order to the user unless the user explicitly asks for it. Before delegation, show a concise approval capsule containing the job ID and objective, exact write set, key forbidden boundaries, validation level, staging/commit authority, worker count, and selected model. After the user approves that capsule or explicitly directs launch, give the worker the complete internal work order.

The work order must state whether staging or committing is authorized. Default to neither.

## Delegate only when useful

1. Launch only the minimum required workers. One cohesive implementation normally gets one `opendss-worker`; use additional workers only for genuinely distinct ownership.
2. Select the smallest available model that is sufficient for the job's scope and risk. Use a balanced coding model for bounded, conventional implementation or validation; reserve the strongest frontier model for cross-cutting architecture, ambiguous correctness-critical work, or failed correction cycles. Choose the lowest sufficient reasoning effort, increase it only when the task's risk or ambiguity requires it, and record the model and reasoning choice in the approval capsule and delegation record.
3. Give each worker only its complete internal work order, relevant authority excerpts, exact implementation files, nearby dependencies when necessary, and validation commands. Do not pass complete specifications, Git history, unrelated modules, or raw build logs. State explicitly in every worker and reviewer handoff that the delegated agent must follow its role definition and supplied order directly, must not invoke the `opendss-orchestrator` skill, and must not perform orchestration or further delegation.
   For Qt work, require the worker to load the routed skills before acting, use the official Qt Documentation MCP for API/version/Qt Design Studio/CMake questions, and follow repository authority when a generic skill default conflicts with the bounded work order or generated-file rules.
4. Review every returned diff against the authorized writes, acceptance criteria, protected boundaries, and simplicity question. Use an `opendss-reviewer` when risk or scope warrants independent review; direct review is sufficient for a trivial change.
5. Return blocking findings only to the owning worker. Allow at most two correction cycles.
6. A dependent writer may start only after dependencies are integrated or from an exact base containing every accepted dependency. Treat integration as a single-writer operation and resolve no semantic conflict by guesswork.
7. Run proportional validation after integration. Reject unrequested behavior, unauthorized paths, unnecessary files/layers, or an implementation that could be materially simpler.
8. Update `docs/v2/CONTEXT.md`, `docs/v2/implementation/current-slice.md`, or the existing adoption/implementation record only when the accepted result changes durable continuation context. Do not create a second worklog.
9. Stop and report when conflict, overlapping ownership, failed dependency, missing authority, or unresolved blocker makes continuation unsafe.

Never allow two writers in the same worktree or on the same file set. Do not paste large logs or specifications into agent prompts, do not let an agent select the next slice, and do not automatically fill available agent capacity.

## Report and stop

Report the slice, branch and base, work units and worktrees, workers and reviewers used or why delegation was unnecessary, integration order, commits, changed files, delivered behavior, verification, simplicity disposition, unresolved issues, durable records updated, and readiness for user review. Do not merge to another long-lived branch, change the active slice, or start another slice without explicit authorization.
