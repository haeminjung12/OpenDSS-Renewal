# OpenDSS Repository Agent Policy

Ruleset revision: `ODSS-2026-07-30.2`

This file contains OpenDSS-specific authority, orchestration, continuity, engineering, and safety rules. Global policy remains authoritative for universal rules, filesystem investigation, work reports, and shared-rule change control.

## User-facing leadership

- **O-FRONT-001** — One Lead is the sole user-facing agent for an active task. The Implementation Lead owns planned product work. The Debug Lead owns defect investigation in a designated debug task or worktree. Workers and validators report only to the active Lead.
- **O-OWN-001** — The Lead owns scope, accepted decisions, the canonical ledger, integration, user updates, and the final result. Each artifact or task ID has exactly one accountable owner.
- **O-LIMIT-001** — Normally use no more than two workers and one validator at once. Exceed this only when the independent work and coordination benefit are explicit.
- **O-WORK-001** — Give every worker one bounded task ID, allowed paths, required authority inputs, acceptance evidence, and return format.
- **O-OVERLAP-001** — Do not assign overlapping write ownership. Serialize unavoidable overlap under one owner.
- **O-SPAWN-001** — Spawn only for independent work that materially improves speed, evidence quality, or isolation. Keep small or tightly coupled work with the Lead.
- **O-REPORT-001** — Workers return changed IDs, files, evidence, blockers, and the next recommendation. They do not send parallel user-facing narratives or raw transcripts.
- **O-VALID-001** — Use one scoped independent validation pass when risk warrants it. Validate the user-observable outcome and protected behavior; do not create review-of-review loops.
- **O-RULES-001** — At task start, load the current global and OpenDSS revisions. If either changes during work, explicitly reread the changed authority files before continuing; start a fresh agent when safe adoption is uncertain.

## Verification scheduling

- **O-BATCH-001** — Treat an approved collection of related changes as one integration batch. Integrate the coherent batch before running one full build and its required tests; do not run the full build after every edit or worker result.
- **O-EARLY-CHECK-001** — Before batch completion, run only narrow checks needed to resolve a named uncertainty that could invalidate remaining work. An early full build requires an explicit build-boundary, protected-asset, ABI, toolchain, or hardware reason.
- **O-RERUN-001** — Do not rerun verification when its relevant inputs and evidence are unchanged. After a failure, rerun affected checks until repaired, followed by one final required pass.
- **O-WORKER-BUILD-001** — Workers do not each run the same full build. Workers return targeted evidence; the Lead performs the integrated build after accepting the batch.

## Plan fidelity

- **O-PLAN-001** — Measure plan fidelity against canonical OpenDSS authority and the active durable plan or state, never agent memory.
- **O-GATE-001** — Use one fresh read-only `$opendss-plan-guardian` before integration, closure, merge, or after a material scope change.
- **O-DEVIATE-001** — Only the user may approve a material deviation from canonical authority or the active plan. Update durable authority before the deviating work continues.
- **O-GUARD-001** — The Plan Guardian reports evidence to the active Lead and owns no implementation, scheduling, canonical state, or user communication.

## Durable continuity

- **O-STATE-001** — The active Lead owns `docs/agent-state/current.md` as the single canonical durable state file. It contains accepted facts and evidence links, not raw transcripts or private reasoning.
- **O-CHECKPOINT-001** — Before a planned handoff or context boundary, record policy revision, mode, active IDs, branch, checkpoint commit, dirty paths, accepted decisions, tests, blockers, and the exact next action.
- **O-RESUME-001** — A replacement Lead or worker must run the applicable OpenDSS initializer, load current rules and durable state, and verify branch, commit, dirty files, and referenced evidence before acting.
- **O-HANDOFF-001** — Prefer a fresh-agent handoff over repeated compaction when context becomes materially crowded. Checkpoint before the handoff.
- **O-WORKER-STATE-001** — Workers never independently rewrite `docs/agent-state/current.md` or the canonical bug ledger. The Lead accepts or rejects reports and performs state updates.
- **O-COMPACT-001** — Compaction output is non-authoritative. After any compaction, reread and verify durable state before continuing.

## OpenDSS v2 authority

- **O-AUTH-001** — Apply the authority order in `docs/v2/CONTEXT.md`. In brief: Approved Product Model; Information Architecture plus Low-Fidelity Interaction/Application-State specification; nonconflicting Detailed Workflow; Consolidated Draft for Review; historical draft; code as implementation evidence only.
- D-001 through D-019 belong only to the Approved Product Model. Existing v1 behavior cannot reintroduce superseded navigation, terminology, product states, scientific policy, or editable settings.
- Derived engineering documents cannot override canonical specifications. Report conflicts instead of silently resolving them.

