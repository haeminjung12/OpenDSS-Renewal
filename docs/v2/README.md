# OpenDSS v2 documentation

This directory contains the current OpenDSS v2 product baseline, design review material, implementation guidance, and retained historical evidence. Start with [CONTEXT.md](CONTEXT.md) for the authority order and [current-slice.md](implementation/current-slice.md) for the work currently authorized.

## Directory map

| Directory | Purpose |
|---|---|
| [`canonical/`](canonical/) | Controlling product, information-architecture, interaction-state, and detailed-workflow specifications. |
| [`design/`](design/) | Current consolidated visual/component design draft. It remains under review. |
| [`reviews/`](reviews/) | Active user-review artifacts. |
| [`implementation/`](implementation/) | Current slice, implementation sequence, and adopted engineering workflow records. |
| [`audits/`](audits/) | Repository evidence and reusable-core findings. |
| [`archive/`](archive/) | Superseded, non-normative historical documents retained for traceability. |

## Current documents

| Document | Role | Status |
|---|---|---|
| [Approved Product Model](canonical/product-model.md) | Controls D-001 through D-019, product scope, terminology, configuration boundaries, and product-state ownership. | Approved product model |
| [Approved UI/UX Design Amendment](OpenDSS_v2_UIUX_Design_Amendment.md) | Controls every UI, layout, naming, interaction, and workflow matter it explicitly changes. | Approved amendment |
| [Information Architecture and Screen Inventory](canonical/information-architecture.md) | Controls navigation, workspaces, shell composition, and screen inventory with the interaction-state specification. | Current canonical baseline |
| [Interaction and Application-State Specification](canonical/interaction-and-state.md) | Controls interactions, application states, locks, resource ownership, and contextual recovery with the information architecture. | Current canonical baseline |
| [Detailed Workflow Specification](canonical/detailed-workflows.md) | Supplies nonconflicting scientific, artifact, persistence, recovery, and acceptance requirements. | Current canonical baseline |
| [Consolidated Product Design Specification](design/consolidated-design-draft.md) | Visual, component, accessibility, and design-handoff guidance subordinate to the canonical baseline. | **Consolidated Draft for Review** |
| [Page Composition Interpretation](reviews/page-composition-review.md) | Current page-by-page interpretation for user validation, including the amended Capture composition. | Active review artifact; not implementation authorization |
| [Current Slice](implementation/current-slice.md) | Defines the only currently authorized v2 work boundary. | Held for user design review |
| [Reuse-First Implementation Plan](implementation/reuse-first-plan.md) | Orders future engineering work packages and evidence gates. | Engineering guidance |
| [Qt Design Studio Adoption](implementation/qt-design-studio-adoption.md) | Records the adopted designer/form-wrapper-controller workflow. | Adopted implementation constraint |
| [Visual Scaffold Two-Round Plan](implementation/visual-scaffold-two-round-plan.md) | Authorizes the all-workspace visual scaffold, Round 1 single visual writer, and Round 2 design/backbone split. | User-authorized implementation workflow |
| [Reusable-Core Audit](audits/reusable-core-audit.md) | Records repository reuse evidence, risks, and recommendations. | Completed audit evidence |
| [Product Design Draft v0.1](archive/product-design-draft-v0.1.md) | Superseded design evidence. | Archived; non-normative |

## Authority order

1. The Approved Product Model controls D-001 through D-019 and the product model.
2. The Approved UI/UX Design Amendment controls every UI, layout, naming, interaction, and workflow matter it explicitly changes.
3. The Information Architecture and the Interaction and Application-State Specification jointly control the shell, navigation, workspaces, interactions, application states, resource ownership, locks, and contextual recovery.
4. The Detailed Workflow Specification applies only where it does not conflict with the documents above.
5. The Consolidated Product Design Specification is visual guidance under review and cannot override the canonical baseline.
6. Archived documents and repository code are evidence only, not v2 product authority.

See [baseline-manifest.md](baseline-manifest.md) for the current controlled-document paths, statuses, checksums, and change-control rule.
