# OpenDSS v2 design baseline

The commit containing this directory is the version-controlled OpenDSS v2 design baseline. The six documents below are frozen without editorial changes; only their repository filenames and paths were normalized.

## Baseline documents

| Document | Role | Status |
|---|---|---|
| [Approved Product Model](source/OpenDSS_v2_Approved_Product_Model.md) | Controls approved product decisions D-001 through D-019, product structure, scope, terminology, configuration boundaries, and product-state ownership constraints. | Approved product model |
| [Information Architecture and Screen Inventory](source/OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md) | Controls the current shell, navigation, workspaces, interactions, application states, resource ownership, locks, and contextual recovery together with the low-fidelity specification. | Consolidated OpenDSS v2 design baseline |
| [Low-Fidelity Interaction and Application-State Specification](source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md) | Controls the current shell, navigation, workspaces, interactions, application states, resource ownership, locks, and contextual recovery together with the information-architecture inventory. | Interaction and state-definition baseline |
| [Detailed Workflow Specification](source/OpenDSS_Detailed_Workflow_Specification.md) | Supplies detailed scientific, artifact, persistence, recovery, and acceptance requirements that do not conflict with higher-authority documents. | Product workflow baseline |
| [Consolidated Product Design Specification](OpenDSS_v2_Consolidated_Product_Design_Specification.md) | Current consolidated visual and component design baseline under review; it cannot override the approved product model or interaction baselines. | **Consolidated Draft for Review** |
| [Product Design Specification Draft v0.1](source/OpenDSS_Product_Design_Specification_Draft_v0.1.md) | Historical, non-normative design evidence only. It is not a source of truth for product behavior, navigation, application state, terminology, editable controls, or scope. | Draft v0.1; historical and non-normative |

## Source authority

Apply this authority order:

1. The Approved Product Model controls D-001 through D-019, product structure, scope, terminology, configuration boundaries, and product-state ownership constraints.
2. The Information Architecture and Screen Inventory and the Low-Fidelity Interaction and Application-State Specification control the current shell, navigation, workspaces, interactions, application states, resource ownership, locks, and contextual recovery.
3. The Detailed Workflow Specification supplies detailed scientific, artifact, persistence, recovery, and acceptance requirements that do not conflict with the documents above.
4. The Consolidated Product Design Specification is the current consolidated visual/component design baseline under review. It does not override the approved product model or interaction baselines, and its status remains **Consolidated Draft for Review**.
5. Product Design Specification Draft v0.1 is historical, non-normative design evidence only and is not a source of truth for product behavior, navigation, application state, terminology, editable controls, or scope.
6. Repository code is implementation evidence and a source of reusable technical components. It is not the authority for OpenDSS v2 product behavior, UX, product structure, terminology, product policy, or exposed configuration. Existing behavior is not retained merely because it is present in code.

See [BASELINE_MANIFEST.md](BASELINE_MANIFEST.md) for frozen paths, roles, statuses, checksums, and change control.
