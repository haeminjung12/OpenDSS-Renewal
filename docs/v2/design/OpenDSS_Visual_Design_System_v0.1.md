# OpenDSS Visual Design System v0.1

**Status:** Corrected visual foundation for user approval  
**Approved:** July 24, 2026  
**Platform:** Windows desktop; resizable rectangular window; minimum 1600 × 900  
**Primary use:** Desk viewing with keyboard and pointer  
**Implementation status:** The approved component-sheet gate is complete. The shared-component rollout is implemented and technically validated across Capture/shell, Label, Sequence Viewer, Settings, Model Test, Library, Live, Sequence Test, Runs, and Train; user visual acceptance remains pending. The accepted GUI at commit `ea48ec3` remains the baseline beneath this bounded visual-only rollout.

This document controls the shared visual language and component appearance. It is subordinate to the Approved Product Model and the July 23 visual-review amendment. It does not change approved workflows, terminology, scientific behavior, state ownership, editability, public interfaces, or the location of Hardware Configuration.

## 1. Freeze boundary

The design-system phase freezes product structure and behavior, not every current pixel.

### Frozen

- The ten approved workspaces, navigation taxonomy, labels, and workflow order.
- Product terminology and approved prerequisite logic.
- Camera, DAQ, Dataset, Model, Training, Sequence, Live, Run, and persistence behavior.
- Which values are editable, read-only, locked, or unavailable.
- Approved application states, resource ownership, recovery behavior, and output arbitration.
- Existing exported QML aliases, properties, signals, state names, wrapper seams, and C++ ownership boundaries.
- Qt Design Studio ownership and generated-file boundaries.
- Hardware Configuration beneath the left navigation.

### Visually governed

- Color, typography, spacing, control geometry, iconography, focus treatment, button hierarchy, and surface hierarchy.
- Navigation styling, panel presentation, disclosure handles, scroll/reflow behavior, and visual grouping.
- Empty, disabled, warning, error, selected, running, and completed presentation.
- Directly adjustable panel proportions and enlarged-text behavior.

No component-system decision authorizes new handlers, persistence, backend calls, hardware calls, workflow branches, editable settings, or replacement interfaces.

## 2. Design direction

OpenDSS is a **modern scientific desktop workstation**: technical, precise, calm, professional, compact without crowding, familiar on Windows, and suitable for sustained laboratory use.

The reference blend is:

- NVIDIA Omniverse for workstation composition, a dominant workspace, docked configuration regions, and compact technical density.
- Roboflow Annotate for image-centered Label composition, class selection, navigation, zoom, and keyboard efficiency.
- Microsoft Fluent 2 for tokens, typography, interaction states, icons, Windows familiarity, and visible focus.
- Qt Quick Controls Basic as the eventual shared-control foundation, not as the visual direction itself.

OpenDSS is not a decorative SaaS dashboard, enlarged mobile interface, raw collection of default controls, full dark IDE, or gradient-heavy concept. The shell remains light; camera, image, and selected plot canvases may be dark.

## 3. Surface hierarchy

| Level | Role | Treatment |
|---|---|---|
| Application | Shell background | Light neutral gray |
| Workspace | Main page region | White or near-white |
| Panel | Workspace inspector, Hardware Configuration, cards | White with restrained boundary |
| Control | Inputs, buttons, selectors | Distinct neutral or action surface |
| Canvas | Camera, image, and appropriate plot surfaces | Dark navy |

Spacing and surface contrast perform most grouping. Repeated boxes, decorative shadows, glass effects, and gradients are not part of the system. Shadows are reserved for dialogs, menus, popovers, and temporary overlays.

## 4. Color tokens

Components consume semantic aliases, never unregistered raw colors.

### 4.1 Global palette

