# OpenDSS v2 implementation context

## Authority

Use this order when requirements differ:

1. [Approved Product Model](canonical/product-model.md).
2. [Approved UI/UX Design Amendment](OpenDSS_v2_UIUX_Design_Amendment.md) for every UI, layout, naming, interaction, and workflow matter it explicitly changes.
3. [Information Architecture and Screen Inventory](canonical/information-architecture.md) and [Interaction and Application-State Specification](canonical/interaction-and-state.md).
4. Nonconflicting requirements in the [Detailed Workflow Specification](canonical/detailed-workflows.md).
5. [Consolidated Product Design Specification](design/consolidated-design-draft.md), which remains **Consolidated Draft for Review**.
6. [Archived historical design evidence](archive/product-design-draft-v0.1.md).
7. Repository code as implementation evidence only.

Derived engineering documents, including this file, cannot override canonical specifications. Report a conflict rather than choosing a convenient interpretation.

## Documentation map

| Topic | Authoritative document | Relevant heading or section | Derived engineering document, when useful |
|---|---|---|---|
| Product scope and exclusions | [Approved Product Model](canonical/product-model.md) | §2 Product definition; §17 First-release boundaries | — |
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

- The canonical product, information-architecture, interaction-state, and detailed-workflow baselines incorporate the approved July 23, 2026 UI/UX amendment.
- The current slice is the [visual navigation scaffold and Mock Single Image](implementation/current-slice.md). Its approved GUI forms and deterministic hardware-free backbone are complete and reviewed through `9b27313`, `7e953d9`, `5be26c2`, `12be7e3`, `67930e5`, `2dd088c`, `53dc6a7`, `0f16cc0`, `639a60e`, `c850a8a`, and `f39f1b8`.
- The user explicitly authorized a visual-only scaffold for every approved workspace with no speculative controls or behavior.
- The adopted first-two-round workflow is recorded in the [Visual Scaffold Two-Round Plan](implementation/visual-scaffold-two-round-plan.md): Round 1 uses one visual-scaffold writer; Round 2 splits design and minimal backbone into nonoverlapping worktrees after the interface is frozen.
- The active [page-composition review](reviews/page-composition-review.md) has been reconciled to the amendment; it remains a review artifact rather than independent authority.
- The earlier Single Image-only Qt Design Studio baseline at commit `188a649` is preserved as evidence, but its Capture composition, former smaller minimum, and former Hardware placement are superseded.
- A bounded work order naming exact files is still required before any form, wrapper, mock, test, or CMake input is edited.
- Validation for these visual rounds is intentionally proportional and user-led: avoid the full legacy/hardware test matrix and prefer manual Qt Design Studio review where practical.
- The Qt Design Studio designer/developer workflow and form-wrapper-controller boundary remain adopted implementation constraints.
- Qt Design Studio generator registration was committed at `2019cb0`, followed by the visual test-host fix at `26edf96`. Configure in `odss-v2-dbg`, the `Desktop_app_v2App` and `tst_ShellSingleImage` builds, `ShellSingleImage` CTest (1/1), and the offscreen event-loop smoke with no QML runtime warnings passed; the generated registration/build gate is closed.
- The slice remains open only for user-led overall visual validation in Qt Design Studio 2D view and Live Preview at 1600 × 900, maximized, and a larger 16:9 size, including focus and non-color cues. Automated GUI interaction is not accepted evidence: it was interrupted by user Escape, and its first launch lacked the MinGW runtime on `PATH`.
- The tracked worktree was clean at validation handoff, and the protected untracked `Desktop_app_v2.qmlproject.qtds` was preserved.
- P0-1 detector characterization and neutral-contract work remains accepted groundwork.
- Real Camera preview, TIFF capture, DAQ, Training, Live, Sequence Test, Results, persistence, and other production integrations remain later slices requiring explicit authorization.
