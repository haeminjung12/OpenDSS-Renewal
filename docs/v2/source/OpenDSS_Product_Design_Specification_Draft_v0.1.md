# OpenDSS Product Design Specification

**Status:** Draft v0.1  
**Date:** 21 July 2026  
**Scope:** GUI, product visual system, interaction model, screen requirements, design handoff  
**Out of scope:** backend architecture, SDK integration, threading, data processing, build system, packaging, CI, and security design

> **Working-name notice:** “OpenDSS” is treated as the current working name. Public naming and wordmark approval remain open decisions and must be resolved before final release branding is frozen.

This document is the design source of truth for the OpenDSS renewal. It supersedes the earlier HTML prototype and handoff wherever it states a different requirement.

Normative terms: **MUST** = required; **SHOULD** = default unless documented otherwise; **MAY** = optional.

## 1. Design direction and baseline

### Retain

- Droplet-and-branch logo concept.
- Deep navy foundation.
- Royal blue and aqua brand accents.
- Light neutral application tone.
- Dark live-view canvas.
- Simple geometric icon language.

### Change

- Replace five persistent status chips with one concise system-status control and a details drawer.
- Remove repeated workspace labels and redundant global status.
- Separate class identity, selection, keyboard focus, review state, and exclusion.
- Increase operational typography to a 13–14 px balanced default.
- Use spacing and surface hierarchy instead of outlining every region.
- Use resizable and collapsible panes.
- Specify all relevant loading, empty, ready, running, disabled, degraded, success, and fault states.

The current three-pane Dataset structure is useful, but the class-colored borders conflict with selection and focus. The current Live screen has state contradictions, including an enabled sorting action while hardware dependencies are unavailable. The current Models screen needs a master-detail structure and clear distinctions among selected, active, valid, and compatible models.

## 2. Experience principles and users

### Primary users

| Role | Primary jobs | Design implication |
|---|---|---|
| Laboratory operator | Connect hardware, preview, sort, monitor safety and results | State clarity and safe primary actions take precedence over configuration density |
| Researcher / dataset reviewer | Inspect, label, exclude, compare, export | High-throughput keyboard review and responsive large collections are mandatory |
| Model developer | Import, train, validate, compare, activate | Provenance, compatibility, metrics, and active state must be visible |
| Maintainer / troubleshooter | Diagnose drivers, paths, runtime failures, logs | Concise operator messages must link to technical detail |

### Principles

1. **State before decoration.** Explain what the system can do now, what it cannot do, and why.
2. **One screen, one primary job.** Each workspace has one dominant purpose and one state-dependent primary action.
3. **Safe by default.** Hardware actuation is never implied or enabled without explicit prerequisites and visible state.
4. **Context near control.** Local state appears near the relevant task; global health is summarized once.
5. **Failure includes recovery.** State what happened, its impact, and the next step.
6. **Keyboard for repeated work.** Dataset review and high-frequency live operations support discoverable shortcuts.
7. **Progressive disclosure.** Advanced detector and hardware settings do not occupy the operational surface permanently.
8. **Neutral canvas, restrained brand.** Brand colors guide attention rather than decorate every panel.

## 3. Information architecture

```text
OpenDSS
├── Live
│   ├── Sorting
│   └── Capture mode
├── Data
│   ├── Datasets
│   └── Review
├── Models
│   ├── Library
│   ├── Train
│   └── Validate
├── Runs
│   ├── Results
│   ├── Exports
│   └── Logs
└── System
    ├── Camera
    ├── DAQ and trigger
    ├── Paths
    └── Diagnostics
```

The primary rail MUST support a compact icon mode and an expanded icon-plus-label mode. Tooltips and accessible names are required. Camera configuration remains in System but may be opened contextually from Live.

## 4. Application shell

| Region | Default requirement | Behavior |
|---|---|---|
| Navigation rail | 56 px compact; 208 px expanded | Persistent and user-selectable |
| Context header | 56 px | One page title, optional context, one system-status control |
| Primary workspace | Flexible | Receives most available area; no fixed desktop-coordinate layout |
| Context inspector | 340–440 px | Resizable and collapsible |
| Command bar | 56–72 px where operational | One state-dependent primary action |
| System-status drawer | 420–520 px overlay | Component details and recovery actions |