| Token | Value | Role |
|---|---:|---|
| `neutral.0` | `#FFFFFF` | Primary surface |
| `neutral.25` | `#F8FAFC` | Secondary surface |
| `neutral.50` | `#F3F6F9` | Application background |
| `neutral.100` | `#EEF3F7` | Hover and disabled surface |
| `neutral.200` | `#D7DEE7` | Subtle border and divider |
| `neutral.300` | `#AAB7C5` | Strong border |
| `neutral.500` | `#6B7785` | Placeholder and tertiary text |
| `neutral.600` | `#5D6978` | Secondary text |
| `neutral.900` | `#17202A` | Primary text |
| `canvas.900` | `#15283F` | Viewer canvas dark surface; not an application theme |
| `brand.50` | `#E7F0F8` | Selected background |
| `brand.100` | `#D5E6F3` | Strong selected background |
| `brand.600` | `#276DA3` | Primary action and focus |
| `brand.700` | `#236A9D` | Primary hover |
| `brand.800` | `#1E5D8F` | Primary pressed |

### 4.2 Status palette

| State | Foreground | Background |
|---|---:|---:|
| Success | `#1F7A55` | `#EAF6F0` |
| Warning | `#8A5A00` | `#FFF4D8` |
| Error | `#B42318` | `#FDECEC` |
| Information | `#2563A6` | `#E7F0F8` |

### 4.3 Class identity

Class identity remains independent of brand selection, focus, and status colors.

| Token | Foreground | Background | Persistent cue |
|---|---:|---:|---|
| `class.0` | `#075985` | `#E0F2FE` | `0` plus Class Name |
| `class.1` | `#9A3412` | `#FFEDD5` | `1` plus Class Name |
| `class.2` | `#6B21A8` | `#F3E8FF` | `2` plus Class Name |

Required semantic aliases cover application, surface, subtle, hover, selected, and canvas backgrounds; primary, secondary, placeholder, and inverse text; subtle, default, and focus borders; primary action rest/hover/pressed; status roles; and Class 0/1/2 identity. Color never carries state alone.

## 5. Typography

Use Segoe UI Variable, then Segoe UI, then the system UI fallback. The values below are the approved defaults at **Medium (100%) Text Size**.

| Token | Size / line height | Weight | Use |
|---|---:|---:|---|
| `type.pageTitle` | 22–24 / 30–32 px | 400 | Workspace title |
| `type.majorSection` | 16–18 / 22–26 px | 600 | Major panel or card group |
| `type.section` | 14–16 / 20–22 px | 600 | Disclosure and card heading |
| `type.body` | 16 / 20 px | 400 | Body text and standard control text |
| `type.label` | 15 / 18 px | 400 | Ordinary field and settings labels |
| `type.button` | 16 / 18–20 px | 600 | Button and segmented-control text |
| `type.caption` | 13 / 16–18 px | 400 | Supporting, warning, and metadata text |
| `type.status` | 13 / 16–18 px | 500 | Compact global status content |
| `type.metric` | 22–32 / context px | 600 | Metric values and prominent counters |

Rules:

- Use sentence case and left alignment for normal English content.
- Page titles are regular; section titles are semibold.
- Ordinary field and settings labels use the 15 px label token with approximately 18 px line height.
- Reserve smaller muted type for supporting, warning, status, or metadata text.
- Do not bold body copy merely to create hierarchy.
- Paths use body type; monospace is reserved for logs, code, and diagnostics.
- Text Size supports exactly **Small (80%)**, **Medium (100%)**, and **Large (125%)**, with Medium as the default.
- **200%** is retained as a validation-only stress condition and is not exposed as a selectable preference.
- SettingsRepository and the UI integration must expose only 80, 100, and 125. Before publication or persistence, legacy persisted values must normalize as 90 → 100 and 150/175/200 → 125. The UI must never silently expose an unsupported value.
- Containers scale with text; components reflow rather than clip.

## 6. Spacing, shape, and elevation

Use a four-pixel spacing base: `space.1` 4, `space.2` 8, `space.3` 12, `space.4` 16, `space.6` 24, `space.8` 32, and `space.12` 48 px.

| Token | Value | Use |
|---|---:|---|
| `radius.none` | 0 | Structural rails and full-bleed canvas |
| `radius.control` | 4 px | Inputs and buttons |
| `radius.panel` | 6 px | Cards and grouped panels |
| `radius.overlay` | 8 px | Dialogs, popovers, and menus |

