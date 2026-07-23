# OpenDSS v2 implementation context

## Authority

Use this order when requirements differ:

1. [Approved Product Model](canonical/product-model.md).
2. [User-approved visual-review amendment](OpenDSS_v2_UIUX_Visual_Review_Amendment_2026-07-23.md) for its listed UI layout, naming, disclosure, and composition topics. It is subordinate to the Product Model and supersedes conflicting lower visual sources.
3. [Approved UI/UX Design Amendment](OpenDSS_v2_UIUX_Design_Amendment.md) for its nonconflicting UI, layout, naming, interaction, and workflow decisions.
4. [Information Architecture and Screen Inventory](canonical/information-architecture.md) and [Interaction and Application-State Specification](canonical/interaction-and-state.md), except where the scoped visual-review amendment supersedes conflicting visual language.
5. Nonconflicting requirements in the [Detailed Workflow Specification](canonical/detailed-workflows.md).
6. [Consolidated Product Design Specification](design/consolidated-design-draft.md), which remains **Consolidated Draft for Review**.
7. [Archived historical design evidence](archive/product-design-draft-v0.1.md).
8. Repository code as implementation evidence only.

Except for the expressly scoped user-approved visual-review amendment, derived engineering documents, including this file, cannot override canonical specifications. Report a conflict rather than choosing a convenient interpretation.

## Documentation map

| Topic | Authoritative document | Relevant heading or section | Derived engineering document, when useful |
|---|---|---|---|
| Product scope and exclusions | [Approved Product Model](canonical/product-model.md) | §2 Product definition; §17 First-release boundaries | — |
| User-approved visual review | [UI/UX Visual Review Amendment — 2026-07-23](OpenDSS_v2_UIUX_Visual_Review_Amendment_2026-07-23.md) | All decisions; scoped override for listed visual topics | Current Slice |
| Approved UI/UX amendment | [Approved UI/UX Design Amendment](OpenDSS_v2_UIUX_Design_Amendment.md) | §§2–19 | All downstream documents listed below |
| Navigation and workspace behavior | [Information Architecture and Screen Inventory](canonical/information-architecture.md) | §1 Application shell; §2 Navigation hierarchy; §3 Complete workspace inventory; §4 Workspace state inventory | — |
| Application state and resource locks | [Interaction and Application-State Specification](canonical/interaction-and-state.md) | §3 Conceptual application-state model; §17 Shared resource ownership and navigation effects | — |
| Fixed versus editable configuration | [Approved Product Model](canonical/product-model.md) | §11 Editable and fixed configuration; also IA §1.6 Bottom Hardware panel | — |
| Detector behavior | [Approved Product Model](canonical/product-model.md) plus nonconflicting [Detailed Workflow Specification](canonical/detailed-workflows.md) | Product Model §7.1, §7.8, and §11.3; Workflow §13.6 Detection and crop rule | [Reusable-core audit](audits/reusable-core-audit.md), findings F-A01 and F-A02 |
| Protected reusable assets | [Repository agent policy](../../AGENTS.md) | Protected reusable technical assets; Change-control rule for protected assets | [Reusable-core audit](audits/reusable-core-audit.md) |
| Persistence/threading requirements | [Approved Product Model](canonical/product-model.md) plus nonconflicting [Detailed Workflow Specification](canonical/detailed-workflows.md) | Product Model §15 Persistence and provenance; Workflow §31 Event persistence | [Reusable-core audit](audits/reusable-core-audit.md), especially F-E02; [implementation plan](implementation/reuse-first-plan.md), P0-2 |
| Qt Design Studio handoff | [Consolidated Product Design Specification](design/consolidated-design-draft.md), subordinate and under review | §24 Qt Design Studio and design-handoff contract | [Qt Design Studio adoption record](implementation/qt-design-studio-adoption.md) |
| Current design-review gate | Canonical sources above | Page-by-page visual interpretation | [Page Composition Interpretation](reviews/page-composition-review.md) and [Current Slice](implementation/current-slice.md) |
| Future implementation sequence | — | Engineering guidance only | [Reuse-First Implementation Plan](implementation/reuse-first-plan.md) |
| First two visual rounds | Current slice and canonical sources above | Visual scaffold and frozen form/runtime seam | [Visual Scaffold Two-Round Plan](implementation/visual-scaffold-two-round-plan.md) |

## Reading workflow

1. Read [AGENTS.md](../../AGENTS.md).
2. Read this file.
3. Read [current-slice.md](implementation/current-slice.md).
4. Open only the canonical sections referenced by the current slice.
5. Open a complete specification only when the task spans it or a conflict is unclear.

## Current status

- The user-approved [July 23, 2026 visual-review amendment](OpenDSS_v2_UIUX_Visual_Review_Amendment_2026-07-23.md) remains authoritative for its listed visual topics. Its visual/mock baseline was accepted by the user on July 23, 2026; remaining appearance and graphics refinements continue in a separate design thread.
- The accepted visual navigation scaffold and Mock Single Image slice is closed. Its forms, exported aliases, and frozen seam are the functional contract by default. A design change requires exact named form ownership and an alias/interface handoff before functional integration.
- The current slice is [Dataset Load + read-only Dataset Summary](implementation/current-slice.md): it alone authorizes later bounded production work for selecting an existing v2 `dataset.json` and presenting its authoritative read-only Label summary. No other production behavior is authorized.
- The build/test prerequisite gate passed on integrated main: `5dfcf0f` restored durable `qds.cmake` integration and `176b84e` corrected the test-module import. A clean Qt 6.11.1 MinGW/Ninja configure passed; `Desktop_app_v2App` and `tst_ShellSingleImage` built; the direct offscreen JUnit run passed 21 cases with zero failures, errors, or skips; and `ShellSingleImage` CTest passed 1/1. Bounded Dataset Load + read-only Dataset Summary functional work orders may now launch.
- The final approved structure records Label's ordered outer panel and Dataset Save As semantics, the shared 390 px outer-panel width/collapse behavior, Train and Model Test Results placement, and Settings Visuals/Text Size policy. D-012 permits configured Datasets to switch between two and three classes while preserving label consistency; D-017 defines the sole application-wide Text Size preference.
- The canonical product, information-architecture, interaction-state, and detailed-workflow baselines incorporate the approved July 23, 2026 UI/UX amendment. The active [page-composition review](reviews/page-composition-review.md) remains a review artifact, not independent authority.
- A bounded work order naming exact writable files remains required before any source, form, wrapper, test, or CMake input is edited.
- Preserve the locally excluded `Desktop_app_v2.qmlproject.qtds` and the sole dirty Qt Design Studio-generated reorder in `Desktop_app_v2Content/CMakeLists.txt`; do not absorb or overwrite either.
- P0-1 detector characterization and neutral-contract work remains accepted groundwork. Camera, TIFF, DAQ, Training, Live, Sequence Test, Results, persistence, and all other functional integrations require later explicit slices.