The header does not show five equal-weight component chips. It shows a summary such as `Offline setup · Camera and DAQ not connected` or `Sorting · 224 fps · Trigger armed`.

A dependent feature is not labeled as failed merely because a prerequisite is unavailable. When DAQ is absent, trigger output is **unavailable**, not **failed**, unless an attempted operation actually failed.

## 5. Visual design system

### Brand colors

The vector masters use `#0B1F52`; the supplied board lists `#0B1F5E`. This specification adopts `#0B1F52` as canonical unless the vectors are deliberately regenerated.

| Token | Hex | Use | Rule |
|---|---:|---|---|
| `brand.navy.900` | `#0B1F52` | Logo, headings, structure | Primary brand dark |
| `brand.blue.600` | `#2563EB` | Primary action, focus, links | Do not use as both class and selection |
| `brand.teal.500` | `#14B8A6` | Brand and flow accent | Not a default success fill with white text |
| `brand.sky.300` | `#7DD3FC` | Tint and chart support | Not normal text |
| `neutral.slate.400` | `#94A3B8` | Disabled and nonessential metadata | Not normal text on white |
| `neutral.slate.200` | `#E2E8F0` | Dividers | Not the only cue for an active control |

### Light semantic colors

| Token | Hex | Use |
|---|---:|---|
| `canvas` | `#F4F7FB` | Application background |
| `surface` | `#FFFFFF` | Panels and inspectors |
| `surface.subtle` | `#F8FAFC` | Rows, empty states, hover |
| `text.primary` | `#0F172A` | Body and data |
| `text.secondary` | `#475569` | Labels and descriptions |
| `text.muted` | `#64748B` | Low-priority metadata |
| `border.subtle` | `#E2E8F0` | Dividers |
| `action.primary` | `#2563EB` | Primary action and focus |
| `state.ready` | `#0F766E` | Connected and ready |
| `state.success` | `#15803D` | Completed outcome |
| `state.warning` | `#B45309` | Degraded but operable |
| `state.error` | `#B91C1C` | Fault or failed operation |
| `state.armed` | `#7C3AED` | Actuation-ready trigger state |

### Typography

| Role | Typeface | Balanced size |
|---|---|---|
| Page title | Inter Semi Bold | 20–22 px / 28–30 |
| Section title | Inter Semi Bold | 14–16 px / 20–24 |
| Body / control | Inter Regular or Medium | 13–14 px / 19–21 |
| Caption / metadata | Inter Regular | 11–12 px / 16–18 |
| Metric value | Inter Semi Bold with tabular figures | 22–32 px |
| Path / log / code | Monospace fallback | 12–13 px / 18 |

Monospace is reserved for data that benefits from fixed-width alignment; it is not a general status style.

### Spacing, density, and geometry

- Base spacing: `4, 8, 12, 16, 20, 24, 32` px.
- Compact control/row: 32 px and 12 px body.
- Balanced default: 36 px and 13 px body.
- Comfortable: 40 px and 14 px body.
- Buttons and fields: 6 px radius.
- Panels: 8 px radius.
- Drawers/dialogs: 10–12 px radius.
- Standard borders: 1 px.
- Shadows: floating overlays only.

### Icons and logo

- Standard icon sizes: 16, 20, and 24 px.
- Default click targets: 32–36 px or larger.
- Ambiguous icons require tooltips and accessible names.
- A dedicated micro-mark is required for 16–20 px use.
- The full dotted logo is not mechanically scaled to 8 px.
- The dotted wave graphic is reserved for splash, documentation, release, and presentation surfaces.

## 6. Component system

A screen MUST be assembled from shared components. One-off button, field, status, panel, and thumbnail styles are defects unless explicitly approved.

### Core controls

| Component | Variants / states | Required behavior |
|---|---|---|
| Button | Primary, secondary, ghost, destructive; hover, pressed, focus, disabled, busy | One primary action per local state; disabled reason available |
| Icon button | Neutral, selected, destructive | 32–40 px target; tooltip and accessible name |
| Segmented control / tabs | Default, selected, focus, disabled | Mutually exclusive peer views; avoid nested tab bars |
| Text / number field | Default, hover, focus, invalid, disabled, read-only | Persistent label, aligned units, nearby validation |
| Path field | Editable/read-only, Browse, Reveal | Middle ellipsis; full value inspectable |
| Combo box | Default, open, selected, invalid, disabled | Current value preserves discriminating text |
| Checkbox / toggle | Off, on, mixed, focus, disabled | Toggle for immediate binary settings; checkbox for independent options |

