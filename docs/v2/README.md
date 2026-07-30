# OpenDSS v2 documentation

This directory contains the current OpenDSS v2 product baseline, design review material, implementation guidance, and retained historical evidence. Start with [CONTEXT.md](CONTEXT.md) for the authority order and [current-slice.md](implementation/current-slice.md) for the work currently authorized.

## Directory map

| Directory | Purpose |
|---|---|
| [`canonical/`](canonical/) | Provenance sources incorporated into the master specification. |
| [`design/`](design/) | The controlling consolidated master specification and its lock record. |
| [`reviews/`](reviews/) | Active user-review artifacts. |
| [`implementation/`](implementation/) | Current slice, implementation sequence, and adopted engineering workflow records. |
| [`audits/`](audits/) | Repository evidence and reusable-core findings. |
| [`archive/`](archive/) | Superseded, non-normative historical documents retained for traceability. |

## Current documents

| Document | Role | Status |
|---|---|---|
| [Consolidated Product Design Specification](design/consolidated-design-draft.md) | Single master product and design specification controlling implementation, review, and validation. | **User-Designated Master Specification** |
| [Approved Product Model](canonical/product-model.md) | Incorporated product-decision provenance for the master. | Incorporated provenance |
| [Approved UI/UX Design Amendment](OpenDSS_v2_UIUX_Design_Amendment.md) | Incorporated UI/UX provenance for the master. | Incorporated provenance |
| [Information Architecture and Screen Inventory](canonical/information-architecture.md) | Incorporated information-architecture provenance for the master. | Incorporated provenance |
| [Interaction and Application-State Specification](canonical/interaction-and-state.md) | Incorporated interaction and state provenance for the master. | Incorporated provenance |
| [Detailed Workflow Specification](canonical/detailed-workflows.md) | Incorporated workflow and acceptance provenance for the master. | Incorporated provenance |
| [Page Composition Interpretation](reviews/page-composition-review.md) | Current page-by-page interpretation for user validation, including the amended Capture composition. | Active review artifact; not implementation authorization |
| [Current Slice](implementation/current-slice.md) | Defines the only currently authorized v2 work boundary. | Held for user design review |
| [Reuse-First Implementation Plan](implementation/reuse-first-plan.md) | Orders future engineering work packages and evidence gates. | Engineering guidance |
| [Qt Design Studio Adoption](implementation/qt-design-studio-adoption.md) | Records the adopted designer/form-wrapper-controller workflow. | Adopted implementation constraint |
| [Visual Scaffold Two-Round Plan](implementation/visual-scaffold-two-round-plan.md) | Authorizes the all-workspace visual scaffold, Round 1 single visual writer, and Round 2 design/backbone split. | User-authorized implementation workflow |
| [Reusable-Core Audit](audits/reusable-core-audit.md) | Records repository reuse evidence, risks, and recommendations. | Completed audit evidence |
| [Product Design Draft v0.1](archive/product-design-draft-v0.1.md) | Superseded design evidence. | Archived; non-normative |

## Authority order

1. The Consolidated Product Design Specification (`ODSS-DES-002`) is the single master.
2. Current-slice and derived documents may limit scope or preserve context but cannot override it.
3. Other specifications, amendments, archives, reviews, and repository code are provenance or evidence only.
4. If anything deviates, follow the master; if the master is ambiguous, inconsistent, missing, or unsafe, stop and clarify with the user.

See [baseline-manifest.md](baseline-manifest.md) for the current controlled-document paths, statuses, checksums, and change-control rule.
