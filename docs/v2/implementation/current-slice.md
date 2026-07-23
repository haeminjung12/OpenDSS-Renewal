# OpenDSS v2 functional slice — Dataset Load + read-only Dataset Summary

## Status

**Opened:** July 23, 2026  
**Authorization:** The user accepted the current visual/mock baseline on July 23, 2026. Appearance and graphics refinements continue in a separate design thread; acceptance does not claim that every visual imperfection is fixed or that manual review found no issues.

The visual navigation scaffold and Mock Single Image slice is closed as accepted. Its accepted forms, exported aliases, and frozen visual/runtime seam are the functional contract by default. A design-thread change requires exact named form ownership and an alias/interface handoff before functional integration; no functional work order may silently adapt an unannounced form change.

**Prerequisite gate — not yet passed:** restore durable `qds.cmake` build integration and then run the focused Qt Quick test successfully. Until both steps have evidence, no production implementation work order in this slice may launch or be accepted. `qmllint` evidence alone does not satisfy this gate.

Preserve the sole dirty Qt Design Studio-generated reorder in `Desktop_app_v2Content/CMakeLists.txt` and the protected locally excluded `Desktop_app_v2.qmlproject.qtds`; do not absorb, overwrite, or hand-edit either as part of this slice.

## Current objective

Implement exactly one first production capability: select and load an existing OpenDSS v2 Dataset artifact through its `dataset.json` manifest, then project its authoritative read-only summary in **Data > Label**.

## Required reading

1. [Repository agent policy](../../../AGENTS.md).
2. [Implementation context](../CONTEXT.md).
3. [Approved Product Model](../canonical/product-model.md), §7.2 **Data > Label**, §8 **Artifact model**, §15 **Persistence and provenance**, and D-012.
4. [Information Architecture](../canonical/information-architecture.md), §0 **Amendment-controlled screen inventory** and §4.5 **Data > Label**.
5. [Interaction and Application-State Specification](../canonical/interaction-and-state.md), §0 and its authoritative-state/error rules.
6. [Detailed Workflow Specification](../canonical/detailed-workflows.md), §§14.3–14.4, 14.10, 25, 73.2, and 74.1–74.2.
7. [User-approved visual-review amendment](../OpenDSS_v2_UIUX_Visual_Review_Amendment_2026-07-23.md), **Label** and **Implementation consequence**, only for the accepted composition and scope boundary.
8. [Visual scaffold first-two-round plan](visual-scaffold-two-round-plan.md), as the accepted visual-contract record.

## In scope

- real selection of an existing v2 `dataset.json` manifest and authoritative loading of that Dataset artifact;
- a single authoritative read-only projection for Label: Dataset identity, selected path, load status, total and labeled counts, configured two- or three-class schema, and class metadata/counts defined by the Dataset contract;
- deterministic direct errors and recovery for unreadable, unsupported, missing, or locked Dataset artifacts;
- targeted tests and only the narrow controller/backend/ordinary-QML-wrapper integration named by later exact work orders.

The selected schema is displayed only. Numeric Class IDs and class metadata are read from the artifact; this slice does not change them.

## Out of scope

- changing class schema; class-name edits; labeling, relabeling, exclusion, undo, navigation, or any other label mutation/action;
- filters that mutate data; Dataset Save As/copy; capture; Training; Model Test; hardware; TIFF; Camera; DAQ; detector; inference; Results; and broad persistence work;
- edits to accepted `*.ui.qml` forms, visual assets, or tokens;
- changes to protected reusable technical assets or generated CMake files;
- any later functional slice.

## Work-order and validation boundary

Each implementation work order must name exact writable controller/backend, wrapper QML, test, and durable CMake files; keep all other forms and generated files read-only. It must preserve one authoritative Dataset state owner and must not add a speculative framework, duplicate state, or a compatibility path.

After the prerequisite gate has passed, validation is proportional: focused loader/summary tests, the focused Qt Quick test, and one directly relevant configure/build when required. Do not run full legacy, Python, or hardware suites.

## Simplest complete approach

Use one Dataset-loading owner to parse and validate the selected `dataset.json`, expose one immutable summary projection to the existing Label wrapper, and test valid and invalid manifests. Do not implement write paths, schema switching, label actions, or a general persistence framework.
