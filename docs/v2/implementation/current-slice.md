# P0-1 — Detector Characterization and Neutral Contract

## Status

Planned — implementation not yet authorized by this documentation task.

## Objective

Characterize both current detector implementations and introduce the smallest neutral internal detector boundary needed by current consumers, without changing detector algorithms or current detector selection.

## Required reading

- [AGENTS.md](../../../AGENTS.md).
- [OpenDSS v2 implementation context](../CONTEXT.md).
- Approved Product Model: [§11 Editable and fixed configuration](../source/OpenDSS_v2_Approved_Product_Model.md#11-editable-and-fixed-configuration), [§15 Persistence and provenance](../source/OpenDSS_v2_Approved_Product_Model.md#15-persistence-and-provenance), and [§18 Product-state ownership boundary](../source/OpenDSS_v2_Approved_Product_Model.md#18-product-state-ownership-boundary).
- Interaction/Application-State specification: [§3.2 Authoritative owners](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#32-authoritative-owners), [§3.3 Authoritative versus derived state](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#33-authoritative-versus-derived-state), [§3.5 Operation lifecycle state machine](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#35-operation-lifecycle-state-machine), [§3.7 Configuration snapshot rule](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#37-configuration-snapshot-rule), and [§17 Shared resource ownership and navigation effects](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#17-shared-resource-ownership-and-navigation-effects).
- Reusable-core audit: [F-A01](../audits/OpenDSS_v2_Reusable_Core_Audit.md#f-a01--the-detectors-are-reachable-alternative-implementations-not-exact-duplicates) and [F-A02](../audits/OpenDSS_v2_Reusable_Core_Audit.md#f-a02--detector-contracts-state-and-tests-are-fragmented).
- Reuse-first implementation plan: [P0-1 Detector contract and consolidation](../OpenDSS_v2_Reuse_First_Implementation_Plan.md#p0-1--detector-contract-and-consolidation).

## Current facts

- `FastEventDetector` serves the current desktop workflows and the default CLI path.
- `EventDetector` serves the CLI precise path.
- Both implementations are reachable; reachability does not imply that both must remain permanently.
- `FastEventDetector` is the provisional production implementation.
- `EventDetector` is temporary comparison/reference code pending evidence.
- The intended end state is one neutral detector contract and one qualified production implementation.

## In scope

- Deterministic characterization tests and small replay-test support.
- Direct behavior tests for both implementations.
- One neutral detector contract or façade.
- The minimum wrappers or adapters required for existing consumers.
- Direct-versus-wrapper parity tests.
- Minimal build integration.

## Out of scope

- Deleting `EventDetector` or changing either algorithm.
- Changing thresholds, background processing, morphology, scaling, or hysteresis.
- DAQ or persistence refactoring, camera ownership changes, routing redesign, or application-state reconstruction.
- QML work.
- User-selectable detector strategies or user-editable detector settings.
- Broad cleanup.

## Lean implementation constraints

- Do not build a detector plugin system, registry, runtime strategy-selection framework, or generic scientific-processing framework.
- Do not expose detector choice to the UI.
- The neutral contract contains only values required by current consumers and tests.
- Hit Class, Hit/Waste Decision, Observed Route, model inference, and DAQ output do not belong in the detector contract.
- `FastEventResult::fired` must not be copied under an ambiguous name. Characterize its actual temporal meaning and represent it neutrally only when a current consumer requires it.
- Prefer thin wrappers over broad abstractions. Every new type and method must have a current use.
- Add no compatibility layer without a named removal condition.
- `EventDetector` remains temporary only until the required replay, timing, and hardware evidence has been reviewed.

## Tests first

Tests must cover:

- reset;
- background readiness;
- empty frame;
- one deterministic synthetic droplet;
- repeated frames;
- hysteresis or event re-entry;
- bounding box and centroid;
- mask behavior where applicable;
- 8-bit input;
- current 16-bit conversion behavior;
- deterministic replay;
- direct-versus-wrapper parity; and
- current temporal `fired` semantics.

Synthetic fixtures characterize software behavior; they do not prove scientific equivalence.

## Acceptance criteria

- Current desktop/default selection remains `FastEventDetector`.
- Current CLI precise selection remains `EventDetector`.
- Neither detector is deleted.
- One small neutral contract exists, with no product-routing or DAQ concepts.
- Both wrappers match their direct implementations on deterministic fixtures.
- Existing builds and tests pass.
- No unrelated production files change.
- Missing representative-sequence and hardware qualification remain explicit.
- No speculative or unused production code is introduced.

## Removal gate

Retire `EventDetector` only after:

- representative real-sequence replay is reviewed;
- required detection and crop behavior is retained;
- timing is acceptable;
- physical trigger qualification is completed; and
- CLI precise mode is confirmed unnecessary or migrated.
