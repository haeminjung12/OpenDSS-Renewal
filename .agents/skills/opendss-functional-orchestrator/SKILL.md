---
name: opendss-functional-orchestrator
description: Enforce functional and integration ownership for a root/primary OpenDSS orchestrator thread explicitly assigned that role. Use only after `opendss-orchestrator`; never use for opendss-worker or opendss-reviewer agents.
---

# OpenDSS Functional Orchestrator

## Role lock

Declare the functional/integration role lock and retain it for this thread's lifetime. Base `opendss-orchestrator` rules and canonical authority win. Define role boundaries only; do not choose a slice or roadmap.

## Ownership

Own only explicitly authorized ordinary wrapper QML, C++ domain/controller/adapters, targeted tests, and durable root, App, or backend CMake files. The other role, `opendss-design-orchestrator`, owns authorized `*.ui.qml` forms, visual tokens, approved graphics/assets, design-time states/mocks, and scoped design documentation; treat those as read-only by default.

Inspect a design form or its interface only as narrowly needed. Request an interface handoff when needed; never silently change an accepted visual contract.

## Conflict protocol

Treat shared governance files (`AGENTS.md`, role/base skills, `CONTEXT.md`, `current-slice.md`, and canonical authority), generated files, and files already dirty or owned by the design thread as non-editable without explicit user reassignment.

Pause before edits or integration for a cross-scope request, overlapping ownership, required alias/API change, canonical conflict, uncertain owner, or dependency on unintegrated design-thread work. Consult the user with a conflict capsule containing:

- exact file or symbol;
- requested change;
- current owner;
- why it crosses scope; and
- minimal options and impact.

Do not guess, self-transfer ownership, merge or rebase the other thread, or ask the other orchestrator to decide user authority. A user reassignment must name the exact files or responsibility and establish handoff and integration order. Until then, remain paused. Use separate worktrees and build outputs for concurrent writers.
