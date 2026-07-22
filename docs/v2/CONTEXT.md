# OpenDSS v2 implementation context

## Authority

Use this order when requirements differ:

1. [Approved Product Model](source/OpenDSS_v2_Approved_Product_Model.md).
2. [Information Architecture and Screen Inventory](source/OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md) and [Low-Fidelity Interaction and Application-State Specification](source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md).
3. Nonconflicting requirements in the [Detailed Workflow Specification](source/OpenDSS_Detailed_Workflow_Specification.md).
4. [Consolidated Product Design Specification](OpenDSS_v2_Consolidated_Product_Design_Specification.md), which remains **Consolidated Draft for Review**.
5. Historical design evidence.
6. Repository code as implementation evidence only.

Derived engineering documents, including this file, cannot override canonical specifications. Report a conflict rather than choosing a convenient interpretation.

## Documentation map

| Topic | Authoritative document | Relevant heading or section | Derived engineering document, when useful |
|---|---|---|---|
| Product scope and exclusions | [Approved Product Model](source/OpenDSS_v2_Approved_Product_Model.md) | §2 Product definition; §17 First-release boundaries | — |
| Navigation and workspace behavior | [Information Architecture and Screen Inventory](source/OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md) | §1 Application shell; §2 Navigation hierarchy; §3 Complete workspace inventory; §4 Workspace state inventory | — |
| Application state and resource locks | [Low-Fidelity Interaction and Application-State Specification](source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md) | §3 Conceptual application-state model; §17 Shared resource ownership and navigation effects | — |
| Fixed versus editable configuration | [Approved Product Model](source/OpenDSS_v2_Approved_Product_Model.md) | §11 Editable and fixed configuration; also IA §1.6 Shared Camera/DAQ drawer | — |
| Detector behavior | [Approved Product Model](source/OpenDSS_v2_Approved_Product_Model.md) plus nonconflicting [Detailed Workflow Specification](source/OpenDSS_Detailed_Workflow_Specification.md) | Product Model §7.1, §7.8, and §11.3; Workflow §13.6 Detection and crop rule | [Reusable-core audit](audits/OpenDSS_v2_Reusable_Core_Audit.md), findings F-A01 and F-A02 |
| Protected reusable assets | [Repository agent policy](../../AGENTS.md) | Protected reusable technical assets; Change-control rule for protected assets | [Reusable-core audit](audits/OpenDSS_v2_Reusable_Core_Audit.md) |
| Persistence/threading requirements | [Approved Product Model](source/OpenDSS_v2_Approved_Product_Model.md) plus nonconflicting [Detailed Workflow Specification](source/OpenDSS_Detailed_Workflow_Specification.md) | Product Model §15 Persistence and provenance; Workflow §31 Event persistence | [Reusable-core audit](audits/OpenDSS_v2_Reusable_Core_Audit.md), especially F-E02; [implementation plan](OpenDSS_v2_Reuse_First_Implementation_Plan.md), P0-2 |
| Qt Design Studio handoff | [Consolidated Product Design Specification](OpenDSS_v2_Consolidated_Product_Design_Specification.md), subordinate and under review | §24 Qt Design Studio and design-handoff contract | — |
| Current implementation work package | [Reuse-First Implementation Plan](OpenDSS_v2_Reuse_First_Implementation_Plan.md) | Current implementation sequence | [Current slice](implementation/current-slice.md) |

## Existing derived engineering documents

- [OpenDSS v2 Reusable-Core Architecture and Redundancy Audit](audits/OpenDSS_v2_Reusable_Core_Audit.md) contains repository evidence, reuse findings, risks, and recommendations.
- [OpenDSS v2 Reuse-First Implementation Plan](OpenDSS_v2_Reuse_First_Implementation_Plan.md) orders the engineering work packages and their evidence gates.

These documents provide engineering evidence and recommendations. They do not have product authority.

## Reading workflow

1. Read [AGENTS.md](../../AGENTS.md).
2. Read this file.
3. Read [current-slice.md](implementation/current-slice.md).
4. Open only the canonical sections referenced by the current slice.
5. Open a complete specification only when the task spans it or a conflict is unclear.

## Current status

- Design baseline frozen.
- Reusable-core audit completed.
- Reuse-first implementation plan completed.
- P0-1 detector characterization and neutral-contract work is completed and remains accepted groundwork.
- The Qt Quick/QML v2 Shell and Mock Single Image slice is next and remains planned pending implementation authorization.
- Real DCAM preview and TIFF capture are a separate later slice.
