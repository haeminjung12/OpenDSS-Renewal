# OpenDSS Visual Design System Decision Response and Handoff

**Date:** July 24, 2026  
**Status:** Corrected decision record for user approval  
**Responds to:** [OpenDSS Visual Design System v0.1](OpenDSS_Visual_Design_System_v0.1.md)

## 1. Final decisions

| Topic | Final decision |
|---|---|
| Typography | Text Size offers exactly **Small (80%)**, **Medium (100%, default)**, and **Large (125%)**. At Medium: page titles are 22–24 / 30–32 px; major sections 16–18 / 22–26 px; section and accordion titles 14–16 / 20–22 px; body and standard controls 16 / 20 px; ordinary field and settings labels 15 / 18 px; buttons 16 / 18–20 px; captions, status, warning, and metadata 13 / 16–18 px; metrics 22–32 px as context requires. **200% is validation-only and is not selectable.** |
| Theme | OpenDSS has one light application theme. There is no dark mode or theme selector. Dark surfaces are limited to Camera, image, crop, and sequence-viewer canvases. |
| Outer-panel toggle | The workspace outer-panel toggle is a fixed **28 × 36 px** icon control with a **14 px** chevron, fixed at the panel's right edge and vertically centered. It does not grow with Text Size. |
| Hardware placement | **Hardware Configuration remains docked beneath the left navigation.** It is shell-owned and is not a right-inspector mode. |

SettingsRepository and the UI integration must expose only 80, 100, and 125. Before publication or persistence, legacy persisted values must normalize as 90 → 100 and 150/175/200 → 125. The UI must never silently expose an unsupported value.
| Operational toggle | **`AppSwitch`** is the selected component name. It represents persistent operational ON/OFF state, including **Trigger Every Droplet** and **DAQ Output**. `AppCheckBox` remains reserved for independent inclusion or selection. |
| Component-sheet technology | The component sheet is a **Qt/QML artifact editable and reviewable in Qt Design Studio**. It must not be substituted with HTML. |
| Implementation baseline | The component-sheet approval gate is complete. The shared-component rollout is implemented and technically validated across all approved workspaces; user visual acceptance remains pending. Commit **`ea48ec3`** remains the baseline beneath this bounded visual-only rollout. |
| Current visual artifact | `component-sheet/ComponentDesignSheet.ui.qml` is the static visual-review artifact, with only design-sheet-local visual helpers. |
| Current implementation scope | Production scope is limited to twelve shared controls adopted mechanically across Capture/shell, Label, Sequence Viewer, Settings, Model Test, Library, Live, Sequence Test, Runs, and Train. No component-gallery route or executable, complete component library, backend integration, or new functional behavior is authorized. |

These decisions preserve the approved workspace inventory, navigation taxonomy, workflows, terminology, editability, resource ownership, scientific behavior, public QML interfaces, and Qt Design Studio ownership boundaries.

## 2. Reconciliation with the supplied proposal

The following proposal language is superseded:

1. The approved typography uses 16 / 20 px body and standard controls, 16 / 18–20 px buttons, 15 / 18 px ordinary field and settings labels, and 13 / 16–18 px captions, status, warning, and metadata.
2. Any proposal to move Hardware Configuration into a right-side inspector mode is rejected. The right inspector remains workspace-specific.
3. Operational ON/OFF examples use `AppSwitch`, not a checkbox or unnamed generic toggle.
4. Any HTML component-sheet or browser-gallery proposal is out of scope. The review artifact is Qt/QML in Qt Design Studio.
5. Design-system approval does not itself authorize production component implementation or workspace conversion.

All nonconflicting visual-system guidance remains applicable.

## 3. Controlling documents

### Design definition and sheet specification

- [OpenDSS Visual Design System v0.1](OpenDSS_Visual_Design_System_v0.1.md)
- [OpenDSS Component Design Sheet Specification](component-design-sheet-specification.md)
- This decision response

### Product and visual authority

The corrected Medium typography-token clauses and Small/Medium/Large Text Size choices are synchronized in:

- `docs/v2/canonical/product-model.md`
- `docs/v2/OpenDSS_v2_UIUX_Visual_Review_Amendment_2026-07-23.md`
- `docs/v2/OpenDSS_v2_UIUX_Design_Amendment.md`
- `docs/v2/canonical/information-architecture.md`
- `docs/v2/canonical/interaction-and-state.md`
- `docs/v2/canonical/detailed-workflows.md`

Those documents remain authoritative according to the repository authority order. This response records the design decision and handoff; it does not override higher-authority product or workflow requirements.

## 4. Component-sheet artifact

The visual-review artifact is:

- `docs/v2/design/component-sheet/ComponentDesignSheet.ui.qml`

Qt/QML screenshot evidence:

- [Medium component-sheet render](component-sheet/screenshots/component-sheet-regular-100.png)
- [Fixed outer-panel chevron proof](component-sheet/screenshots/component-sheet-panel-toggle-proof.png)
- [Text Size choices and 200% validation proof](component-sheet/screenshots/component-sheet-text-size-proof.png)

Any supporting files are confined to `docs/v2/design/component-sheet/` and exist only to present the approved static states.

The sheet is a deterministic Qt Design Studio review surface. It must show foundations, component anatomy, variants, applicable states, real OpenDSS wording, light-surface examples, dark-canvas examples, keyboard focus, disabled/read-only distinction, all three selectable Small, Medium, and Large Text Size choices, and a separate 200% validation-only proof.

The sheet may demonstrate shared components visually. It does not make those demonstrations production controls and does not create runtime interfaces or behavior.

## 5. Preserved implementation boundary

Component-sheet approval, the technically validated Train proof, and the implemented all-workspace mechanical rollout preserve these limits:

