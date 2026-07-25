# OpenDSS Component Design Sheet Specification

**Status:** Approved reference; all-workspace shared-component rollout implemented and technically validated, with user visual acceptance pending  
**Depends on:** [OpenDSS Visual Design System v0.1](OpenDSS_Visual_Design_System_v0.1.md)  
**Artifact:** One design reference sheet—not a workspace, QML implementation, or component gallery.

## 1. Boundary

The sheet defines shared-component anatomy, variants, states, wording, scale, and accessibility evidence before implementation.

It does not change approved workflows, terminology, editability, prerequisites, scientific behavior, resource ownership, public QML interfaces, Qt Design Studio boundaries, or generated files. It does not authorize handlers, controllers, persistence, backend integration, QML components, or gallery code.

## 2. Sheet grid

Use one zoomable 12-column canvas.

| Measure | Value |
|---|---:|
| Outer margin | 32 px |
| Column gutter | 16 px |
| Section gap | 32 px |
| Component-tile gap | 16 px |
| Tile padding | 16 px |
| Anatomy-label gap | 8 px |

Arrange full-width bands in this order:

```text
FOUNDATIONS
Color | Typography | Spacing | Radius | Borders | Icons

ACTIONS
Buttons | Icon buttons

INPUTS
Text field | Text area | Combo box | Spin box | Check box | Switch | Radio

STRUCTURE
Card | Accordion | Tabs | Navigation | Workspace inspector | Hardware panel

FEEDBACK
Status | Inline message | Empty state | Loading | Dialog | Tooltip

OPENDSS COMPOSITIONS
Property grid | Readiness list | Sticky footer | Transport bar | Chart frame | Class selector
```

Each component tile shows:

1. Name and purpose.
2. Anatomy callouts.
3. Minimum dimensions and alignment.
4. Variants.
5. Applicable states at identical scale.
6. Real OpenDSS wording.
7. Light-surface example.
8. Dark-canvas example when relevant.
9. Accessibility and approval notes.

## 3. Foundations

### Color

Show every global token beside its semantic alias. Include primary text/background pairs and success, warning, error, and information treatments with contrast results. Every status example combines color with an icon, label, marker, or shape.

### Typography

Show the complete approved typography ramp at Medium (100%):

- Page title: 22–24 / 30–32 px.
- Major section: 16–18 / 22–26 px.
- Section and accordion title: 14–16 / 20–22 px.
- Body and standard controls: 16 / 20 px.
- Ordinary field and settings labels: 15 / 18 px.
- Buttons: 16 / 18–20 px.
- Captions, status, warning, and metadata: 13 / 16–18 px.
- Metrics: 22–32 px as context requires.

The application Text Size selector shows exactly:

- **Small — 80%**
- **Medium — 100% (default)**
- **Large — 125%**

**200% is validation-only and must not appear in the application dropdown.**

SettingsRepository and the UI integration must expose only 80, 100, and 125. Before publication or persistence, legacy persisted values must normalize as 90 → 100 and 150/175/200 → 125. The UI must never silently expose an unsupported value.

Use:

- Capture
- Hardware Configuration
- Output Configuration
- Decision-to-trigger Delay (ms)
- Camera unavailable
- `C:/OpenDSS/Runs/Run-042`

Show one long label and one path across Small, Medium, and Large, plus a separate 200% validation-only reflow proof.

### Spacing, shape, borders, and focus

Show measured spacing tokens, the approved control/panel/overlay radii, 1 px default borders, 2 px focus borders, and the selected-navigation marker. Demonstrate focus on the light application surface and, only for viewer controls, on a dark image canvas.

### Icons

Use Fluent System Icons: 16 px in controls, 20 px in navigation and headings, and 12 px for noninteractive status detail. Show regular/rest and filled/selected examples. Icon-only actions require a tooltip and accessible name.

## 4. Core components

| Component | Variants | States | OpenDSS examples |
|---|---|---|---|
| `AppButton` | Primary, secondary, tertiary, destructive | Rest, hover, pressed, focus, disabled, loading | Start Training; Open Dataset; Undo; Delete Model |
| `AppIconButton` | Standard, canvas | Rest, hover, pressed, focus, disabled | Zoom in; Zoom out; Fit |
| `AppTextField` | Editable, read-only, path with trailing action | Rest, hover, focus, disabled, read-only, error | Model Name; Save Location |
| `AppTextArea` | Editable, read-only | Rest, focus, disabled, error | Notes |
| `AppComboBox` | Standard, long value | Rest, hover, open, focus, disabled, error | Architecture; Weights; Compute Device |
| `AppSpinBox` | Integer, unit-bearing | Rest, hover, focus, disabled, error | Amplitude (Vpp); Frequency (kHz); Event Duration (ms) |
| `AppCheckBox` | Unchecked, checked | Rest, hover, pressed, focus, disabled | Record full image sequence; Physical DAQ Output |
| `AppSwitch` | Off, on | Rest, hover, pressed, focus, disabled, read-only | Trigger Every Droplet; DAQ Output |
| `AppRadioGroup` | Two-option, three-option | Rest, hover, focus, selected, disabled | Component anatomy example; implement only when an approved consumer exists |
| `AppTabs` | Standard | Rest, hover, focus, selected, disabled | Overall Results; Per-Class Results |
| `AppAccordion` | Expanded, collapsed, disabled | Rest, hover, pressed, focus, disabled | Camera; DAQ; Dataset Summary |
| `AppStatusBadge` | Success, warning, error, information, neutral | Static; selected only if interactive | DAQ Ready; Camera unavailable; No Active Model |
| `AppInlineMessage` | Information, warning, error, success | Static; dismissible only when approved | Start requires an Active Model. |
| `AppCard` | Standard, selected, read-only | Rest, interactive hover/focus, selected, disabled | Selected Model; Dataset Summary |
| `AppEmptyState` | Neutral, unavailable, error | Static | No Dataset selected; No Runs found |
| `AppDialog` | Confirmation, warning, error | Initial focus, traversal, disabled action | Camera unavailable. Continue? |
| `AppTooltip` | Text, shortcut | Pointer and keyboard reveal | Fit to window; Zoom in |
| `AppProgressIndicator` | Determinate, indeterminate | Running, paused, completed, error | Processing 360 of 1,200 |

