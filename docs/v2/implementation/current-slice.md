# Qt Quick/QML v2 Shell and Mock Single Image

## Status

Planned — awaiting implementation authorization.

## Objective

Add a new Qt Quick/QML OpenDSS v2 executable beside the legacy application and implement the first deterministic UI slice:

- application shell and approved navigation;
- global status header, workspace host, and operation-side panel;
- shared Camera/DAQ drawer presentation;
- one mock authoritative application-state owner;
- a narrow fake Camera service; and
- the Data > Capture > Single Image visual workflow.

## Intended result

The new executable launches independently of laboratory hardware and opens at **Data > Capture > Single Image**. It demonstrates the approved v2 shell and Single Image states with deterministic mock data.

## Required reading

- [Repository agent policy](../../../AGENTS.md).
- [OpenDSS v2 implementation context](../CONTEXT.md).
- [Reuse-first implementation plan](../OpenDSS_v2_Reuse_First_Implementation_Plan.md).
- Approved Product Model: [§5 Global application shell](../source/OpenDSS_v2_Approved_Product_Model.md#5-global-application-shell), [§7.1 Data > Capture](../source/OpenDSS_v2_Approved_Product_Model.md#71-data--capture), and [§18 Product-state ownership boundary](../source/OpenDSS_v2_Approved_Product_Model.md#18-product-state-ownership-boundary).
- Information Architecture: [§1 Application shell](../source/OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md#1-application-shell), [§2 Navigation hierarchy](../source/OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md#2-navigation-hierarchy), and [§4.2 Data > Capture > Single Image](../source/OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md#42-data--capture--single-image).
- Interaction/Application-State specification: [§2 Application shell interaction](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#2-application-shell-interaction), [§3 Conceptual application-state model](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#3-conceptual-application-state-model), [§4 Shared interaction patterns](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#4-shared-interaction-patterns), [§5 Shared Camera/DAQ hardware drawer](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#5-shared-cameradaq-hardware-drawer), and [§6.4 Single Image](../source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md#64-single-image).
- Consolidated Design Specification, still **Consolidated Draft for Review**: [§4 Application shell](../OpenDSS_v2_Consolidated_Product_Design_Specification.md#4-application-shell), [§6.7 Component accessibility](../OpenDSS_v2_Consolidated_Product_Design_Specification.md#67-component-accessibility), [§9.2 Single Image](../OpenDSS_v2_Consolidated_Product_Design_Specification.md#92-single-image), [§23 Responsive, high-DPI, and accessibility requirements](../OpenDSS_v2_Consolidated_Product_Design_Specification.md#23-responsive-high-dpi-and-accessibility-requirements), and [§24 Qt Design Studio and design-handoff contract](../OpenDSS_v2_Consolidated_Product_Design_Specification.md#24-qt-design-studio-and-design-handoff-contract).

## In scope

- A separate Qt Quick/QML executable target and Qt Design Studio-compatible `.ui.qml` forms.
- Centralized minimal design tokens; shell layout; approved primary and secondary navigation; and startup at Data > Capture > Single Image.
- Global Camera, DAQ, Active Model, and Current Activity projections.
- Shared Camera/DAQ drawer visual states.
- Minimal mock authoritative state exposed to QML and a fake Camera service used only by this slice.
- Single Image Empty/Unavailable where applicable, Ready, transient Busy, Completed, and Failed presentations. Canonical Single Image defines no distinct Empty presentation, so the slice must not invent one unless an approved prerequisite makes Empty applicable.
- File Name and Save Location UI, with deterministic fake Capture Image results.
- A direct disabled reason and one contextual fault banner.
- Basic keyboard focus and supported-window checks.
- Tests appropriate to the shell and mock state.

## Out of scope

- Real DCAM integration, physical-camera testing, real TIFF capture, NI-DAQmx, detector integration, inference, training, and production persistence.
- Dataset Capture, Image Sequence, Label, Live, Sequence Test, or Results implementation.
- Replacing or deleting the legacy application or porting old Qt Widgets screens.
- Broad architecture frameworks or later-workspace placeholder buttons.

## Lean implementation constraints

- Do not build a plugin system, service locator, dependency-injection framework, generic workflow engine, or complete services for future workspaces.
- Create only state and interfaces consumed by this shell and mock Single Image slice; every production type and property must have an immediate consumer.
- UI state derives from one mock authoritative owner, not widget text or duplicated control-local truth.
- `.ui.qml` forms contain visual structure and state presentation; wrapper QML contains presentation behavior.
- QML must not call hardware or file APIs directly.
- Do not copy legacy Widgets navigation or application composition.
- Navigation may display approved destinations, but only Single Image receives functional workspace content.

## Required UI states

- Camera unavailable, connected, and streaming.
- Capture ready, fake capture in progress, fake capture completed with a displayed path, and fake capture failed.
- Hardware drawer open and closed.
- Direct disabled reason and contextual fault banner.
- No Active Model.
- Current Activity = `Idle` and `Capturing Image`.

## Verification

- Configure and build the new v2 executable, then launch it without Camera, DAQ, Python, or GPU.
- Verify the startup workspace, approved navigation, drawer behavior, and deterministic fake capture transitions.
- Verify no vendor SDK is called and no real file is presented as written.
- Open or validate the `.ui.qml` forms and check keyboard focus, 1280×720 minimum layout, and relevant scaling states.
- Run relevant automated tests and inspect changed files for unused or speculative code.
- Confirm the legacy executable and shared low-level code were not modified. The legacy executable need not be rebuilt unless shared existing code changes; this slice should avoid such changes.

## Acceptance criteria

- A separate v2 executable exists, launches without hardware, and starts at Data > Capture > Single Image.
- The approved shell and navigation are present without implementing unrelated workspaces.
- The global header is a projection of one state owner, and the shared drawer presents mock Camera and DAQ state.
- Single Image mock states work deterministically with no real hardware or production persistence.
- No speculative or unused framework code is introduced.
- The legacy application remains intact, and no later DCAM/TIFF work is included.

## Following slice

Record without authorization: connect the existing DCAM implementation to the v2 Camera boundary, display a real preview, capture one frame, save one real TIFF, and perform physical-camera qualification.