Standard borders are 1 px. Focus borders and selected-navigation markers are 2 px. Docked regions and controls have no decorative shadow.

## 7. Control dimensions

Dimensions are content-driven minimums at 100% Text Size and grow with text.

| Component | Minimum |
|---|---:|
| Standard button | 32 px high |
| Primary workflow button | 36 px high |
| Icon button | 32 × 32 px |
| Text field, combo box, and spin box | 32 px high |
| Multiline text area | 72 px high |
| Navigation item | 34 px high |
| Disclosure header | 36 px high |
| Global status bar | 40 px high |
| Sticky action footer | 56 px high |
| Outer-panel disclosure rail | 24 px wide |

No fixed text-bearing height may clip at 125–200%.

## 8. Shell composition

```text
┌──────────────────────────────────────────────────────────────┐
│ Global status bar                                            │
├──────────────┬───────────────────────────────┬──┬────────────┤
│ Navigation   │                               │  │ Workspace  │
│              │           Workspace           │  │ inspector  │
│              │                               │  │            │
├──────────────┤                               │  │            │
│ Hardware     │                               │  │            │
│ Configuration│                               │  │            │
└──────────────┴───────────────────────────────┴──┴────────────┘
                                                   fixed rail
```

- **Hardware Configuration remains docked beneath the left navigation.** It is shell-owned, matches the adjustable navigation-column width, expands upward, and never becomes part of the workspace inspector.
- The right inspector remains workspace-specific and has no Workspace/Hardware mode switch.
- The left navigation, Hardware Configuration height, and workspace outer right panels remain directly adjustable within their approved bounds and reset on launch.
- Workspace outer right panels default to approximately 536 px at 100% Text Size, are not fixed, and retain their approved titles.
- The outer chevron is a fixed 28 × 36 px icon control with a 14 px glyph. It occupies the panel's right edge at vertical center, remains at the same screen position when expanded or collapsed, and does not grow with Text Size.
- Restored windows may use any aspect ratio at or above 1600 × 900; content fills the available height.
- At enlarged text, navigation and panels scroll independently, action rows wrap or stack, and no sticky footer obscures the focused control.

## 9. Interaction states and accessibility

Applicable components document Rest, Hover, Pressed, Focus, Selected, Disabled, Read-only, Loading, Success, Warning, and Error.

- OpenDSS has one light application theme. There is no dark mode, alternate application theme, or theme selector.
- Dark surfaces are limited to Camera, image, crop, and sequence-viewer canvases where image contrast requires them.
- Focus uses a visible 2 px ring. On dark viewer canvases, use a light outer ring with dark separation.
- Every action is reachable and operable by keyboard and pointer; Tab order follows visual reading order.
- Escape exits modal or temporary overlay contexts; there are no keyboard traps.
- Text contrast is at least 4.5:1; large text and component boundaries are at least 3:1.
- Status and readiness use color plus a second cue.
- Controls tolerate 30–40% text expansion and 200% Text Size.
- Motion, if later approved, communicates state, remains brief, and provides an instant reduced-motion path.

## 10. Component vocabulary

### Core controls

`AppButton`, `AppIconButton`, `AppTextField`, `AppTextArea`, `AppComboBox`, `AppSpinBox`, `AppCheckBox`, `AppSwitch`, `AppRadioGroup`, `AppTabs`, `AppAccordion`, `AppStatusBadge`, `AppInlineMessage`, `AppCard`, `AppEmptyState`, `AppDialog`, `AppTooltip`, and `AppProgressIndicator`.

Use `AppSwitch` for persistent operational ON/OFF state, including **Trigger Every Droplet** and **DAQ Output**. Use `AppCheckBox` for independent inclusion choices such as **Physical DAQ Output** in Sequence Test. Do not substitute one control type for the other.

### OpenDSS compositions

`AppStatusBar`, `AppStatusItem`, `AppNavigation`, `AppNavigationGroup`, `AppNavigationItem`, `AppPageHeader`, `AppWorkspaceInspector`, `AppInspectorRail`, `AppHardwarePanel`, `AppPropertyGrid`, `AppReadinessList`, `AppStickyActionFooter`, `AppCanvas`, `AppTransportBar`, `AppChartFrame`, and `AppClassSelector`.