- commit `ea48ec3` remains the accepted GUI implementation baseline beneath the bounded visual-only rollout;
- adoption covers Capture/shell, Label, Sequence Viewer, Settings, Model Test, Library, Live, Sequence Test, Runs, and Train;
- the implemented shared-control set is limited to `AppButton`, `AppTextField`, `AppComboBox`, `AppAccordion`, `AppInspectorRail`, `AppTextArea`, `AppCheckBox`, `AppSwitch`, `AppSpinBox`, `AppRadioButton`, `AppProgressBar`, and `AppNavigationItem`, with semantic-token ownership in `Constants.qml`;
- no component gallery, alternate executable, or HTML preview is created;
- no existing property, alias, signal, state name, ID seam, wrapper connection, controller contract, or C++ owner is renamed or replaced;
- no QML calls hardware, persistence, filesystem services, vendor SDKs, or the trainer;
- new QML registration remains Qt Design Studio-generated, `qds.cmake` remains unchanged, and unrelated implementation files remain unchanged;
- protected Camera, DAQ, detector, ONNX, Training, export, and persistence mechanics remain untouched.

Intentional specialized controls remain local: Label class-color buttons; Library selectable model rows; Capture's camera `RangeSlider` and camera-prompt Yes/No choices; and Sequence Viewer's frame slider and transport structure.

Acceptance of the visual sheet and integrated technical validation do not constitute user visual acceptance of the rendered all-workspace rollout. User visual acceptance remains pending.

## 6. Functional handoff boundary

Design owns only the static appearance and review states of the component sheet. Functional work begins only through a later exact bounded work order.

The functional handoff must identify, for every implemented component:

1. The exact production file write set.
2. The existing public interface atoms that remain preserved.
3. Any proposed new interface atom, approved atomically with its wrapper or controller consumer.
4. The authoritative state owner.
5. Keyboard, accessibility, validation, and disabled/read-only behavior.
6. The absence of direct hardware, persistence, filesystem, or trainer calls from QML.
7. Proportional build and test evidence.
8. Qt Design Studio compatibility and generated-file boundaries.

A visual component must not invent application state, duplicate an authoritative owner, or silently add behavior. Wrapper, controller, C++, persistence, and hardware integration remain functional ownership.

## 7. Completed component-sheet approval gate

User visual approval of `ComponentDesignSheet.ui.qml` in Qt Design Studio completed the prerequisite gate for the later bounded Train proof.

Review covers:

- the approved Medium typography ramp, including 16 / 20 px body and standard controls, 16 / 18–20 px buttons, 15 / 18 px ordinary field and settings labels, and 13 / 16–18 px captions, status, warning, and metadata;
- the type, color, spacing, border, focus, and icon foundations;
- `AppSwitch` versus `AppCheckBox` semantics;
- applicable component variants and states;
- real OpenDSS wording;
- the single light application theme and viewer-canvas-only dark surfaces;
- keyboard focus and state cues that do not rely on color alone;
- Small, Medium, and Large Text Size presentation plus a separate 200% validation-only proof;
- Hardware Configuration beneath the left navigation, not as a right-inspector mode.

No mass implementation follows from that approval.

## 8. Post-approval sequence and shared-component rollout

The post-approval sequence remains:

1. Approved visual tokens — proven through `Constants.qml`.
2. Initial shared controls with immediate Train consumers — `AppButton`, `AppTextField`, `AppComboBox`, `AppAccordion`, and `AppInspectorRail`.
3. A **Train** representative-workspace proof — implemented and technically validated through `TRAIN-COMP-1` and `TRAIN-COMP-2`.
4. Additional immediately consumed controls — `AppTextArea`, `AppCheckBox`, `AppSwitch`, `AppSpinBox`, `AppRadioButton`, `AppProgressBar`, and `AppNavigationItem`.
5. Mechanical adoption across Capture/shell, Label, Sequence Viewer, Settings, Model Test, Library, Live, Sequence Test, Runs, and Train — implemented with interfaces and state meaning preserved.
6. Integrated technical build and smoke validation — passed with the Qt 6.11.1 MinGW/Ninja `Desktop_app_v2App` build and a five-second offscreen smoke run that remained alive; smoke output contained only the known scaffold anchor and binding warnings.
7. User visual acceptance of the rendered all-workspace rollout — pending.

Train is first because it exercises page hierarchy, inspector composition, disclosures, selectors, path fields, actions, readiness, charts, empty states, and disabled states without introducing Live hardware complexity.

No complete component library, gallery, new backend behavior, or change to functional ownership is authorized by this rollout.

### Preserved implementation evidence

- Existing aliases, states, wrapper seams, models, indices, values, checked state, visibility and enabled bindings, and functional ownership boundaries are preserved across the rollout.
- New QML registration was produced through Qt Design Studio-generated project integration.
- `qds.cmake` remains unchanged.
- The earlier Train proof's independent review, Qt 6.11.1 MinGW/Ninja build, and five-second offscreen smoke remain historical targeted evidence only.

`TRAIN-COMP-2` corrected the technically validated Train proof: `AppComboBox` now supplies a tokenized default standard-height `ItemDelegate`, and `trainingDeviceSelector` explicitly uses standard height. The Architecture and Weights specialized delegates and all interfaces and states remain unchanged. Independent review returned **PASS**, the incremental Qt 6.11.1 MinGW/Ninja build passed, and the five-second offscreen smoke remained alive. User visual acceptance remains pending.

Integrated all-workspace build and smoke validation passed. User visual acceptance remains pending. The rollout preserves the light-only application theme, Hardware Configuration beneath the left navigation, Small/Medium/Large Text Size selector, and separate 200% validation-only condition.