Component-sheet inclusion does not automatically authorize a production component file. Each later implementation requires a current consumer or an exact repository boundary.

### Boolean-control distinction

Show `AppCheckBox` and `AppSwitch` side by side:

- `AppSwitch` represents persistent operational ON/OFF state, including **Trigger Every Droplet** and **DAQ Output**.
- `AppCheckBox` represents independent inclusion or selection, including **Record full image sequence** and **Physical DAQ Output**.

Their anatomy and state presentation must remain distinct.

## 5. OpenDSS compositions

| Component | Anatomy and variants | Required wording |
|---|---|---|
| `AppStatusBar` / `AppStatusItem` | Icon, label, value, non-color cue; ready, active, unavailable, idle | Camera Streaming; DAQ Ready; Activity Idle |
| `AppNavigation` | Group labels, destination labels, selected marker; rest, hover, focus, selected, disabled | Data; Models; Sort; Results |
| `AppPageHeader` | Workspace title and approved action region; Small, Medium, Large, constrained, 200% validation-only | Capture; Sequence Viewer; Model Test |
| `AppWorkspaceInspector` | Top title strip, scroll body, stationary right-edge rail, optional sticky footer; expanded, collapsed, constrained, Small, Medium, Large, 200% validation-only | Train; Library; Runs |
| `AppInspectorRail` | Fixed 28 × 36 px right-edge chevron target with a 14 px glyph; vertically centered; expanded, collapsed, hover, focus, disabled; independent of Text Size | Open or close Train panel |
| `AppHardwarePanel` | Title, Camera disclosure, DAQ disclosure, nested Output Configuration; expanded, collapsed, unavailable, locked, overflow | Hardware Configuration; Camera; DAQ; Output Configuration |
| `AppPropertyGrid` | Label, value/control, supporting/error line; editable, read-only, locked, error | Output Channel; Decision-to-trigger Delay (ms) |
| `AppReadinessList` | Icon, requirement, status, direct reason; ready, warning, unavailable | Active Model — Ready; DAQ — Unavailable |
| `AppStickyActionFooter` | Primary plus approved secondary/destructive action; enabled, disabled with reason, running, stopping | Start Model Test; Stop; Browse |
| `AppCanvas` | Dark surface, content, overlay, focus; empty, loaded, unavailable, error | Camera preview; Selected Crop |
| `AppTransportBar` | Navigation, frame position, zoom; ready, disabled, focus sequence | −50; −10; Previous; Next; +10; +50; slider; frame entry |
| `AppChartFrame` | Title, plot, legend/status; empty, loading, populated, error | Training Loss / Validation Loss |
| `AppClassSelector` | Class actions and identities; two-class, three-class, selected, disabled | Class 0; Class 1; Class 2; Exclude |

`AppHardwarePanel` is always shown **beneath the left navigation**. It is not a right-inspector mode. Camera and DAQ are peers; Output Configuration is nested beneath DAQ. The right inspector remains workspace-specific.

## 6. State strips

Every applicable state appears at identical scale.

| State | Required evidence |
|---|---|
| Rest | Neutral default |
| Hover | Subtle surface or border change; no geometry shift |
| Pressed | Stronger surface treatment |
| Focus | Visible 2 px ring independent of fill |
| Selected | Accent tint plus marker, icon, or text cue |
| Disabled | Recognizable control using disabled tokens |
| Read-only | Information surface without editable affordance |
| Loading | Stable component width with progress cue |
| Success | Icon + label + semantic color |
| Warning | Icon + direct warning text + semantic color |
| Error | Border or icon plus actionable text |

Do not invent a state that frozen OpenDSS behavior cannot enter.

## 7. Light application theme and viewer-canvas examples

OpenDSS uses one light application theme. There is no dark mode, alternate application theme, or theme selector. Dark examples validate only components used on or over Camera, image, crop, and sequence-viewer canvases.

### Light cluster