### Content and feedback

Required shared components include `Panel`, `Inspector`, `ListRow`, `DataTable`, `DatasetThumbnail`, `StatTile`, `Progress`, `LogStream`, `StatusBadge`, `SystemStatusButton`, `EmptyState`, `Toast`, `InlineMessage`, `Dialog`, `Drawer`, `CommandBar`, `Splitter`, and `Tooltip`.

Dataset thumbnails use:

- Royal-blue focus/selection ring.
- Small text/symbol class badge.
- Check indicator for reviewed.
- Muted overlay plus X for excluded.
- Neutral default border.
- Optional model suggestion subordinate to the committed label.

## 7. Status, state, and messaging

| State | Meaning | Color |
|---|---|---:|
| Neutral | Not configured, not required, or inactive | `#64748B` |
| Information | Known alternate mode or context | `#2563EB` |
| Ready | Dependency connected and configured | `#0F766E` |
| Active | Operation running | `#15803D` |
| Armed | Hardware can actuate | `#7C3AED` |
| Warning | Can continue with a limitation | `#B45309` |
| Fault | Requested operation failed or is unsafe | `#B91C1C` |

Messages should communicate:

1. What happened.
2. Impact.
3. Recovery action.
4. Link to technical detail when necessary.

Start Sorting MUST remain disabled until camera preview, model, target, output path, and required DAQ conditions are valid. The disabled reason identifies the unmet prerequisite.

## 8. Workspace specifications

### Live

- Flexible viewer, contextual inspector, and operational command bar.
- Preflight before a run; meaningful metrics during a run.
- Camera settings and detector tuning use contextual drawers.
- State-dependent primary actions:
  - Camera absent → Open hardware setup.
  - Camera ready → Start preview.
  - Preview running but pipeline incomplete → Resolve pipeline.
  - Preview and pipeline ready → Start sorting.
  - Sorting → Stop sorting.
  - Fault → Retry or acknowledge, with safe hardware state shown.
- Default shortcuts: `Space` start/stop, `S` snapshot, `F` fit, `1` actual pixels.
- Force Trigger remains disabled unless explicitly armed and permitted by the state machine.

### Data and Dataset Review

- Left pane 260–320 px, flexible gallery, right inspector 340–440 px.
- Side panes are resizable, collapsible, and persisted.
- Adaptive, virtualized thumbnail grid.
- Label changes autosave and are immediately undoable.
- Default shortcuts: `1` Empty, `2` Single, `3` Non-target B, `X` Exclude, `U`/`Ctrl+Z` Undo, arrows previous/next, `Space` fit/actual size.
- Inspector supports fit, 1:1, zoom, pan, metadata, committed label, review state, and optional model suggestion.

### Models

- Master-detail library.
- Distinguish selected, active, valid, compatible, and provenance states.
- Import is the principal library action.
- Make Active and Validate are contextual.
- Remove is in overflow and requires confirmation.
- Train uses stages: dataset → starting model → compute/configuration → progress/result.
- Validate shows progress, metrics, confusion matrix, and misclassified review.

### Runs and System

- Runs: searchable history with date, mode, model, counts, status, path, exports, and logs.
- System: Camera, DAQ and trigger, Paths, and Diagnostics.
- Operator summaries remain concise; technical detail remains available.

## 9. Interaction and keyboard

- Visible focus on every interactive element.
- Tab order follows visual and task order.
- Repeated workflows expose discoverable shortcuts.
- Global shortcuts do not fire while typing.
- Pane resizing has a keyboard or explicit collapse/reset alternative.
- Loading, empty, filter-empty, disabled, recoverable-error, and fatal/unsafe states each have distinct content requirements.
- Dataset changes autosave and support undo.
- Destructive actions state scope and consequence; the safe option receives default focus.
- Motion is functional and restrained; respect reduced motion.

## 10. Responsive, high-DPI, and accessibility

### Target matrix

- Minimum supported window: 1280×720.
- Primary design width: 1600×900.
- Wide reference: 1920×1080.
- Windows scaling: 100%, 125%, 150%, 200%.
- Wide: persistent three-pane layouts where useful.
- Standard: inspectors may collapse.
- Below 1280 px: diagnostic/condensed only unless explicitly approved.

