---
name: opendss-debug-init
description: Resume an OpenDSS Debug Lead from verified repository state and the canonical bug ledger. Use only for a new Debug Lead chat, handoff, replacement Lead, compaction recovery, or a reported root/state mismatch; do not use for ordinary prompts in an unchanged verified session or for a temporary investigator with a valid Lead task packet.
---

# OpenDSS Debug Resume

Run the bundled read-only verifier:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "<skill-directory>\scripts\Initialize-OpenDssDebugWorkspace.ps1" -AsJson
```

Use `pwsh` instead of `powershell` on macOS/Linux. Pass `-RepoRoot` only when outside the intended debug worktree.

## Resume

1. Stop for the wrong root, missing files, non-debug mode, policy mismatch, branch mismatch, or HEAD mismatch.
2. Read only `requiredReads`: canonical state and the bug ledger.
3. Select one stable active `DBG-*` record and retrieve only its linked evidence.
4. Report bug ID, reproduction status, branch, HEAD, dirty paths, blockers, and next action.

## Debug contract

- Capture the smallest reliable reproduction and expected versus observed behavior before production changes.
- Separate observations, hypotheses, and accepted conclusions.
- Change only what the accepted root cause requires.
- Add the narrowest useful regression test or record why none is feasible.
- Close only with root cause, changed files, verification, remaining risk, and rollback.
- Do not bypass protected-asset evidence gates.

Do not invoke this skill again in the same unchanged verified session. Temporary investigators use the Debug Lead's verified task packet unless observed facts conflict.

Only the user-facing Debug Lead updates canonical state or `docs/debug/bug-ledger.md`.
