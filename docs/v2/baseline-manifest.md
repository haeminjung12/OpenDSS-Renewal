# OpenDSS v2 controlled-document manifest

**Last verified:** 2026-07-26  
**Hash algorithm:** SHA-256

This manifest records provenance documents and the user-designated master specification. Only `docs/v2/design/consolidated-design-draft.md` is controlling; other entries are retained for traceability.

| Controlled document path | Status | Authority role | SHA-256 |
|---|---|---|---|
| `docs/v2/canonical/product-model.md` | Incorporated provenance | Product-model source consolidated into ODSS-DES-002; cannot override the master. | `9a01aa8079739728b855b405583dc92ced345e0324a0e0316d0ec4602bed1b46` |
| `docs/v2/OpenDSS_v2_UIUX_Design_Amendment.md` | Incorporated provenance | UI/UX amendment consolidated into ODSS-DES-002; cannot override the master. | `5c55e7b9b6cdce6a2065591bfe18435bc80e02963a84929f521e74c93ae3407f` |
| `docs/v2/canonical/information-architecture.md` | Incorporated provenance | Information-architecture source consolidated into ODSS-DES-002; cannot override the master. | `7e469dbb5f7c4a1f07a25496af2ac2b9cc5dcf286b949c688bc1c71ec668fb98` |
| `docs/v2/canonical/interaction-and-state.md` | Incorporated provenance | Interaction/state source consolidated into ODSS-DES-002; cannot override the master. | `4da0a0f663a901bbf09aeb35c92b3898abd0d187d4f800430c77e2667d7571c5` |
| `docs/v2/canonical/detailed-workflows.md` | Incorporated provenance | Workflow source consolidated into ODSS-DES-002; cannot override the master. | `ffd8fbd87669e15502df49d15cc36e014b396c9a9bea6e6a553063ee1779ad14` |
| `docs/v2/design/consolidated-design-draft.md` | **User-Designated Master Specification** | Single controlling product/design authority; canonical LF SHA-256 verified through `consolidated-design-lock.json`. | `1589c6a2a422549def25b220fa20621a44494c805017ce3c8aa08d0f1e18c7f3` |
| `docs/v2/archive/product-design-draft-v0.1.md` | Archived Draft v0.1 | Historical, non-normative evidence only. | `c34ca181c885061ceddce1b2a4f2457f52caaa20253a8f0bcd36343927939d9b` |

Changing the master requires explicit user approval and a same-change refresh of `consolidated-design-lock.json`. Silent editorial rewrites or re-hashing are prohibited.