## Protected reusable technical assets

- **O-PROTECT-001** — Preserve qualified DCAM, NI-DAQmx, camera acquisition, DAQ output, detector, ONNX Runtime, preprocessing/inference, Python training, model export, and proven background/atomic persistence mechanics unless the active slice explicitly authorizes behavioral change.
- Representative protected paths include:
  - `app/runtime/dcam_camera.*`
  - `app/runtime/daq_trigger.*`
  - `app/runtime/event_detector.*`
  - `app/runtime/fast_event_detector.*`
  - `app/runtime/onnx_classifier.*`
  - `app/runtime/metadata_loader.*`
  - `training/python/droplet_trainer/**`
  - `app/runtime/desktop_app/json_persistence.*`
  - `app/runtime/desktop_app/live_data_collection_writer.*`
  - `app/runtime/desktop_app/live_log_writer.*`
  - `app/runtime/desktop_app/sequence_summary_writer.*`
- Before consolidating, deleting, replacing, or materially changing a protected module, record its consumers and build targets, characterization/regression tests, representative fixtures, before/after behavior, timing evidence where relevant, hardware-in-the-loop evidence where hardware is affected, justification, and rollback strategy.

## Engineering boundaries

- QML and UI code must not call vendor SDKs or the trainer directly.
- Separate old product policy from reusable mechanics. Do not restore user-facing detector, crop, routing, internal timing, or training-hyperparameter controls from old code.
- Keep one authoritative owner for each domain state; duplicated widget-local state cannot become v2 architecture.
- Make the smallest coherent change that satisfies the active slice. Every new production class, function, field, flag, and abstraction needs an immediate production consumer or required characterization test.
- Do not add speculative factories, registries, plugin systems, strategy selectors, service locators, compatibility layers, placeholder APIs, TODO scaffolding, or unrelated cleanup.
- A temporary parallel implementation requires a documented reason, removal condition, and tests protecting retained behavior.
- Comments explain non-obvious constraints or rationale; they do not narrate straightforward code.

## Implementation mode

1. Read `AGENTS.md`.
2. Run `$opendss-agent-rules-init` to verify repository and checkpoint state.
3. Read `docs/v2/CONTEXT.md`.
4. Read `docs/agent-state/current.md`.
5. Read `docs/v2/implementation/current-slice.md`.
6. Read only canonical sections referenced by the active slice.
7. Work on one explicitly authorized slice at a time.

`docs/v2/implementation/current-slice.md` is currently P0-1 and remains planned until the user explicitly authorizes implementation.

## Debug mode

- **DBG-ID-001** — Assign one stable bug ID to every investigation and fix.
- **DBG-REPRO-001** — Capture the smallest reliable reproduction and expected versus observed behavior before changing production code.
- **DBG-EVID-001** — Separate observations, hypotheses, and conclusions. Attach commands, logs, tests, or artifacts to accepted findings.
- **DBG-SCOPE-001** — Change only what the accepted root cause requires. Keep cleanup and unrelated refactoring out of the fix.
- **DBG-REGRESS-001** — Add the narrowest useful regression test or explain the exact reason it is not feasible.
- **DBG-PROTECT-001** — Do not bypass protected-asset evidence gates during debugging.
- **DBG-VERIFY-001** — Verify the reproduction no longer fails and relevant existing behavior still passes.
- **DBG-CLOSE-001** — Close a bug only with root cause, changed files, verification evidence, remaining risk, and rollback notes.
- **DBG-LEDGER-001** — The Debug Lead owns `docs/debug/bug-ledger.md` and accepts worker findings into it.
- **DBG-USER-001** — Only the Debug Lead communicates bug status, decisions, and completion to the user.

For a debug task, run `$opendss-debug-init`, confirm `Mode: debug` in `docs/agent-state/current.md`, and use one active bug ID.

## Process safety

- When a process blocks the selected OpenDSS build output, identify its PID, executable path, and command line. Terminate it only when those details prove it belongs to that exact build root.
- Never terminate OpenDSS while training or testing is active. Never terminate unrelated user applications, camera/vendor tools, DAQ utilities, or builds targeting another workspace.
- After terminating a verified blocker, report its PID and path or command line, then retry the original build.
- DAQ output may be intentionally fired only when the active task requires hardware verification. Record trigger source, channel, waveform/settings, count, and observed or logged result.

## Inspection and verification

- Follow `G-INSPECT-001` and `C:\Users\goals\.codex\RTK.md`; do not duplicate its changing command policy here.
- Prefer deterministic unit, integration, replay, build, and hardware evidence in proportion to risk.
- Refresh semantic or graph indexes only when their task value justifies it.
