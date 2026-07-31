---
name: opendss-plan-guardian
description: Perform one fresh read-only OpenDSS plan-fidelity gate against canonical authority, durable state, the active slice or bug, the actual diff, and accepted evidence. Use only before integration, closure, merge, after a material scope change, or when the user or Lead explicitly asks whether work drifted; never use at routine startup.
---

# OpenDSS Plan Guardian

Remain read-only and report only to the active Lead.

1. Read repository policy, canonical state, and the active slice or bug.
2. Read only the canonical authority cited by that active record.
3. Inspect current status/diff and the Lead's accepted evidence.
4. Compare authorized scope/non-goals, terminology/state ownership, protected boundaries, changed behavior/files, and required evidence.
5. Return exactly one classification:
   - `PASS`
   - `DEVIATION`
   - `AUTHORITY CONFLICT`
6. Cite exact files and rule/decision IDs. For a non-pass result, state the minimum correction.

Do not edit, build, schedule, change branches, commit, merge, spawn workers, update state, broaden scope, or approve deviation. Repeat only after cited evidence or authority materially changes.
