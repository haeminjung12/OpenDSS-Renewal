# OpenDSS Repository Agent Policy

Ruleset revision: `ODSS-2026-07-30.3`

This file contains common OpenDSS authority, ownership, safety, and coordination rules. Global policy controls universal inspection, logging, verification, and shared-rule maintenance. Mode-specific procedures live in repository skills.

## Leadership and allocation

- **O-FRONT-001** — One Lead is the sole user-facing agent for an active task. The Implementation Lead owns planned product work; the Debug Lead owns defect work in a designated debug worktree. Temporary workers and validators report only to that Lead.
- **O-OWN-001** — The Lead owns scope, accepted decisions, canonical state or ledger, integration, user updates, and the final result. Each task or artifact has one accountable owner.
- **O-LIMIT-001** — Normally use no more than two workers and one validator concurrently.
- **O-WORK-001** — Give each worker one bounded ID, the Lead's verified root/mode/HEAD snapshot, exact allowed paths, required authority, non-goals, acceptance evidence, build permission, and return format.
- **O-OVERLAP-001** — Do not assign overlapping writes. Serialize unavoidable overlap under one owner.
- **O-SPAWN-001** — Spawn only independent work that materially improves speed, evidence, or isolation. Keep small or coupled work with the Lead.
- **O-MODEL-001** — Follow `G-MODEL-001`: use the lowest-cost capable model and reasoning effort; escalate only for ambiguity, protected assets, high risk, or a failed cheaper attempt.
- **O-WORKER-STATE-001** — Workers never edit `docs/agent-state/current.md` or `docs/debug/bug-ledger.md`.

## Preflight budget

- **O-FAST-001** — In an unchanged verified Lead session, begin work immediately. Do not rerun an initializer, reread policy, check Git/index/watchers, invoke Plan Guardian, or repeat unchanged verification.
- **O-RESUME-001** — On a new Lead chat, handoff, replacement Lead, or compaction, run exactly one appropriate initializer, then read only canonical state and the active slice or bug record.
- **O-ESCALATE-001** — Load more policy, Git history, authority, index, or protected-asset evidence only for a named mismatch, conflict, unexpected dirty path, protected boundary, or material environment change.
- **O-WORKER-PREFLIGHT-001** — Temporary workers consume the Lead's verified task packet. They do not rerun full Lead initialization unless the packet conflicts with their observed root, mode, HEAD, or dirty paths.
- Do not start or refresh Graphify/grepai merely because a session began. Use semantic indexes only when substantial or conceptually unclear investigation justifies them.

## Verification and plan fidelity

- **O-BATCH-001** — Integrate one coherent batch before one full build and required test gate. Workers return targeted evidence and do not each run that build.
- **O-EARLY-CHECK-001** — Before integration, run only narrow checks that resolve a named uncertainty capable of invalidating remaining work.
- **O-RERUN-001** — Do not rerun unchanged checks. After a failure, rerun affected checks until repaired, then perform one final required pass.
- **O-GATE-001** — Use one fresh read-only `$opendss-plan-guardian` before integration, closure, merge, or after a material scope change; never at routine startup.
- **O-DEVIATE-001** — Only the user may approve material deviation from canonical authority or the active plan. Update durable authority before deviating work continues.

## Continuity

- **O-STATE-001** — The active Lead owns `docs/agent-state/current.md`; the Debug Lead also owns `docs/debug/bug-ledger.md`. Store accepted facts and evidence links, not transcripts or private reasoning.
- **O-CHECKPOINT-001** — Before a planned context boundary, record policy revision, mode, active ID, branch, exact HEAD, dirty paths, decisions, evidence, blockers, and next action.
- **O-HANDOFF-001** — Prefer a fresh-chat handoff over repeated compaction when context becomes crowded. Compaction summaries are non-authoritative.
- Existing sessions explicitly adopt changed rules; new sessions load current rules automatically.

## Product authority and engineering boundaries

- **O-AUTH-001** — Follow `docs/v2/CONTEXT.md` for OpenDSS authority precedence. Derived documents and code cannot silently override canonical product authority.
- Preserve one authoritative owner for each domain state. QML/UI code must not call vendor SDKs or the trainer directly.
- Make the smallest coherent authorized change. Do not add speculative factories, registries, strategy selectors, service locators, compatibility layers, placeholder APIs, unused production code, or unrelated cleanup.
- Do not restore superseded v1 navigation, terminology, product state, scientific policy, or editable settings.

## Protected technical assets

- **O-PROTECT-001** — Preserve qualified DCAM, NI-DAQmx, camera acquisition, DAQ output, detector, ONNX Runtime, preprocessing/inference, Python training/export, and proven persistence mechanics unless the active task explicitly authorizes behavioral change.
- Representative protected paths include `app/runtime/dcam_camera.*`, `daq_trigger.*`, `event_detector.*`, `fast_event_detector.*`, `onnx_classifier.*`, `metadata_loader.*`, `training/python/droplet_trainer/**`, and the desktop persistence writers.
- Before a material protected change, record consumers/targets, characterization or regression tests, fixtures, before/after behavior, timing where relevant, required HIL, justification, and rollback.

## Mode entry points

- Implementation Lead: confirm `Mode: implementation`, then use `$opendss-agent-rules-init` only on session boundaries and work one explicitly authorized active slice.
- Debug Lead: confirm `Mode: debug`, then use `$opendss-debug-init` only on session boundaries and work one stable `DBG-*` record at a time.
- Plan fidelity: use `$opendss-plan-guardian` only at the gates named by `O-GATE-001`.

## Build-root retention

- **BUILD-CLEAN-001** — Keep only the active build, latest successful build, and one current diagnostic or fallback build at the top level of `C:\b`.
- Move an inactive completed direct child of `C:\b` intact to the same name under `C:\b\archive` only after confirming no active process, lock, live-validation dependency, current acceptance role, or destination collision.
- Never move a repository, registered worktree, active environment, backup, user data, policy, release/publication state, or anything outside `C:\b`. Never delete or agent-prune `C:\b\archive`; archive deletion is user-owned.
- Preserve failed-build diagnostics before archival. Do not overwrite, merge, flatten, partially move, or create a one-off build root merely to evade a verified lock.

## Process safety

- Before terminating a build blocker, prove its PID, executable, and command line belong to the exact selected build root. Never terminate training/testing, unrelated applications, vendor tools, or another workspace's build.
- Fire DAQ output only when the active task requires hardware verification; record channel, source, settings, count, and result.
