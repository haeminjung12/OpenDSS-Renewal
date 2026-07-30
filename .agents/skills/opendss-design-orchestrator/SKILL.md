---
name: opendss-design-orchestrator
description: Enforce visual, design, and graphics ownership for a root/primary OpenDSS orchestrator thread explicitly assigned that role. Use only after `opendss-orchestrator`; never use for opendss-worker or opendss-reviewer agents.
---

# OpenDSS Design Orchestrator

## Role lock

Declare the visual/design/graphics role lock and retain it for this thread's lifetime. Base `opendss-orchestrator` rules and canonical authority win. Define role boundaries only; do not choose a slice or roadmap.

## Ownership

Own only explicitly authorized `*.ui.qml` forms, visual tokens, approved graphics/assets, scoped design documentation, and explicitly named Qt Design Studio design-time mock files in the generated project's `MockDatas/` folder (or another exact design-only path named by authority). The other role, `opendss-functional-orchestrator`, owns ordinary wrapper QML, `app/runtime/Desktop_app_v2/Desktop_app_v2/MockAppState.qml`, other runtime/mock backbone state when an exact work order names it, tests, C++, persistence, backend adapters, hardware, and CMake; treat those as read-only by default.

The word `mock` alone assigns no ownership. Pause and consult the user for a different or uncertain mock path.

Preserve accepted aliases, properties, signals, and states. Do not add production behavior. If a refinement requires an interface change, propose the exact delta and pause before editing or integration.

## Conflict protocol

Treat shared governance files (`AGENTS.md`, role/base skills, `CONTEXT.md`, `current-slice.md`, and canonical authority), generated files, and files already dirty or owned by the functional thread as non-editable without explicit user reassignment.

Pause before edits or integration for a cross-scope request, overlapping ownership, required alias/API change, canonical conflict, uncertain owner, or dependency on unintegrated functional-thread work. Consult the user with a conflict capsule containing:

- exact file or symbol;
- requested change;
- current owner;
- why it crosses scope; and
- minimal options and impact.

Do not guess, self-transfer ownership, merge or rebase the other thread, or ask the other orchestrator to decide user authority. A user reassignment must name the exact files or responsibility and establish handoff and integration order. Until then, remain paused. Use separate worktrees and build outputs for concurrent writers.
