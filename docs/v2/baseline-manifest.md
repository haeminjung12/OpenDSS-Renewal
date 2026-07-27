# OpenDSS v2 controlled-document manifest

**Last verified:** 2026-07-26  
**Hash algorithm:** SHA-256

This manifest records provenance documents and the user-designated master specification. Only `docs/v2/design/consolidated-design-draft.md` is controlling; other entries are retained for traceability.

| Controlled document path | Status | Authority role | SHA-256 |
|---|---|---|---|
| `docs/v2/canonical/product-model.md` | Incorporated provenance | Product-model source consolidated into ODSS-DES-002; cannot override the master. | `797b97b235ea834a7ae27011360a8be13dedde53b81ee0f6dd67abe4212cf1fa` |
| `docs/v2/OpenDSS_v2_UIUX_Design_Amendment.md` | Incorporated provenance | UI/UX amendment consolidated into ODSS-DES-002; cannot override the master. | `70f7ed1bfa83bdbbadf36a514d710e878195e8820831524a333ac67dac63d2cb` |
| `docs/v2/canonical/information-architecture.md` | Incorporated provenance | Information-architecture source consolidated into ODSS-DES-002; cannot override the master. | `7e469dbb5f7c4a1f07a25496af2ac2b9cc5dcf286b949c688bc1c71ec668fb98` |
| `docs/v2/canonical/interaction-and-state.md` | Incorporated provenance | Interaction/state source consolidated into ODSS-DES-002; cannot override the master. | `23acb38f9bf9a5f3a1d59d004b75e5e54ec247781a2c97ee1fb9159d3498f8d1` |
| `docs/v2/canonical/detailed-workflows.md` | Incorporated provenance | Workflow source consolidated into ODSS-DES-002; cannot override the master. | `9b03b8af4552e3f5ac60c7c2a3c3057760637806b8b4bc93f16340826e4e2f1c` |
| `docs/v2/design/consolidated-design-draft.md` | **User-Designated Master Specification** | Single controlling product/design authority; canonical LF SHA-256 verified through `consolidated-design-lock.json`. | `fe186542b89c1ce4c10061643e97a362f1146241a4b2c99bc98b096355d8d359` |
| `docs/v2/archive/product-design-draft-v0.1.md` | Archived Draft v0.1 | Historical, non-normative evidence only. | `c34ca181c885061ceddce1b2a4f2457f52caaa20253a8f0bcd36343927939d9b` |

Changing the master requires explicit user approval and a same-change refresh of `consolidated-design-lock.json`. Silent editorial rewrites or re-hashing are prohibited.
