---
name: opendss-agent-rules-init
description: Resume an OpenDSS Implementation Lead from verified repository state. Use only for a new Lead chat, handoff, replacement Lead, compaction recovery, or a reported root/state mismatch; do not use for ordinary prompts in an unchanged verified session or for a temporary worker with a valid Lead task packet.
---

# OpenDSS Implementation Resume

Run the bundled read-only verifier from the intended repository:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "<skill-directory>\scripts\Initialize-OpenDssAgentRules.ps1" -AsJson
```

On macOS/Linux with PowerShell:

```bash
pwsh -NoProfile -File "<skill-directory>/scripts/Initialize-OpenDssAgentRules.ps1" -AsJson
```

Pass `-RepoRoot "<absolute path>"` only when running outside the intended worktree.

## Resume

1. Stop for the wrong root, missing files, non-implementation mode, policy mismatch, branch mismatch, or HEAD mismatch.
2. Read only the files returned in `requiredReads`; normally canonical state and the active slice.
3. Retrieve only evidence named by the active ID and exact next action.
4. Report mode, active ID, branch, HEAD, dirty paths, blocker, and next action.
5. Continue within the active slice.

The verifier never edits, fetches, switches branches, starts indexes/watchers, spawns agents, or updates state.

## Fast path and ownership

Do not invoke this skill again in the same unchanged verified session. Temporary workers use the Lead's verified task packet and invoke this skill only if their observed root, mode, HEAD, or dirty paths conflict with it.

Only the user-facing Implementation Lead updates `docs/agent-state/current.md`.
