# OpenDSS v2 controlled-document manifest

**Last verified:** 2026-07-23  
**Hash algorithm:** SHA-256

This manifest records the current paths and working-tree checksums after the documentation reorganization and the amended D-014 baseline updates. It does not elevate draft or derived documents.

| Controlled document path | Status | Authority role | SHA-256 |
|---|---|---|---|
| `docs/v2/canonical/product-model.md` | Approved product model | Authority level 1; sole owner of D-001 through D-019 and controlling product model. | `797b97b235ea834a7ae27011360a8be13dedde53b81ee0f6dd67abe4212cf1fa` |
| `docs/v2/OpenDSS_v2_UIUX_Design_Amendment.md` | Approved UI/UX amendment | Controls every UI, layout, naming, interaction, and workflow matter it explicitly changes. | `70f7ed1bfa83bdbbadf36a514d710e878195e8820831524a333ac67dac63d2cb` |
| `docs/v2/canonical/information-architecture.md` | Current canonical baseline | Authority level 2; controls shell, navigation, workspaces, and screen inventory with the interaction-state specification. | `7e469dbb5f7c4a1f07a25496af2ac2b9cc5dcf286b949c688bc1c71ec668fb98` |
| `docs/v2/canonical/interaction-and-state.md` | Current canonical baseline | Authority level 2; controls interactions, states, resource ownership, locks, and contextual recovery with the information architecture. | `23acb38f9bf9a5f3a1d59d004b75e5e54ec247781a2c97ee1fb9159d3498f8d1` |
| `docs/v2/canonical/detailed-workflows.md` | Product workflow baseline | Authority level 3; supplies nonconflicting scientific, artifact, persistence, recovery, and acceptance requirements. | `9b03b8af4552e3f5ac60c7c2a3c3057760637806b8b4bc93f16340826e4e2f1c` |
| `docs/v2/design/consolidated-design-draft.md` | **Consolidated Draft for Review** | Authority level 4; visual/component guidance subordinate to the canonical product and interaction baselines. | `b5b38ef1e057331cb35ebae1d17399798a61ed04eef3bf24d8293def9bd9e229` |
| `docs/v2/archive/product-design-draft-v0.1.md` | Archived Draft v0.1 | Historical, non-normative evidence only. | `c34ca181c885061ceddce1b2a4f2457f52caaa20253a8f0bcd36343927939d9b` |

Changing a controlled document requires an intentional, separately reviewed change. Incidental authority changes, status elevation, and silent editorial rewrites remain prohibited. After an accepted controlled-document change, refresh this manifest in the same documentation change.