### Accessibility baseline

- Normal text: at least 4.5:1 contrast.
- Meaningful non-text controls and focus: at least 3:1.
- Pointer targets: at least 24×24 logical pixels; balanced default 32–36 px or larger.
- Color is not the only state, class, error, armed, or selection cue.
- Icon-only controls have accessible names.
- Core workflows remain keyboard operable.
- Text and controls do not clip at 200% system scaling.

Brand teal `#14B8A6` and sky `#7DD3FC` remain brand/support colors but are not used with white text for ordinary controls.

## 11. Qt Design Studio authoring contract

- Major screens have editable `.ui.qml` form files.
- Behavior lives in wrapper `.qml` and C++ view models.
- Shared components are used across screens.
- Theme tokens are centralized; no screen-local literal colors or arbitrary geometry values.
- Mock data covers all required design states.
- A component gallery exposes light/dark, density, and state variants.
- A non-programmer must be able to move the Live command bar, change inspector width, replace an icon, adjust panel padding, alter text, and preview representative states without editing C++.

Suggested design structure:

```text
docs/design/
  PRODUCT_DESIGN_SPEC.md
  decisions/
  screen-specs/
  accessibility/
branding/
  source/
  exports/
ui/
  Theme/
  Components/
  Screens/
  MockData/
```

## 12. Design QA and deliverables

### Acceptance evidence

- Layout screenshots at 1280×720, 1600×900, and 1920×1080.
- Scaling evidence at 100%, 125%, 150%, and 200%.
- Required states for each screen.
- Keyboard, tab order, and visible focus verification.
- Contrast, target size, non-color cue, accessible-name, and enlarged-text checks.
- Virtualized large data views.
- Hardware safety preconditions and fallback verification.
- Qt Design Studio form files open without errors.

### Deliverables

- Repository Markdown and editable review copy of this specification.
- Qt Design Studio component gallery.
- Screen forms for Live, Dataset, Models, Train, Validate, Runs, and System.
- Design-time mock data.
- Canonical design-token source.
- Approved SVG icon set and deterministic app-icon/favicon exports.
- Visual regression references and design QA checklist.
- Design decision records.

### Decision register

| ID | Status | Decision |
|---|---|---|
| D-001 | Adopted | Retain the droplet/branch concept and navy/blue/teal tone |
| D-002 | Adopted | Use `#0B1F52` as canonical navy unless vectors are regenerated |
| D-003 | Adopted | One system-status summary and drawer |
| D-004 | Adopted | Five primary areas: Live, Data, Models, Runs, System |
| D-005 | Adopted | Separate selected, active, valid, compatible, reviewed, excluded, and class states |
| D-006 | Adopted | Make visual changes Qt Design Studio-editable through forms, components, tokens, and mocks |
| O-001 | Open | Final public product name and wordmark |
| O-002 | Open | Full dark theme or limited dark viewer/splash surfaces |
| O-003 | Open | Final class names, symbols, and color rules |
| O-004 | Open | Support policy below 1280 px |
| O-005 | Open | Capture mode within Live or separate subpage |

A separate engineering architecture specification should define service boundaries, threading, data models, hardware adapters, model/training orchestration, repository structure, testing, packaging, and migration.

## References

1. [Current OpenDSS repository](https://github.com/haeminjung12/OpenDSS_clean)
2. [HTML GUI redesign reference](https://haeminjung12.github.io/opendss-gui-redesign/)
3. [GUI redesign repository and prior handoff](https://github.com/haeminjung12/opendss-gui-redesign)
4. [Qt Design Studio designer–developer workflow](https://doc.qt.io/qtdesignstudio/studio-designer-developer-workflow.html)
5. [Qt Design Studio UI controls guidance](https://doc.qt.io/qtdesignstudio/quick-controls.html)
6. [WCAG 2.2: Contrast (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum)
7. [WCAG 2.2: Non-text Contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html)
8. [WCAG 2.2: Target Size (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum)
9. [WCAG 2.2: Focus Appearance](https://www.w3.org/WAI/WCAG22/Understanding/focus-appearance.html)
10. [WCAG 2.2: Resize Text](https://www.w3.org/WAI/WCAG22/Understanding/resize-text.html)