- Start Training primary button.
- Open Dataset secondary button.
- Architecture combo box.
- Trigger Every Droplet `AppSwitch`.
- Camera unavailable inline warning.
- DAQ Ready status badge.
- Hardware Configuration disclosure group.

### Dark-canvas cluster

- Camera preview.
- Zoom icon buttons.
- Focused transport action with light outer ring and dark separation ring.
- Readable inverse status overlay.
- Warning and loading treatments that do not obscure canvas content.

Do not restyle ordinary inspector forms as dark surfaces.

## 8. Content rules

- Use exact OpenDSS terminology and sentence case.
- Use real wording, never `Button 1` or lorem ipsum.
- Label illustrative paths and values as examples; do not present mock data as fact.
- Prefer direct statuses: **Camera unavailable**, **DAQ Ready**, **No Active Model**.
- Name destructive objects: **Delete Model**, not **Delete**.
- Keep supporting text concise and subordinate.

## 9. Accessibility evidence

Annotate:

- Text contrast of at least 4.5:1.
- Component and large-text contrast of at least 3:1.
- Visible focus on light and dark surfaces.
- Logical Tab order for one representative form and one dialog.
- Icon/text or shape/text pairing for semantic states.
- Wrapping and content-driven growth at 200%.
- Labels and accessible names for icon-only controls.
- No focus obscured by scroll regions, sticky footers, or disclosure rails.

## 10. Approval checklist

### Foundations

- [ ] Palette and semantic aliases are complete.
- [ ] At Medium (100%), body and standard control text are 16 / 20 px, buttons are 16 / 18–20 px, ordinary field and settings labels are 15 / 18 px, and captions, status, warning, and metadata are 13 / 16–18 px.
- [ ] Spacing, radii, borders, focus, and icons match the design system.
- [ ] No unapproved color, type size, radius, or shadow appears.

### Components

- [ ] Every core component shows all applicable variants and states.
- [ ] `AppSwitch` is approved for operational ON/OFF controls.
- [ ] `AppCheckBox` remains visually and semantically distinct.
- [ ] Action hierarchy is unmistakable.
- [ ] Disabled and read-only are distinct.

### Composition

- [ ] Real OpenDSS wording is used.
- [ ] Hardware Configuration remains beneath the left navigation.
- [ ] Camera and DAQ are peers; Output Configuration is nested under DAQ.
- [ ] The right-edge inspector control is 28 × 36 px with a 14 px chevron, remains vertically centered, and never overlaps content.
- [ ] The single light application theme and viewer-canvas-only dark surfaces are approved.

### Accessibility and scale

- [ ] Contrast targets pass.
- [ ] Keyboard focus is visible.
- [ ] State never relies on color alone.
- [ ] Small, Medium, and Large examples preserve hierarchy and do not clip or hide actions.
- [ ] Medium is shown as the default selector value.
- [ ] 200% is absent from the selector and its separate validation example reflows without clipping or hidden actions.
- [ ] Long labels and paths wrap or elide only where appropriate.

### Freeze boundary

- [ ] No workflow, terminology, editability, prerequisite, or scientific behavior changed.
- [ ] No interface atom or ownership boundary changed.
- [ ] No implementation, gallery, backend, or persistence work is implied.

## 11. Approval gate

The original approval gate authorized creation of the rendered visual component sheet only. Separate acceptance of that sheet preceded the bounded Train proof and the later mechanical all-workspace rollout recorded below. Neither stage authorizes a component gallery, a complete production component library, backend behavior, or new product state.

## 12. Implemented shared-component rollout

The initial Train proof implemented `AppButton`, `AppTextField`, `AppComboBox`, `AppAccordion`, and `AppInspectorRail`. The mechanical rollout adds `AppTextArea`, `AppCheckBox`, `AppSwitch`, `AppSpinBox`, `AppRadioButton`, `AppProgressBar`, and `AppNavigationItem`. Their semantic visual foundation remains owned by `Constants.qml`.

Adoption covers Capture/shell, Label, Sequence Viewer, Settings, Model Test, Library, Live, Sequence Test, Runs, and Train. Existing IDs, aliases, signals, state names and meanings, models, indices, values, checked state, visibility and enabled bindings, wrapper/controller seams, and functional ownership boundaries remain intact.

Intentional specialized exceptions remain:

- Label class-color buttons;
- Library selectable model rows;
- Capture's camera `RangeSlider` and camera-prompt Yes/No choices;
- Sequence Viewer's frame slider and transport structure.

Qt Design Studio-generated CMake integration registers the shared QML files; `qds.cmake` remains unchanged. The earlier `TRAIN-COMP-1` and `TRAIN-COMP-2` targeted technical evidence remains valid for the initial Train proof, including the corrected standard-height `AppComboBox` delegate.

Integrated all-workspace validation passed with the Qt 6.11.1 MinGW/Ninja `Desktop_app_v2App` build and a five-second offscreen smoke run that remained alive; smoke output contained only the known scaffold anchor and binding warnings. User visual acceptance is pending. The rollout does not claim implementation of every component listed in this specification. The light-only application theme, Hardware Configuration beneath the left navigation, Small/Medium/Large Text Size selector, separate 200% validation-only condition, and component-sheet approval boundary remain controlling.