Workspace files may compose shared components but may not redefine their colors, font sizes, radii, borders, or interaction states.

## 11. Implementation sequence

1. Approve this definition.
2. Approve the reference component design sheet.
3. Implement approved tokens and the minimum shared controls with immediate consumers through exact bounded work orders.
4. **Completed technically by `TRAIN-COMP-1` and corrected by `TRAIN-COMP-2`, with user visual acceptance still pending:** prove the initial bounded shared-component system in Train.
5. **Implemented and technically validated, with user visual acceptance pending:** roll the accepted shared controls out mechanically across Capture/shell, Label, Sequence Viewer, Settings, Model Test, Library, Live, Sequence Test, Runs, and Train.
6. Prove operational and dark-canvas visual behavior where applicable without changing functional ownership.
7. Refine workspace-specific composition only after the rollout is technically validated and visually accepted.

No mass QML implementation begins before the component sheet is visually accepted.

## 12. Acceptance criteria

- Applicable states are defined for every component.
- Light-surface and dark-canvas contrast and focus treatments are shown.
- Primary, secondary, tertiary, destructive, operational switch, and inclusion checkbox roles are unmistakable.
- No status relies on color alone.
- Keyboard focus is always visible.
- Text reflows without clipping at 200%.
- The right disclosure rail never overlaps content.
- Hardware Configuration remains under the left navigation and uses the shared visual language.
- No workspace introduces local colors, font sizes, radii, or interaction-state rules.
- No placeholder value is presented as factual application data.
- Frozen workflows and interfaces remain unchanged.

### Implemented shared-component rollout

`TRAIN-COMP-1` and `TRAIN-COMP-2` remain the technically validated initial proof. The following mechanical rollout extends that accepted implementation pattern without creating a complete component library:

- The implemented shared controls are exactly `AppButton`, `AppTextField`, `AppComboBox`, `AppAccordion`, `AppInspectorRail`, `AppTextArea`, `AppCheckBox`, `AppSwitch`, `AppSpinBox`, `AppRadioButton`, `AppProgressBar`, and `AppNavigationItem`.
- `Constants.qml` remains the semantic-token owner consumed by those controls and workspace compositions.
- Adoption covers Capture/shell, Label, Sequence Viewer, Settings, Model Test, Library, Live, Sequence Test, Runs, and Train.
- Existing IDs, property aliases, signals, state names and meanings, wrapper/controller seams, models, indices, values, checked state, visibility, enabled state, and functional ownership boundaries are preserved.
- Intentional specialized controls remain local: Label class-color buttons; Library selectable model rows; Capture's camera `RangeSlider` and camera-prompt Yes/No choices; and Sequence Viewer's frame slider and transport structure.
- New QML registration is produced through Qt Design Studio-generated CMake integration. `qds.cmake` remains unchanged.

Integrated validation passed with the Qt 6.11.1 MinGW/Ninja `Desktop_app_v2App` build and a five-second offscreen smoke run that remained alive; smoke output contained only the known scaffold anchor and binding warnings. User visual acceptance of the rendered all-workspace rollout remains pending; technical validation does not imply acceptance. The single light application theme, Hardware Configuration beneath the left navigation, Small/Medium/Large Text Size choices, separate 200% validation-only condition, and protected functional ownership remain unchanged.

## References

- [NVIDIA Omniverse interface](https://docs.omniverse.nvidia.com/composer/latest/interface.html)
- [Roboflow Annotate](https://docs.roboflow.com/annotate/use-roboflow-annotate)
- [Microsoft Fluent 2 color](https://fluent2.microsoft.design/color)
- [Microsoft Fluent 2 typography](https://fluent2.microsoft.design/typography)
- [Microsoft Fluent 2 iconography](https://fluent2.microsoft.design/iconography)
- [Microsoft Fluent 2 layout](https://fluent2.microsoft.design/layout)
- [Qt Quick Controls customization](https://doc.qt.io/qt-6/qtquickcontrols-customize.html)
- [WCAG 2.2](https://www.w3.org/TR/WCAG22/)
