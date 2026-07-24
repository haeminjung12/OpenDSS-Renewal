# OpenDSS v2 Low-Fidelity Interaction and Application-State Specification

**File:** `interaction-and-state.md`  
**Version:** 1.1  
**Date:** July 23, 2026  
**Status:** Interaction and state-definition baseline  
**Upstream baseline:** `information-architecture.md`  
**Primary authority:** *OpenDSS Approved v2 Product Model*, decisions D-001 through D-019  
**Secondary authority:** *OpenDSS Detailed User Workflow Specification*, for requirements that do not conflict with the approved product model

This document defines the next design layer after the approved information architecture: low-fidelity workspace composition, interaction behavior, control enablement, application-state projection, operation transitions, resource locks, and direct recovery behavior.

It does not reopen D-001 through D-019, alter the approved navigation, add product features, prescribe production code, or define polished visual styling. Text diagrams describe structure and interaction only.

---

## 0. Amendment-controlled interaction and state rules

The July 23, 2026 UI/UX amendment controls the following interaction and state behavior. Older detailed material in this document applies only where it agrees with these rules.

### 0.1 Startup, shell, and errors

OpenDSS starts maximized at `Data > Capture`, attempts Camera connection automatically, and shows `Camera unavailable. Continue?` once per session only when no Camera is available. This is a shell-global modal overlay above the entire shell and current workspace, with `Data > Capture` still selected beneath it. Yes continues without Camera and leaves ordinary unavailable status visible; No closes OpenDSS. Restored size is never below 1600 × 900, manual resizing is free to use any aspect ratio above that minimum, and content fills all available window space through the bottom.

The header remains a single line. Each Camera, DAQ, Active Model, and Current Activity item has icon, text, value, readiness color, and a non-color readiness cue. Hardware is docked at the bottom of the resizable left-navigation column, matches that column's width, expands upward when opened, and remains available subject to factual device readiness and operation arbitration.

Ordinary workspace failure presentation is `Error`. Logs own technical details and remain reachable through Settings > Diagnostics. Retry and Open Folder are allowed only when directly useful; paths and success states appear only after confirmed success.

### 0.2 Operation and disclosure states

- Capture section headings remain visible. Idle bodies collapse independently and may be open together. An active body is forced open; other headings remain visible and disabled until completion, interruption, or failure.
- Sequence Viewer has only Empty, Ready, and frame-navigation state; it has no frame-navigation lifecycle. Missing frames are skipped silently.
- Train has Ready, Training, Saving, Completed, and Save Failed states. There is no normal completed-but-unsaved state. Save Failed retains completed temporary artifacts and enables Retry Save; Active Model changes only after successful save.
- Live retains one workspace. Before sorting, Running is collapsed. Start collapses setup sections, expands Running, makes setup read-only, and retains the Camera preview. Pause/Resume and Stop act on the same Run.
- Results maintains separate `selectedRunId` and `loadedRunId`; selection alone never replaces the center Run.

### 0.3 Active Model ownership and locks

Model Test and Sequence Test consume the authoritative Active Model and expose no local Model selector. Library is the ordinary path to Set Active. While Model Test or Sequence Test uses the Active Model, replacement, rename, deletion, package mutation, and Set Active are blocked with one direct reason. Live is the explicit exception: its Active Model field remains editable while active, and a valid committed selection applies immediately through ModelRegistry and the effective-configuration-history contract; package mutation remains blocked while a package is in use.

### 0.4 Live decision, DAQ, and observed-route state

`triggerEveryDroplet` and `daqOutputEnabled` are independent booleans:

- Trigger Every Droplet OFF means Class-Based Sorting; ON makes every detected droplet Decision Hit while classification may still be logged.
- DAQ Output OFF permits Start when all non-DAQ prerequisites pass and retains processing, detection, classification when applicable, tracking, event logging, Run creation, and persistence without physical output.
- DAQ Output ON additionally requires DAQ Ready and sends physical output according to Decision.

Hit boundary calibration owns `boundary_y`, `hit_side`, `image_width`, and `image_height`. `Top is Hit` maps −Y to Hit and +Y to Waste; `Bottom is Hit` reverses that mapping. The boundary affects Observed Route only, never Predicted Class, Decision, or DAQ output. During Live, its fields remain editable and valid committed changes apply immediately under the effective-configuration-history contract.

### 0.5 Sequence Test loading and timing

Sequence Test accepts only an OpenDSS Image Sequence folder containing `sequence.json`. Selection shows a first-frame preview, frame count, recorded FPS, and status. Processing FPS defaults to recorded FPS, remains editable before Start, drives pipeline read scheduling, is snapshotted in the Run Summary, and governs physical DAQ timing when enabled.

Start stays disabled until bounded memory-buffer allocation succeeds. Allocation failure shows `Error`, logs details, cancels the load, and keeps Start disabled. A sequence larger than the buffer refills during processing. Physical DAQ Output is off by default and DAQ ownership is acquired only when it is enabled.

## 1. Design frame

### 1.1 Interaction objectives

OpenDSS v2 interactions shall make the following immediately apparent:

1. which workspace is open;
2. which artifact, model, or hardware is selected;
3. whether the requested operation can start;
4. the one direct reason when it cannot start;
5. which long-running operation currently owns shared resources;
6. whether data is being written, paused, finalized, completed, interrupted, or failed;
7. which direct action continues or recovers the workflow.

### 1.2 Interaction constraints

The interaction design preserves these boundaries:

- no Home screen;
- no mandatory wizard;
- no separate Sort Setup workspace;
- no duplicate Camera or DAQ settings inside workspaces or Settings;
- no editable droplet-detection, Droplet Crop, routing-algorithm, internal timing, or training-hyperparameter controls;
- no scientific approval, quality-gate, class-balance, model-suitability, or confidence-threshold interaction;
- no projects, accounts, cloud functions, or collaboration hierarchy;
- no managed Setup Profile library;
- no legacy migration or compatibility workflow;
- no Training or Model Test history in Results;
- no placeholder controls for excluded or deferred features.

### 1.3 Low-fidelity notation

The diagrams use the following notation:

```text
[ Action ]          enabled button
[ Action — disabled ]
[ x ]               checked checkbox
[   ]               unchecked checkbox
(●) / ( )           selected / unselected radio option
[ value ▼ ]         selector
[________________]  text or numeric field
<read-only>          factual value, not editable
┌──────────────┐     structural region
```

The diagrams do not define color, typography, iconography, animation, exact dimensions, or final visual hierarchy beyond the approved shell and the relative prominence stated here.

### 1.4 Interaction axioms

1. **Direct navigation remains available.** Contextual links preselect artifacts but never force a sequence of screens.
2. **One workspace owns the foreground interaction.** A long-running operation may continue after the user navigates elsewhere.
3. **One primary action is visually dominant in each current state.** Secondary actions remain available only when technically valid.
4. **Disabled actions state one direct reason.** OpenDSS does not show a separate readiness checklist.
5. **Technical blocking only.** Factual incompatibility, missing hardware, resource ownership, unreadable files, or unwritable output may block an action; scientific judgment may not.
6. **Ordinary file behavior.** Dataset, Sequence, Model Package, Setup Profile, Model Test output, and Run locations use standard Windows file or folder pickers.
7. **Stable scientific terminology.** Predicted Class, Decision, and Observed Route remain separate in all sorting-related presentations.
8. **State is authoritative, not copied into widgets.** The global header, enabled states, counters, locks, and workspace presentations derive from the same domain-state snapshot.

---

## 2. Application shell interaction

### 2.1 Shell layout

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Camera: <status> | DAQ: <status> | Active Model: <name> | Activity: <value> │
│                                                               [ Hardware ]  │
├────────────────┬──────────────────────────────────────┬──────────────────────┤
│ PRIMARY NAV    │ WORKSPACE REGION                     │ OPERATION PANEL      │
│                │                                      │                      │
│ Data           │ Preview, crop grid, sequence frame,  │ Inputs, selection,   │
│ Models         │ metrics, model list, Run content,    │ progress, counters,  │
│ Sort           │ or Settings content                  │ status, and actions  │
│ Results        │                                      │                      │
│ Settings       │                                      │                      │
├────────────────┴──────────────────────────────────────┴──────────────────────┤
│ Contextual fault banner appears inside the affected workspace when needed. │
└──────────────────────────────────────────────────────────────────────────────┘
```

Workspace outer right panels use a light top strip with the exact visible title **Capture**, **Label**, **Train**, **Model Test**, **Library**, **Live**, **Sequence Test**, or **Runs**, left-aligned while expanded, and the existing narrow chevron toggle fixed on the panel's right edge at vertical center, with the same screen x/y position in expanded and collapsed states. When collapsed width is insufficient, only the fixed chevron may remain visible. Settings has no outer collapsible panel. This outer-panel composition is distinct from titled inner disclosure headers and does not change existing exported toggle aliases or wrapper ownership.

### 2.2 Primary navigation behavior

- Selecting a primary item exposes its approved secondary items.
- Selecting a secondary item opens that persistent workspace directly.
- `Data > Capture` opens the shared Capture workspace. Its three modes are internal equal selectors, not additional primary-navigation levels.
- `Sort > Live` always opens the same Live workspace. Its pre-run, running, paused, and completed/interrupted presentations are state changes, not navigation changes.
- The selected navigation item remains indicated while its workspace is visible.
- Navigation does not terminate an active operation.
- Returning to the owning workspace restores its current operation presentation.
- A conflicting Start action in another workspace is disabled with **Another operation is active**.
- Read-only or nonconflicting work may continue elsewhere according to the resource-lock rules in Section 17.

### 2.3 Startup behavior

Every launch selects:

```text
Data > Capture
```

The application does not restore the previously open workspace. This startup rule does not clear separately persisted domain state such as the Active Model or storage preferences.

If the camera is unavailable, the once-per-session `Camera unavailable. Continue?` prompt is a shell-global modal overlay above the entire shell and current workspace while `Data > Capture` remains selected beneath it. Yes continues without Camera; No closes OpenDSS.

If the camera is unavailable at launch, the Capture preview remains present and displays:

> **Camera unavailable**

### 2.4 In-session workspace retention

Within one application session:

- direct navigation does not clear a workspace's selected artifact or draft fields;
- Capture preserves separate draft fields for each of its three modes while idle;
- an active operation retains its operation state regardless of foreground navigation;
- a contextual link replaces or supplies only the artifact named by that link;
- the destination workspace may replace the preselected artifact before starting;
- completed-state content remains visible until the user starts a new operation, selects another artifact, or explicitly resets through the provided next action.

This retention is an interaction convenience within the current session, not a project, workflow record, or startup-restoration feature.

### 2.5 Global status-header projection

| Header area | Authoritative source | Projection rule |
|---|---|---|
| **Camera** | Hardware state | `Unavailable` when the qualified Camera integration cannot be used; `Connected` when available but not streaming; `Streaming` while a preview or camera-owning operation is streaming. |
| **DAQ** | Hardware state and ownership | `Unavailable` when the qualified DAQ cannot be used; `Ready` when available and not owned; `Active` while Live or a physical-output Sequence Test owns it, including a paused Run whose DAQ remains locked. |
| **Active Model** | Model registry | `No Active Model` or the stored Model Name. The value changes only through successful model save, explicit Model Library activation, or loading a valid Setup Profile model reference while Live is in pre-run. |
| **Current Activity** | Operation coordinator plus limited foreground activity | Derived by the priority rules below. |

### 2.6 Current Activity priority

The header uses the first applicable rule:

1. A paused Image Sequence, Droplet Dataset Capture, or Live Run displays `Paused`.
2. An active long-running operation displays its approved activity value:
   - Image Sequence → `Recording Sequence`;
   - Droplet Dataset Capture → `Droplet Dataset Capture`;
   - Training → `Training`;
   - Model Test → `Testing Model`;
   - Sequence Test → `Testing Sequence`;
   - Live Sorting → `Sorting`.
3. A momentary Single Image action displays `Capturing Image` until the write succeeds or fails.
4. Sequence Viewer frame navigation does not project a long-running Current Activity state.
5. Data > Label with a Dataset open in the foreground displays `Labeling` when no higher-priority operation exists.
6. Otherwise, the header displays `Idle`.

`Starting` and `Stopping` do not add new header values. The operation's approved activity value remains displayed until resources are released; the header then returns to `Idle` or the applicable foreground activity.

### 2.7 Global fault presentation

A fault associated with a long-running operation appears as `Error` in the owning workspace, not in a global notification center. The header continues to show factual hardware and activity state. Technical detail is written to the program log; directly useful Retry or Open Folder actions may remain.

---

## 3. Conceptual application-state model

### 3.1 State snapshot

The following is a conceptual state map, not a production serialization format:

```text
Application State
├── Shell
│   ├── selected primary navigation item
│   ├── selected workspace
│   ├── expanded Capture sections
│   └── hardware panel presentation
├── Hardware
│   ├── Camera availability, stream state, applied settings, lock owner
│   └── DAQ availability, readiness, applied settings, lock owner
├── Model
│   ├── Active Model reference
│   └── Model Package use/write locks
├── Artifacts
│   ├── selected Dataset by workspace
│   ├── selected Image Sequence by workspace
│   ├── selected Model Package by workspace
│   └── selected Run in Results
├── Operation
│   ├── operation type or none
│   ├── lifecycle state
│   ├── owning workspace
│   ├── resource locks
│   ├── configuration snapshot
│   └── progress and finalized counters
├── Workspace drafts
│   ├── Capture section fields
│   ├── Train and Model Test selections
│   ├── Live pre-run configuration
│   ├── Sequence Test configuration
│   └── Run Notes edit buffer
├── Fault
│   ├── affected operation or action
│   ├── direct technical reason
│   ├── preservation result
│   └── direct recovery actions
└── Preferences
    ├── default data root
    └── application-owned local preferences
```

### 3.2 Authoritative owners

| State | Authoritative owner | Interaction consequence |
|---|---|---|
| Camera state and applied Camera settings | Hardware coordination domain | Header, panel, previews, and Camera-dependent enablement agree. |
| DAQ state and applied DAQ settings | Hardware coordination domain | Header, panel, Send Test Sine Wave, Live, and Sequence Test agree. |
| Active Model and package metadata | Model registry domain | Header, Library, Live, Train completion, Model Test, and Sequence Test agree. |
| Dataset content and label persistence | Dataset domain | Label, Train, and Model Test use the same class, crop-state, and readability result. |
| Image Sequence loading | Sequence domain | Sequence Viewer and Sequence Test use the same v2 sequence interpretation. |
| Current long-running operation and locks | Operation coordination domain | All Start actions and resource mutations agree on ownership. |
| Training execution and metrics | Training domain | Train progress and completion derive from one execution state. |
| Run discovery and persisted Run data | Run repository domain | Live/Sequence Test completion and Results present the same Run status and files. |
| Storage and application preferences | Settings domain | New operations use the current valid default without rewriting active outputs. |
| Global header | Read-only state projection | The header never becomes an independent state owner. |

Exact internal service or class names remain an engineering choice.

### 3.3 Authoritative versus derived state

The following are derived presentation values and shall not be stored as independent competing truth:

- the global header text;
- workspace `Empty`, `Unavailable`, or `Ready` presentation;
- whether a primary action is enabled;
- the direct disabled reason;
- the hardware panel lock presentation;
- Predicted Class count totals shown in Live and Results;
- Decision counts;
- Observed Route counts;
- the Decision-versus-Observed Route matrix.

Run counters derive from finalized event records. Live counters may be maintained in memory from finalized events but shall not be recomputed by repeatedly reading `events.csv`.

### 3.4 Workspace presentation state derivation

A workspace presentation is derived in this order:

1. If the workspace owns the current operation, display its lifecycle state.
2. Otherwise, if a contextual fault/result remains selected, display the applicable Completed, Interrupted, or Failed presentation.
3. Otherwise, if no required artifact is selected, display Empty where that state applies.
4. Otherwise, if any technical prerequisite is unsatisfied, display Unavailable.
5. Otherwise, display Ready.

`Unavailable` is not a long-running lifecycle state. It is a computed presentation of unmet technical prerequisites.

### 3.5 Operation lifecycle state machine

```text
Empty or Unavailable
        │ prerequisites satisfied
        ▼
      Ready
        │ Start accepted
        ▼
     Starting ────────────────┐
        │ initialization OK   │ initialization fault
        ▼                     ▼
      Running             Interrupted / Failed
        │  │
        │  ├── Pause ──► Paused ── Resume ──┐
        │  │              │                  │
        │  │              └── Stop ─────────┤
        │  ├── Stop                           │
        │  └── Duration expires               │
        ▼                                     │
     Stopping ◄───────────────────────────────┘
        │
        ├── finalization succeeds ──► Completed
        ├── externally interrupted ─► Interrupted
        └── processing/write failure ─► Failed
```

Only Image Sequence, Droplet Dataset Capture, and Live use the long-operation Paused branch. Sequence Viewer has no playback lifecycle. Training, Model Test, and Sequence Test do not add Pause.

### 3.6 Completed, Interrupted, and Failed distinctions

- **Completed** means the operation finalized its expected canonical output. A clean manual Stop may lead to this workspace presentation.
- **Interrupted** means an accepted operation ended before its normal endpoint because the user stopped a nonfinalizable operation or an external technical interruption occurred. Recoverable output may exist.
- **Failed** means the operation's processing, runtime, or persistence contract failed. Partial output may still be preserved and must be reported factually.

For Live and Sequence Test, a clean user Stop uses the completed post-operation presentation while the persisted Run status may be `Stopped` with a user stop reason.

### 3.7 Configuration snapshot rule

When a long-running operation is accepted:

1. OpenDSS validates prerequisites.
2. OpenDSS retains the initial snapshot of all effective user selections and fixed qualified configuration required for provenance.
3. The operation owns the required resources.
4. Later navigation, Setup Profile changes, or idle settings do not alter the active operation. Live is the explicit exception for valid committed changes to Active Model, Hit Class, Trigger Every Droplet, DAQ Output, and hit-boundary fields: each applies immediately, appends a timestamped effective-configuration identity, and is referenced by every affected event.

---

## 4. Shared interaction patterns

### 4.1 Primary-action placement

The current primary action appears at the bottom of the operation panel or the equivalent final action region. Examples are:

- Capture Image;
- Start Recording;
- Start Droplet Dataset Capture;
- Start Training;
- Automatic Save;
- Start Model Test;
- Set Active;
- Start Sorting;
- Pause or Resume during Live;
- Start Sequence Test;
- Load selected Run;
- Save Notes.

Only the action appropriate to the current state is visually primary.

### 4.2 Disabled-reason behavior

A disabled primary action displays one short reason immediately adjacent to the action. The reason updates as state changes.

```text
[ Start Sorting — disabled ]
Camera unavailable
```

The interface does not add a checklist of all unmet conditions. Field-specific validation may appear beside the affected field, while the primary button still shows the first operation-level blocker.

When several blockers exist, the reason is selected in this order:

1. the workspace is already Starting, Running, Paused, or Stopping;
2. another operation or resource lock conflicts;
3. required hardware is unavailable or not ready;
4. a required artifact is absent, unreadable, or incompatible;
5. a required workflow selection is missing;
6. the output location is not writable.

Workspace-specific wording in Section 16 takes precedence over this general order.

### 4.3 Artifact selection

- `Open Dataset`, `Open Sequence`, Model selectors, Model import, Setup Profile Open, output selection, and folder actions use the standard Windows picker appropriate to the artifact.
- A successful selection replaces the selection in that workspace only, unless it changes the global Active Model through an explicit activation action.
- A failed open leaves the last valid selection unchanged when one exists.
- Unsupported artifacts produce a direct **Unsupported OpenDSS v2 schema** message and are not partially interpreted.
- Model Test and Sequence Test selections do not change the global Active Model.
- Contextual links supply a selection but do not start an operation automatically.

### 4.4 Names, Duration, and Save Location

- Optional names may be blank. OpenDSS resolves them to a timestamp when the artifact or Run is created.
- Blank Duration means continue until Stop and is persisted as `null`, not `0`.
- A specified Duration may end the operation automatically; Stop remains available earlier.
- Save Location defaults to the applicable OpenDSS data folder and may be replaced with another writable location.
- Output writability is checked before Start and again during actual writes.
- A failed write is never reported as success.

### 4.5 Start acceptance

Selecting Start has two possible outcomes:

1. **Not accepted:** a prerequisite changed or validation failed; the workspace stays Ready/Unavailable and shows the direct reason.
2. **Accepted:** the workspace enters Starting, input controls lock, the configuration snapshot is created, and Stop becomes available as soon as cancellation can be handled safely.

Double activation of Start shall not create duplicate operations or folders.

### 4.6 Stop behavior

- Every long-running operation provides Stop.
- Stop requests are idempotent: repeated activation does not create repeated finalization.
- After Stop is accepted, the workspace enters Stopping and disables new Start/Pause/Resume actions.
- Persistence queues flush at Stop.
- Image Sequence and Droplet Dataset Capture cleanly finalize their canonical metadata when possible.
- Live and Sequence Test stop new DAQ output before finalization.
- Training and Model Test stop new processing and preserve only output that their contracts can represent factually.

### 4.7 Pause behavior

| Workspace | Continues while paused | Stops while paused | Resume behavior |
|---|---|---|---|
| **Image Sequence** | Live camera preview | TIFF writing and active-recording time | Continues the same sequence folder and numbering. |
| **Droplet Dataset Capture** | Live camera preview | TIFF writing, droplet detection, and Droplet Crop creation | Continues the same Dataset and counters. |
| **Live** | Live camera preview | Inference, new DAQ output, and new event finalization | Continues the same Run and configuration snapshot. |
| **Sequence Viewer** | Current frame display | Frame navigation | Retains the current frame position. |

During an operation pause, Camera acquisition settings remain locked when Camera is owned. DAQ edits remain available only when the device is available and the explicit ownership and arbitration rules permit them; supported Amplitude and Frequency edits may retune active continuous output immediately.

### 4.8 Completion actions

Completion actions are contextual and do not create a wizard:

- Image Sequence → Open in Sequence Viewer; Open in Sequence Test;
- Droplet Dataset Capture → Open in Label;
- Train after save → Open in Model Test;
- Live or Sequence Test → Open Run Summary; Open Run Folder;
- Results with full sequence → Open Saved Sequence.

### 4.9 Contextual fault banner

```text
┌──────────────────────────────────────────────────────────────────────┐
│ Droplet Dataset Capture interrupted                                          │
│ The camera disconnected. Existing frames and crops were preserved.  │
│                                                                      │
│ [ Open Dataset ]  [ Open Folder ]                                    │
└──────────────────────────────────────────────────────────────────────┘
```

A banner contains:

- operation or action that failed;
- `Error` as the normal visible message, with technical detail in the program log;
- whether partial data was preserved;
- one or two direct recovery/inspection actions.

The banner persists in the affected workspace until the user starts a new operation, opens a replacement artifact, or completes a provided recovery action. OpenDSS does not repeat the same fault through a sequence of modal dialogs.

### 4.10 Inline validation

- Invalid Camera or DAQ values remain unapplied and the last successfully applied value remains active.
- Invalid numeric or enumerated input displays a short field-level reason.
- Required workflow selections remain empty rather than being silently guessed.
- Scientific performance measurements never become validation errors.

### 4.11 Confirmation and picker inventory

The low-fidelity interaction model uses only these necessary external or modal interactions:

| Interaction | Presentation |
|---|---|
| Open Dataset, Sequence, Profile, Model Package, output file/folder | Standard Windows picker. |
| Save Profile As, Export Model, Automatic Save location | Standard Windows save/folder picker. |
| Delete Model | One direct destructive confirmation naming the Model Package. |
| Replace an existing ordinary file | Standard Windows overwrite confirmation supplied by the picker/file operation. |

Class definition, model naming, Run Notes editing, operation completion, and fault recovery remain inside their workspaces rather than opening product-specific wizard dialogs.

---

## 5. Shared Hardware Configuration panel

### 5.1 Low-fidelity panel

```text
Header:  Camera: Streaming | DAQ: Ready | ...

┌── resizable left navigation ──┐
│ navigation                    │
│                               │
│ ┌───────────────────────────┐ │
│ │ HARDWARE CONFIGURATION  ˄ │ │
│ │ ▾ Camera                  │ │
│ │ Status: <status>          │ │
│ │ <supported controls>      │ │
│ │ ▾ DAQ                     │ │
│ │ Status: <status>          │ │
│ │   Output Configuration    │ │
│ │   Output Channel [ ... ▼ ]│ │
│ │   Amplitude / Frequency   │ │
│ │   Event Duration / Delay  │ │
│ │   [ Start/Stop Continuous]│ │
│ └───────────────────────────┘ │
└───────────────────────────────┘
```

`<supported controls>` means the actual qualified Camera or DAQ properties exposed by the integrated hardware adapter. The UI shall not add generic placeholder fields for unsupported properties.

**Hardware Configuration** is the visible panel title, not an illustrative-mock label. **Camera** and **DAQ** are titled collapsible groups. **Output Configuration** is nested inside DAQ as a visually inset, bordered subordinate group and contains Output Channel, Amplitude, Frequency, Event Duration, Decision-to-trigger Delay, and one **Start Sine Wave** button. The DAQ form has no separate sine-wave heading, status subsection, or explanatory paragraph. This hierarchy is visual/product authority; actual clickable group collapse must use the existing DESIGN/FUNCTIONAL seam and does not authorize new runtime handlers. The form exports `cameraSectionHeadingButton`, `cameraSectionExpanded`, `daqSectionHeadingButton`, and `daqSectionExpanded`; functional wrapper code owns the two click-to-toggle connections.

The Hardware panel opens at a compact design-token height. Its upper boundary is directly adjustable vertically within the left-navigation column, subject to bounded minimum and maximum heights. Content scrolls when it exceeds the selected height. The selected height is session-local, resets on application launch, and is not persisted.

### 5.2 Open and close behavior

- The panel's right-aligned chevron opens or closes the shell-owned panel.
- Ordinary open/closed presentation may remain stable while navigating during the current session.
- The panel does not define a separate workspace or navigation item.
- The global status header remains visible while the panel is open.
- The panel is docked at the bottom of the resizable left-navigation column, matches that column's width, expands upward within it, and has no visible redundant Close action.
- The user may resize the open panel vertically within its bounds without changing device state; overflow content scrolls.
- During Live and Sequence Test, the panel remains available under the explicit Camera-lock and DAQ-arbitration rules.

### 5.3 Immediate-apply behavior

A valid Camera or DAQ change applies when its edit is committed:

- selector: on selection;
- checkbox/toggle: on change;
- numeric/text value: on Enter or focus commit after validation.

There is no separate global Apply action.

If the device accepts the change:

- the applied value becomes authoritative;
- all workspaces immediately observe the same value;
- future operation snapshots use it.

If the device rejects the change:

- the control returns to the last successfully applied value;
- a direct field-level reason is shown;
- no operation snapshot is changed.

### 5.4 Panel conditions

| Condition | Panel presentation | Editable behavior | Direct explanation |
|---|---|---|---|
| **Idle and available** | Section visible and enabled. | Valid edits apply immediately. | None. |
| **Unavailable** | Section visible with status. | All controls in that device section disabled. | **Camera unavailable** or **DAQ unavailable**. |
| **Owned by non-Live operation** | Panel may open. Owned section is read-only; unowned available section remains editable. | No change to the owned device. | **Camera settings are locked while Image Sequence recording is active**, or equivalent. |
| **Live active or paused** | Panel remains docked and may open. | Camera acquisition fields remain locked. Continuous configured waveform may start or stop; other permitted DAQ edits follow immediate apply. | Invalid or unavailable actions show their direct factual reason. |
| **Starting or Stopping** | Panel remains docked. | Controls that cannot safely change in that transition are disabled; the single DAQ-output state remains factual. | Operation-specific starting/stopping reason. |

### 5.5 Ownership effects

- Image Sequence and Droplet Dataset Capture own Camera only.
- Live owns Camera and DAQ.
- Sequence Test owns DAQ only when Physical DAQ Output is enabled.
- Training and Model Test own neither hardware device; the panel remains available.

### 5.6 Continuous configured waveform arbitration

- All physical DAQ output is sine-wave output. Operation-requested output is an event-triggered finite sine wave issued for an accepted `Decision = Hit`.
- Continuous configured waveform is a shared Hardware > DAQ action on the same physical output channel as event-triggered finite sine waves.
- It may start while Live or Sequence Test owns DAQ.
- While active, it has priority. Processing, classification, decisions, and logging continue, but every otherwise-requested event-triggered finite sine wave is recorded as **suppressed / not issued** and discarded rather than queued.
- Event-triggered output resumes after it stops only when DAQ Output is enabled and DAQ is Ready.
- Its explicit Stop, application exit, DAQ disconnect, or DAQ fault stops and resets it, returning the output to 0 V.
- HardwareCoordinator owns one authoritative DAQ-output state; OperationCoordinator owns arbitration with operation-requested event-triggered finite sine waves.
- Hardware > DAQ exposes Amplitude from 0–10 Vpp (default 5 Vpp, centered at 0 V with extrema `-Vpp/2` and `+Vpp/2`, increment 1 Vpp), Frequency from 1–1000 kHz (default 10 kHz, increment 1 kHz), Event Duration from 1–500 ms (default 5 ms, increment 1 ms), and Decision-to-trigger Delay from 0–500 ms (default 0 ms, increment 1 ms).
- Amplitude and Frequency apply to continuous, event-triggered, and test sine output. Event Duration applies to event-triggered and test finite sine waves, not continuous output. Decision-to-trigger Delay is measured from accepted `Decision = Hit` and applies only to decision-triggered output.
- Valid changes apply immediately. Supported Amplitude and Frequency edits retune active continuous output immediately. If the adapter cannot apply a requested value or live retune, the edit is rejected with a direct explanation and the last applied value remains active. Event Duration and Decision-to-trigger Delay changes apply only to future, not-yet-issued event output.
- Sequence Viewer, Label, Library, Results, and Settings own neither device.
- Send Test Sine Wave uses current Amplitude, Frequency, and Event Duration without Decision-to-trigger Delay. It briefly uses the DAQ but creates no Run or event and does not occupy the long-running-operation slot.
- This authority synchronization does not implement or authorize changes to protected NI-DAQmx mechanics; a separate functional work order and the repository's protected-asset characterization, regression, performance, hardware-in-the-loop, justification, and rollback evidence remain required.

### 5.6 Prohibited panel content

The panel shall not expose:

- droplet-detection parameters;
- Droplet Crop parameters;
- routing-algorithm parameters;
- internal tracking or synchronization timing;
- training parameters;
- a software arming state;
- Hit Class or Hit boundary calibration.

Hit Class and Hit boundary calibration are run selections. DAQ Output Channel is a DAQ technical setting.

---

## 6. Data > Capture interaction specification

### 6.1 Shared Capture layout

```text
Data > Capture

┌────────────────────────────────────────────┬───────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ ▸ Single Image            │
│                                            │ ▸ Image Sequence          │
│                                            │ ▸ Droplet Dataset Capture         │
└────────────────────────────────────────────┴───────────────────────────┘
```

The preview remains the primary content area. The right-side panel always shows all three fixed section headings. Each section expands and collapses independently, multiple sections may be open while idle, and all three start collapsed when Data > Capture opens. One expanded body receives all remaining body height; two divide it equally; three divide it into equal thirds. Each expanded body scrolls independently without moving any heading.

### 6.2 Section expansion and operation locking

- While idle, each heading independently toggles its section and does not close another open section.
- Each section retains its own draft fields during the current session when collapsed.
- Camera settings remain in the bottom Hardware panel.
- While the Single Image frame and TIFF are being captured/written, Single Image remains expanded and cannot be collapsed; the other two headings remain visible but disabled.
- From accepted Start through finalization, Image Sequence or Droplet Dataset Capture remains expanded and cannot be collapsed; the other two headings remain visible but disabled.
- After Completed, Interrupted, or Failed, the other headings re-enable and the result section remains expanded until the user collapses it.

### 6.3 Camera-unavailable presentation

```text
┌────────────────────────────────────────────┐
│                                            │
│              Camera unavailable            │
│                                            │
└────────────────────────────────────────────┘
```

All three headings remain visible. Any expanded nonhardware file fields remain visible, and Camera-dependent primary actions are disabled with **Camera unavailable**.

The per-section layouts below illustrate the expanded body for that section. In the complete workspace, all three fixed headings remain visible and any other expanded bodies retain their allocated, independently scrollable regions.

---

### 6.4 Single Image

#### 6.4.1 Ready layout

```text
┌────────────────────────────────────────────┬───────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ SINGLE IMAGE              │
│                                            │                           │
│                                            │ File Name                 │
│                                            │ [____________________]    │
│                                            │                           │
│                                            │ Save Location             │
│                                            │ [ path____________ ] [...]│
│                                            │                           │
│                                            │ [ Capture Image ]         │
└────────────────────────────────────────────┴───────────────────────────┘
```

#### 6.4.2 Interaction behavior

1. File Name is optional; blank resolves to a timestamp.
2. Save Location defaults to the Images folder and may be changed.
3. Selecting **Capture Image** reserves Camera and the image-write path momentarily.
4. The action disables until capture and write finish.
5. Current Activity displays `Capturing Image`.
6. Success displays the saved TIFF path inline without navigating away.
7. Failure displays one banner and does not claim a saved image.
8. No Dataset, Model Test output, Run, classification, or DAQ action is created.

#### 6.4.3 State transitions

```text
Unavailable ── prerequisite restored ──► Ready
Ready ── Capture Image ──► transient busy ──► Completed
                                      └────► Failed
Completed ── Capture Image ──► transient busy
```

---

### 6.5 Image Sequence

#### 6.5.1 Ready layout

```text
┌────────────────────────────────────────────┬───────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ IMAGE SEQUENCE            │
│                                            │ Name [_______________]    │
│                                            │ Experiment Type [____]    │
│                                            │ Notes [______________]    │
│                                            │ Duration [___________]    │
│                                            │ Save Location [____] [...]│
│                                            │                           │
│                                            │ [ Start Recording ]       │
└────────────────────────────────────────────┴───────────────────────────┘
```

#### 6.5.2 Running layout

```text
┌────────────────────────────────────────────┬───────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ RECORDING SEQUENCE        │
│                                            │ Status: Running           │
│                                            │ Elapsed: <active time>    │
│                                            │ Frames: <count>           │
│                                            │                           │
│                                            │ [ Pause ]  [ Stop ]       │
└────────────────────────────────────────────┴───────────────────────────┘
```

#### 6.5.3 Paused layout

```text
Status: Paused
Elapsed: <frozen active-recording time>
Frames: <stable count>

[ Resume ]  [ Stop ]
```

#### 6.5.4 Completed layout

```text
Image Sequence completed
Frames: <final count>
Stop reason: <duration | user>
Location: <path>

[ Open in Sequence Viewer ]  [ Open in Sequence Test ]
[ Start New Recording ]
```

#### 6.5.5 Interaction behavior

- Blank Duration records until Stop.
- Start creates one sequence folder and initializes recoverable metadata.
- Running writes individually numbered TIFF frames in acquisition order.
- Pause stops frame writing and active-recording time while preview continues.
- Resume continues the same folder and frame numbering.
- Stop or Duration expiry enters Stopping and finalizes `sequence.json`.
- The active section remains expanded and read-only where applicable from Starting through Stopping; the other two headings remain visible but disabled.
- Completion does not add the sequence to Results.
- Open in Sequence Viewer and Open in Sequence Test are contextual links, not mandatory next steps.

#### 6.5.6 Transition model

```text
Unavailable ⇄ Ready → Starting → Running ⇄ Paused
                                  │          │
                                  └── Stop ──┘
                                       ↓
                                    Stopping
                                  ↙     ↓      ↘
                         Completed  Interrupted  Failed
```

---

### 6.6 Droplet Dataset Capture

#### 6.6.1 Ready layout

```text
┌────────────────────────────────────────────┬───────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ DROPLET DATASET CAPTURE           │
│                                            │ Dataset Name [_______]    │
│                                            │ Experiment Type [____]    │
│                                            │ Notes [______________]    │
│                                            │ Duration [___________]    │
│                                            │ Save Location [____] [...]│
│                                            │                           │
│                                            │ Fixed qualified detection│
│                                            │ and crop processing       │
│                                            │                           │
│                                            │ [ Start Droplet Dataset Capture ] │
└────────────────────────────────────────────┴───────────────────────────┘
```

The fixed-processing text is factual, not an expandable settings control.

#### 6.6.2 Running layout

```text
┌────────────────────────────────────────────┬───────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ CAPTURING DATASET         │
│                                            │ Status: Running           │
│                                            │ Elapsed: <active time>    │
│                                            │ Full Frames: <count>      │
│                                            │ Detected Droplets: <count>│
│                                            │ Droplet Crops: <count>    │
│                                            │                           │
│                                            │ [ Pause ]  [ Stop ]       │
└────────────────────────────────────────────┴───────────────────────────┘
```

#### 6.6.3 Completed layout

```text
Droplet Dataset Capture completed
Frames: <count>
Detected Droplets: <count>
Saved Droplet Crops: <count>
Location: <path>

[ Open in Label ]  [ Open Folder ]
[ Start New Droplet Dataset Capture ]
```

#### 6.6.4 Interaction behavior

- Droplet Dataset Capture never requires an Active Model or DAQ.
- It does not show model, Predicted Class, Decision, or Observed Route controls.
- It creates one Dataset per capture.
- Every newly created Droplet Crop begins as `Unlabeled` with no Label.
- Pause stops full-frame writing, detection, and Droplet Crop creation while preview continues.
- Stop or Duration expiry finalizes `dataset.json` when technically possible.
- On an interruption, the workspace shows `Error`; preserved artifacts determine whether Open Dataset or Open Folder is available.
- No external-image import or Dataset merge action appears.

#### 6.6.5 Transition model

The transition model is the same as Image Sequence, with Dataset-specific initialization, counters, persistence, and recovery actions.

---

## 7. Data > Label interaction specification

### 7.1 Empty layout

```text
Data > Label

┌──────────────────────────────────────────────────────────────────────┐
│ No Dataset selected                                                  │
│                                                                      │
│ [ Open Dataset ]                                                     │
└──────────────────────────────────────────────────────────────────────┘
```

### 7.2 Dataset with no class definition

```text
Dataset: <name>                                    [ Open Dataset ]

Number of Classes
( ) 2 Classes     ( ) 3 Classes

Class IDs will be created as 0 and 1, or 0, 1, and 2.
Class count becomes fixed after labels are assigned.
```

Selecting two or three classes creates the corresponding stable Class IDs. Class Names remain editable. No scientific recommendation is displayed.

### 7.3 Ready layout

```text
┌───────────────────────────────────────────────────┬──────────────────────┐
│ Dataset: <name>             [ Open Dataset ]      │ SELECTED CROPS       │
│ Filters: [ All ▼ ] [ State ▼ ]                    │ <large crop preview> │
│                                                   │ Selected: <count>    │
│ ┌────┐ ┌────┐ ┌────┐ ┌────┐                       │                      │
│ │crop│ │crop│ │crop│ │crop│   Droplet Crop grid  │ [ Class 0 ]         │
│ └────┘ └────┘ └────┘ └────┘                       │ [ Class 1 ]         │
│                                                   │ [ Class 2 ] if used │
│                                                   │ [ Skip ]            │
│                                                   │ [ Remove from       │
│                                                   │   Dataset ]         │
│                                                   │ [ Restore ] when    │
│                                                   │   Removed selected  │
│                                                   │ [ Undo ]            │
├───────────────────────────────────────────────────┼──────────────────────┤
│ IMAGE COUNTS                                      │ CLASSES              │
│ Class 0 — <name>: <count>                         │ 0 [name________]     │
│ Class 1 — <name>: <count>                         │ 1 [name________]     │
│ Class 2 — <name>: <count>                         │ 2 [name________]     │
│ Unlabeled / Skipped / Removed                     │ [ Use in Train ]     │
└───────────────────────────────────────────────────┴──────────────────────┘
```

### 7.4 Crop selection

- A click selects one crop.
- Standard Ctrl-click toggles individual crops; Shift-click extends a range.
- Class, Skip, Remove, Restore, and relabel actions apply to the current selection.
- Selection count is shown factually.
- No per-crop notes field appears.

### 7.5 Label actions

| Action | Result |
|---|---|
| **Class 0 / 1 / 2** | Sets the selected crop entries to `Labeled` with the chosen immutable Class ID. |
| **Relabel** | The same class action replaces the prior Label on selected Labeled crops. |
| **Skip** | Sets selected crops to `Skipped` with no training-eligible Label. |
| **Remove from Dataset** | Sets selected crops to `Removed`; PNG files and entries remain on disk. |
| **Restore** | Returns selected Removed crops to `Unlabeled`; their PNG files and Dataset entries remain on disk. |
| **Undo** | Reverts the most recent Label workspace edit in the current session and persists the reverted Dataset state atomically. |

### 7.6 Saving behavior

- There is no separate Dataset Save button.
- Each completed labeling, class-name, Skip, Remove, Restore, or Undo command is persisted to the selected `dataset.json` through atomic replacement.
- Class Name edits commit on Enter or focus commit after validation.
- If persistence fails, the prior canonical JSON remains intact, the workspace shows `Error`, and details are logged.
- The interface shall not represent an unsaved change as persisted.

### 7.7 Lock behavior

If Training or Model Test uses the same Dataset:

- the Dataset identity and crops may remain visible;
- mutating actions are disabled;
- the direct reason names the operation, for example **Dataset is in use by Training**;
- another Dataset may be opened and edited if it is not locked.

### 7.8 Contextual transition

**Use in Train** opens Models > Train with the current Dataset preselected. It does not start Training and does not require that all crops be Labeled; Train applies only its technical requirement that eligible Labeled Droplet Crops exist.

---

## 8. Data > Sequence Viewer interaction specification

### 8.1 Empty layout

```text
Data > Sequence Viewer

┌──────────────────────────────────────────────────────────────────────┐
│ No Image Sequence selected                                           │
│                                                                      │
│ [ Open Sequence ]                                                    │
└──────────────────────────────────────────────────────────────────────┘
```

### 8.2 Ready frame-navigation layout

```text
┌──────────────────────────────────────────────────────────────────────┐
│ CURRENT FRAME                                                        │
│                                                                      │
│                         <full frame>                                 │
│                                                                      │
├──────────────────────────────────────────────────────────────────────┤
│ Frame <current> of <total>                                           │
│ [|< Previous] [Next >|]  Frame [________]                            │
│ [ Zoom - ] [ Zoom + ] [ Fit ] [ 1:1 ]                               │
│ [ Open Sequence ]                                                   │
└──────────────────────────────────────────────────────────────────────┘
```

### 8.3 Interaction behavior

- Open Sequence accepts a standalone v2 `sequence.json`, a Dataset-referenced sequence, or a Run-referenced sequence.
- Previous Frame and Next Frame move exactly one readable frame.
- Direct seek moves to the requested readable frame.
- Left/Right move one frame; Shift+Left/Right move 10; Ctrl+Left/Right move 50; Home/End move first/last.
- Plus/minus changes zoom; F selects Fit; 1 selects 1:1.
- Zoom changes the display only and never modifies source files.
- Missing frames are skipped silently.
- Sequence Viewer requires no Camera, DAQ, model, or training environment.
- Frame navigation does not own the global long-running-operation slot and never emits DAQ output.

### 8.4 State transitions

```text
Empty ── Open valid Sequence ──► Ready
Ready ── Previous / Next / seek / zoom ──► Ready
Any open/read failure ──► Failed
```

---

## 9. Models > Train interaction specification

### 9.1 Ready layout

```text
Models > Train

┌────────────────────────────────────────────┬───────────────────────────┐
│ DATASET SUMMARY                            │ TRAINING SETUP            │
│ Dataset: <name> [ Select Dataset ]         │ Architecture              │
│ Classes: 2 or 3                            │ MobileNet — Faster        │
│ Class 0: <name> — <eligible count>         │ EfficientNet — More Accurate │
│ Class 1: <name> — <eligible count>         │                          │
│ Class 2: <name> — <eligible count>         │                          │
│                                            │ Weights [ value ▼ ]       │
│                                            │ Compute Device            │
│                                            │ [ GPU ▼ ]                 │
│                                            │ Effective: <GPU or CPU>   │
│                                            │                           │
│                                            │                           │
│                                            │ Model Name [________]     │
│                                            │ Save Location [____] [...]│
│                                            │                           │
│                                            │ [ Start Training ]        │
└────────────────────────────────────────────┴───────────────────────────┘
```

There is no Advanced Training Parameters section or expansion control.
The fixed seed is omitted from the normal Train presentation but remains recorded in model metadata. Compute Device is a normal Train selector with GPU selected by default and CPU available; it is not a hyperparameter. Requested and effective devices are displayed and recorded.

### 9.2 Running layout

```text
┌────────────────────────────────────────────┬───────────────────────────┐
│ TRAINING METRICS                           │ TRAINING STATUS           │
│ Training/Validation Loss plot              │ Status: Running           │
│ Validation Accuracy plot                   │ Requested: <GPU or CPU>   │
│                                            │ Effective: <GPU or CPU>   │
│                                            │ Elapsed: <time>           │
│                                            │ Estimated Remaining: <t> │
│                                            │ Epoch: <n> of <n>         │
│                                            │ Overall: <progress>       │
│                                            │                           │
│                                            │ [ Stop Training ]         │
└────────────────────────────────────────────┴───────────────────────────┘
```

### 9.3 Completed and automatic-save layout

```text
Training completed
Overall results table
Per-class results table
Confusion matrix when generated
Saved path: <confirmed path>
Active Model: <name>
[ Open in Model Test ]
```

### 9.4 Interaction behavior

- The Dataset selector loads one v2 Dataset through the authoritative Dataset contract.
- Only Labeled Droplet Crops are eligible.
- Image counts are factual; no class-balance warning is shown.
- Architecture requires MobileNet or EfficientNet; **MobileNet — Faster** and **EfficientNet — More Accurate** appear on one line, with Faster and More Accurate rendered smaller and lighter.
- Weights requires ImageNet-pretrained or a technically compatible bundled/user-added OpenDSS droplet-trained checkpoint identified by Model Package identity and checksum.
- Incompatible, missing, unreadable, or checksum-mismatched checkpoints are rejected with a direct reason while the last valid selection remains active.
- Split 70/15/15 and seed 1729 are displayed as read-only facts or concise supporting information.
- A GPU request uses qualified GPU when available and otherwise falls back to effective CPU with a direct explanation. A CPU request stays on CPU and is never promoted automatically.
- Start locks Dataset, Architecture, Weights, Compute Device, Model Name, and Save Location and occupies the global operation slot.
- Stop Training ends processing and does not create a normal completed Model Package.
- Low accuracy, model collapse, or one dominant Predicted Class does not prevent automatic save when required artifacts exist.
- Final save occurs automatically at the preselected name and location.
- Successful save creates `metadata.json`, `checkpoint.pth`, and `model.onnx`, registers the package, and makes it the global Active Model.
- Save failure shows `Error`, retains completed temporary artifacts, enables Retry Save, and does not change the Active Model.
- Training history is not added to Results.

### 9.5 Transition model

```text
Empty / Unavailable ⇄ Ready → Starting → Running → Saving → Completed
                                      │         │
                                      │         └── save fault → Save Failed ── Retry Save ──► Saving
                                      └── Stop → Stopping → Interrupted
```

The Active Model changes only after Saving succeeds.

---

## 10. Models > Model Test interaction specification

### 10.1 Ready layout

```text
Models > Model Test

┌────────────────────────────────────────────┬───────────────────────────┐
│ SELECTED ARTIFACTS                         │ TEST SETUP                │
│ Active Model: <name>                       │ Class Count: <2 or 3>     │
│ Dataset: <name> [ Select Dataset ]         │ Device: <GPU or CPU>      │
│ Eligible Labeled Crops: <count>            │ Output Location [___] [...]│
│                                            │                           │
│                                            │ [ Start Model Test ]      │
└────────────────────────────────────────────┴───────────────────────────┘
```

### 10.2 Running layout

```text
Model Test running
Device: <GPU or CPU>
Processed: <n> of <total>
Progress: <progress>

[ Stop Model Test ]
```

### 10.3 Completed layout

```text
Overall Accuracy: <value>

Per-Class Accuracy
Class 0 — <name>: <value>
Class 1 — <name>: <value>
Class 2 — <name>: <value>

Confusion Matrix
<table sized to 2 or 3 classes>

[ Open Predictions CSV ]  [ Open Output Folder ]
[ Start Another Model Test ]
```

### 10.4 Interaction behavior

- Model Test uses the read-only Active Model; the Dataset is the only local artifact selection. Set Active in Library is required before testing another Model.
- The same Dataset used for Training is permitted without warning.
- A two-class/three-class mismatch blocks Start with the factual class-count message.
- Only Labeled crops are processed; Unlabeled, Skipped, and Removed crops are excluded.
- GPU acceleration is automatic when compatible; CPU fallback is automatic and GPU absence never blocks Start.
- Stop ends further inference and finalizes only technically representable output.
- Completion writes `model_test_summary.json` and `predictions.csv`.
- Class output columns are named Class Scores, not Confidence.
- Model Test does not create a Run and does not appear in Results.
- No integrated misclassified-image browser is introduced.

### 10.5 Transition model

```text
Empty / Unavailable ⇄ Ready → Starting → Running → Completed
                                      │       │
                                      └ Stop  ├ fault
                                        ↓     ↓
                                     Stopping
                                      ↙    ↘
                               Interrupted  Failed
```

---

## 11. Models > Library interaction specification

### 11.1 Empty layout

```text
Models > Library

No OpenDSS v2 Model Packages found.

[ Import Model ]
```

### 11.2 Ready layout

```text
┌────────────────────────────────────────────┬───────────────────────────┐
│ MODEL PACKAGES                             │ SELECTED MODEL            │
│                                            │ Name: <name>              │
│ ● <active model>                           │ Active: Yes / No          │
│   <model 2>                                │ Architecture: <value>     │
│   <model 3>                                │ <Faster/More Accurate>    │
│                                            │ Classes: <2 or 3>         │
│                                            │ Class IDs and Names       │
│                                            │ Source Dataset            │
│                                            │ Creation Date             │
│                                            │ Package Location          │
│                                            │ Training Metadata/Metrics │
│                                            │                           │
│ [ Import Model ]                           │ [ Set Active ]            │
│                                            │ [ Open in Model Test ]    │
│                                            │ [ Export Model ]          │
│                                            │ [ Duplicate Model ]       │
│                                            │ [ Rename Model ]          │
│                                            │ [ Delete Model ]          │
└────────────────────────────────────────────┴───────────────────────────┘
```

### 11.3 Selection and activation

- Selecting a list row loads its metadata in the detail panel.
- Set Active is disabled when no model is selected, the selected model is already Active, or an operation locks replacement of the Active Model.
- Successful Set Active updates the Model Registry and global header immediately.
- Exactly one Active Model or `No Active Model` is represented globally.

### 11.4 Package actions

| Action | Interaction result |
|---|---|
| **Import Model** | Opens a v2 Model Package location, performs technical package checks, then registers/copies the complete package according to the selected file operation. No scientific state is assigned. |
| **Export Model** | Writes the complete package to a user-selected location. |
| **Duplicate Model** | Creates an independent package copy with a new Model ID; the user supplies its name/location through the file action. |
| **Rename Model** | Changes the user-facing Model Name without changing weights, Class IDs, or the scientific package contract. |
| **Delete Model** | Opens one confirmation naming the package; successful deletion removes it. Deleting the Active Model clears Active Model state. |
| **Open in Model Test** | Opens Model Test with the selected package preselected. |

### 11.5 Use locks

- A Model Package used by Model Test, Live, or Sequence Test cannot be renamed, deleted, replaced, or otherwise mutated until the operation releases it.
- Other unowned Model Packages remain manageable.
- When the current Active Model is used by an operation, another model cannot become Active until that operation ends.
- Import accepts approved v2 packages only and provides no legacy conversion action.

---

## 12. Sort > Live interaction specification

### 12.1 Pre-run layout

```text
Sort > Live

┌────────────────────────────────────────────┬────────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ LIVE RUN CONFIGURATION     │
│                                            │ Profile                   │
│                                            │ [ Open ] [ Save ] [Save As]│
│                                            │                           │
│                                            │ Run Name [____________]   │
│                                            │ Experiment Type [______]  │
│                                            │ Notes [_______________]   │
│                                            │ Duration [____________]   │
│                                            │ Save Location [____] [...]│
│                                            │                           │
│                                            │ Trigger Every Droplet              │
│                                            │ ( ) Class-Based Sorting   │
│                                            │ ( ) Trigger Every Droplet │
│                                            │                           │
│                                            │ Active Model: <name/none> │
│                                            │ Hit Class [ class ▼ ]     │
│                                            │ Hit boundary calibration      │
│                                            │ ( ) +Y — Downward         │
│                                            │ ( ) −Y — Upward           │
│                                            │ Hit: <direction>          │
│                                            │ Waste: <opposite>         │
│                                            │                           │
│                                            │ [ ] Record Full Image     │
│                                            │     Sequence              │
│                                            │                           │
│                                            │ [ Send Test Sine Wave ]   │
│                                            │ [ Start Sorting ]         │
└────────────────────────────────────────────┴────────────────────────────┘
```

Camera and DAQ technical controls remain in the bottom Hardware panel opened from the header.

### 12.2 Conditional configuration behavior

#### Class-Based Sorting

- Active Model is required and displayed read-only from the global Model Registry.
- Hit Class is required and contains the Class IDs and Class Names from the Active Model.
- Predicted Class is determined by the largest Class Score.
- Decision is Hit only when Predicted Class equals Hit Class.

#### Trigger Every Droplet

- Active Model is optional.
- Hit Class is hidden or disabled because it does not control Decision.
- Every detected droplet produces Decision = Hit.
- When a model is present, Predicted Class and Class Scores are still logged.
- When no model is present, those fields remain empty.

#### Hit boundary calibration

Selecting a direction immediately shows the factual mapping:

```text
+Y selected: Hit = +Y ↓   Waste = −Y ↑
−Y selected: Hit = −Y ↑   Waste = +Y ↓
```

The interface does not use the ambiguous term Hit Channel.

### 12.3 Setup Profile interactions

- **Open** uses a standard picker and accepts one supported v2 Setup Profile JSON file.
- When the Profile contains a readable referenced Model Package, ModelRegistry makes that package the global Active Model and the header updates.
- **Save** writes the current profile path; when no current profile path exists, it performs Save As behavior.
- **Save As** writes a new ordinary v2 file.
- Save and Save As snapshot the current authoritative applied Camera/DAQ settings, the current Active Model reference when present, and the current approved run selections.
- The profile contains only approved hardware settings and run selections.
- Loading a profile applies valid Camera/DAQ values immediately when their devices are available and idle.
- If the Profile's Active Model reference resolves to a valid, unlocked v2 Model Package, that package becomes the global Active Model as part of the Profile load and the header updates after registry persistence succeeds.
- If the referenced package is valid but Active Model replacement is locked by an operation, the current Active Model remains unchanged, other readable Profile values load, and the direct operation-lock reason is shown.
- When a device is unavailable or locked by another operation, the profile may still be opened and inspected. Readable nonhardware run selections load, the authoritative applied hardware values remain unchanged, and the pre-run panel shows a read-only summary such as `Camera profile values not applied — Camera unavailable` or `DAQ profile values not applied — DAQ is in use by Sequence Test`. The user may reopen the profile or set those values after the device becomes available and idle.
- The read-only profile summary is not a second settings surface and cannot be edited; the hardware panel continues to show the authoritative applied values.
- A missing referenced model does not cause silent substitution. Other readable profile values load, the missing model is shown factually, and Class-Based Sorting remains unavailable.
- An unsupported schema is not partially interpreted; current valid configuration remains unchanged.
- No Import, Export, Delete, or managed profile list appears.

### 12.4 Send Test Sine Wave

- Send Test Sine Wave requires DAQ Ready and current applied Amplitude, Frequency, and Event Duration.
- It remains a discrete action and does not start Continuous configured waveform.
- It is disabled when DAQ is unavailable, settings are invalid, or continuous waveform output has priority.
- It may be used by the normal user.
- It issues one finite sine wave without Decision-to-trigger Delay and creates no Run or Droplet Log event.
- It does not constitute a software arming state or safety-rated Emergency Stop function.

### 12.5 Start enablement

Start Sorting requires:

- no conflicting long-running operation;
- Camera Streaming;
- DAQ Ready;
- Trigger Every Droplet selected;
- Hit boundary calibration selected;
- writable Run location;
- for Class-Based Sorting: a loadable Active Model and Hit Class;
- for Trigger Every Droplet: no model requirement.

### 12.6 Start transition

```text
PRE-RUN READY
    │ Start Sorting accepted
    ▼
STARTING
    ├── create Run ID and folder
    ├── write initial run_summary.json
    ├── open recoverable Droplet Log
    ├── retain initial effective-configuration snapshot
    ├── lock Camera acquisition, Run output, and global slot
    ├── retain editable Active Model, Hit Class, Trigger Every Droplet,
    │   DAQ Output, and hit-boundary fields
    └── keep docked Hardware available under DAQ arbitration
    ▼
RUNNING
```

The transition occurs inside the same Live workspace without navigation.

### 12.7 Running layout

```text
┌────────────────────────────────────────────┬────────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ LIVE SORTING               │
│                                            │ Status: Running            │
│                                            │ Elapsed: <time>            │
│                                            │ Trigger Every Droplet: <value>      │
│                                            │ Active Model: <value>      │
│                                            │ Hit Class: <value>         │
│                                            │ Hit boundary calibration: <value>│
│                                            │ Effective config: <id>     │
│                                            │                            │
│                                            │ Total Droplets: <count>    │
│                                            │ Predicted Class 0: <count> │
│                                            │ Predicted Class 1: <count> │
│                                            │ Predicted Class 2: <count> │
│                                            │ Decision Hit: <count>      │
│                                            │ Decision Waste: <count>    │
│                                            │ Observed Hit: <count>      │
│                                            │ Observed Waste: <count>    │
│                                            │ Unresolved: <count>        │
│                                            │ Inference Time: <value>    │
│                                            │ Camera FPS: <value>        │
│                                            │                            │
│                                            │ [ Pause ]  [ Stop ]        │
└────────────────────────────────────────────┴────────────────────────────┘
```

When no model is used, Predicted Class and Inference Time fields that depend on model inference are absent or empty; Decision and Observed Route remain present.

### 12.8 Running interaction behavior

- Live inference uses the qualified CPU path; GPU is not required.
- No confidence or score threshold changes routing.
- Every finalized event saves one Droplet Crop and one Droplet Log row.
- Record Full Image Sequence controls only full-frame TIFF retention; event crops and event data are always saved.
- Live counters distinguish:
  - Predicted Class;
  - Decision Hit/Waste;
  - Observed Route Hit/Waste/Unresolved.
- Optional nonpersistent preview overlays may appear only if implemented without adding controls or changing saved source files.
- Active Model, Hit Class, Trigger Every Droplet, DAQ Output, and hit-boundary fields remain editable. Each valid committed change applies immediately, appends a timestamped effective-configuration identity while retaining the initial snapshot, and is referenced by every affected event.
- The docked Hardware panel remains available. Continuous configured waveform follows Section 5.6.

### 12.9 Paused layout

```text
Status: Paused
Elapsed active Run time: <frozen>
Counters: <stable>

Live camera preview continues.
No inference, new DAQ output, or new event finalization occurs.

[ Resume ]  [ Stop ]
```

The initial configuration snapshot, effective-configuration history, Camera lock, Run output ownership, and global operation slot remain held. The approved Live fields and docked Hardware panel remain editable under Sections 3.7 and 5.6.

### 12.10 Stopping and finalization

Selecting Stop, reaching Duration, or encountering a terminal fault causes OpenDSS to:

1. stop new inference;
2. stop new DAQ commands;
3. stop accepting new events;
4. flush Droplet Crop, event, and optional sequence queues;
5. finalize `events.csv` and `run_summary.json` as far as technically possible;
6. release resource locks;
7. show Completed, Interrupted, or Failed presentation.

### 12.11 Completed layout

```text
Live Run completed / stopped
Status: <Completed or Stopped>
Stop reason: <value>
Total Droplets: <count>
Location: <path>

[ Open Run Summary ]  [ Open Run Folder ]
[ Start New Run ]
```

Start New Run returns to Live pre-run with the previous values available for review. It does not duplicate the prior Run or automatically start.

### 12.12 Interrupted or Failed layout

```text
┌──────────────────────────────────────────────────────────────────────┐
│ Live Sorting interrupted                                             │
│ Error                                            │
│ <whether recoverable Run data was preserved>                         │
│                                                                      │
│ [ Open Run Summary ] or [ Open Run Folder ]   [ Start New Run ]      │
└──────────────────────────────────────────────────────────────────────┘
```

Hardware faults stop new impossible or unsafe output before the UI message is presented.

### 12.13 Live state model

```text
Unavailable ⇄ Ready → Starting → Running ⇄ Paused
                                  │          │
                                  └── Stop ──┘
                                       ↓
                                    Stopping
                                  ↙     ↓      ↘
                         Completed  Interrupted  Failed
```

---

## 13. Sort > Sequence Test interaction specification

### 13.1 Ready layout

```text
Sort > Sequence Test

┌────────────────────────────────────────────┬────────────────────────────┐
│ SOURCE AND PROCESSING SUMMARY               │ SEQUENCE TEST SETUP        │
│ Active Model: <name/none>                   │ Trigger Every Droplet [OFF]│
│ [ Load Sequence ][ Load to Memory ]         │ Hit Class [ class ▼ ]      │
│ First-frame preview                         │ Processing FPS [____]      │
│ Frames / recorded FPS                       │ [ ] Physical DAQ Output    │
│ Available memory / buffer size              │ Save Location [____] [...] │
│ <selected name / load status>               │ [ Start ] [ Stop ]         │
└────────────────────────────────────────────┴────────────────────────────┘
```

Physical DAQ Output is unchecked when a new Sequence Test setup is initialized.

In the current right-panel composition, **Load Sequence** and **Load to Memory** share one row and each occupies half of the available content width. Selection and memory-loading semantics remain sequential even though the controls share a row.

### 13.2 Conditional behavior

- Class-Based Sorting requires the compatible Active Model and Hit Class.
- Trigger Every Droplet permits no Active Model; when one is present, classification is still logged.
- Hit boundary calibration is always required because Observed Route mapping remains part of the event model.
- Physical DAQ Output checked requires DAQ Ready.
- Physical DAQ Output unchecked requires no DAQ and owns no DAQ resource.
- A Camera is never required.
- Sequence Test uses the Active Model and exposes no local Model selector.
- The fixed detection, Droplet Crop, routing-algorithm, and internal timing configuration is not editable.
- Start remains disabled until a valid OpenDSS Image Sequence containing `sequence.json` is loaded into a bounded buffer.
- Memory allocation failure shows `Error`, logs detail, cancels the load, and leaves Start disabled.

### 13.3 Running layout

```text
┌────────────────────────────────────────────┬────────────────────────────┐
│ SEQUENCE PROCESSING                         │ SEQUENCE TEST STATUS       │
│ Frames processed: <n> of <total>            │ Status: Running            │
│ Events finalized: <count>                   │ Trigger Every Droplet: <value>      │
│                                             │ Physical DAQ: On / Off     │
│ Predicted Class counts when a model exists  │ Model: <name/none>         │
│ Decision Hit / Waste                        │                            │
│ Observed Hit / Waste / Unresolved           │ [ Stop Sequence Test ]     │
└────────────────────────────────────────────┴────────────────────────────┘
```

Sequence Test processes at the configured Processing FPS, defaulted from recorded FPS. Requested and achieved rates are displayed/recorded; physical DAQ output follows that schedule when enabled.

### 13.4 Completion layout

```text
Sequence Test completed / stopped
Status: <value>
Events: <count>
Location: <Run path>

[ Open Run Summary ]  [ Open Run Folder ]
[ Start Another Sequence Test ]
```

### 13.5 Interaction behavior

- Start creates a Run with operation type `sequence_test`.
- The source Image Sequence is read-only and is not modified.
- When physical output is enabled, DAQ output follows Decision.
- Observed Route is independently recorded as Hit, Waste, or Unresolved.
- Stop halts new processing and DAQ output, then finalizes the Run.
- Completion or recoverable interruption provides Open Run Summary.
- Every completed or stopped Sequence Test Run appears in Results.
- Sequence Test has no Pause state.

### 13.6 State model

```text
Empty / Unavailable ⇄ Ready → Starting → Running → Stopping
                                                  ↙   ↓    ↘
                                         Completed Interrupted Failed
```

---

## 14. Results > Runs interaction specification

### 14.1 Runs list layout

```text
Results > Runs

┌──────────────────────────────────────────────────────────────────────┐
│ LOADED RUN DETAIL                         │ RUNS                       │
│ <identity/status/count groups/matrix>      │ <selected row>             │
│ <notes/file actions/factual artifact data>│ <row>                      │
│                                            │ [ Load ]                   │
└────────────────────────────────────────────┴────────────────────────────┘
```

No separate visible **Provenance ›** disclosure row is shown. Stored scientific provenance and artifact facts remain available in their applicable factual groups and persisted artifacts.

The list contains Live Sorting and Sequence Test Runs only.

### 14.2 List interaction

- Selecting a row changes `selectedRunId` only.
- Load copies the selection into `loadedRunId` and replaces the center detail.
- Run status values are factual persisted values: Completed, Stopped, Interrupted, or Failed.
- An unreadable entry remains identifiable when possible and exposes its direct file reason.
- No Training or Model Test entries appear.

### 14.3 Selected Run layout

```text
┌────────────────────────────────────────────┬────────────────────────────┐
│ RUN SUMMARY                                │ FILES AND NOTES            │
│ Run Name / operation / status              │ [ Open Droplet Log ]       │
│ Experiment Type / timestamps / Duration    │ [ Open Run Folder ]        │
│ Stop reason / Save Location                │ [ Open Droplet Crop ]      │
│                                            │ [ Open Saved Sequence ]    │
│ Model identity and checksum when present   │   when present             │
│ Trigger Every Droplet                               │                            │
│ Hit Class when applicable                  │ Notes                      │
│ Hit boundary calibration                       │ <read-only or edit area>   │
│ Physical DAQ state for Sequence Test       │ [ Edit Notes ]             │
│                                            │ [ Save Notes ] [ Cancel ]  │
│ Camera / DAQ / fixed processing snapshot   │                            │
│                                            │                            │
│ Total Droplets                             │                            │
│ Predicted Class counts                     │                            │
│ Decision Hit / Waste                       │                            │
│ Observed Hit / Waste / Unresolved          │                            │
│                                            │                            │
│ DECISION VS. OBSERVED ROUTE                 │                            │
│            Observed Hit Waste Unresolved   │                            │
│ Decision Hit       n     n       n         │                            │
│ Decision Waste     n     n       n         │                            │
└────────────────────────────────────────────┴────────────────────────────┘
```

### 14.4 Selected Run behavior

- Run Summary is rendered from `run_summary.json`.
- Open Droplet Log opens `events.csv` through the operating system's associated application or direct file action.
- Open Run Folder opens the ordinary Windows folder.
- Open Droplet Crop opens a standard file picker rooted in the Run's Droplet Crop folder, then opens the selected referenced crop; there is no integrated per-event browser in the first release.
- Open Saved Sequence appears only when a full Image Sequence exists and opens Sequence Viewer with it preselected.
- Notes enter an explicit edit state. Save Notes atomically updates only the Notes field in `run_summary.json`; Cancel discards the current edit buffer.
- Historical event rows, counters, model/routing snapshot, and saved images remain immutable.
- Missing optional sequence is stated as **No saved Image Sequence for this Run** and does not make the entire Run unavailable.
- No first-class charts or scientific interpretation are added.

---

## 15. Settings interaction specification

### 15.1 Layout

```text
Settings

┌──────────────────────────────────────────────────────────────────────┐
│ STORAGE                                                              │
│ Default Data Root: <path>                                            │
│ [ Choose Default Data Root ]  [ Open Data Root ]                     │
│                                                                      │
│ APPLICATION INFORMATION                                              │
│ OpenDSS Version: <value>                                             │
│ Schema Versions: <values>                                            │
│ Runtime Availability: <facts>                                        │
│ Camera Driver Availability: <fact>                                   │
│ DAQ Driver Availability: <fact>                                      │
│ GPU Environment Availability: <fact>                                 │
│                                                                      │
│ DIAGNOSTICS                                                          │
│ Diagnostic Folder: <path>                                            │
│ [ Open Diagnostic Folder ]                                           │
│                                                                      │
│ VISUALS                                                              │
│ Text Size [ 100% ▼ ]                                                 │
└──────────────────────────────────────────────────────────────────────┘
```

### 15.2 Interaction behavior

- Choose Default Data Root opens a standard folder picker.
- The selected root becomes authoritative only after writability validation and preference persistence succeed.
- Failure leaves the prior valid root unchanged and shows one direct reason.
- Open Data Root and Open Diagnostic Folder invoke Windows Explorer.
- Application and runtime information is read-only.
- Camera and DAQ settings do not appear in Settings; they remain in the hardware panel.
- Text Size is one dropdown containing exactly 80%, 100%, 125%, 150%, 175%, and 200%. The default is 100%, with an approximately 22 px body-text baseline.
- No detector, crop, routing, timing, training, cloud, account, telemetry, update, or legacy-migration controls appear.

---

## 16. Primary-action enablement specification

The reason sequences below are evaluated from left to right; the first applicable reason is shown beside the disabled action.

| Primary action | Enabled when | Direct disabled-reason sequence |
|---|---|---|
| **Capture Image** | Camera Streaming; image location writable; Camera and image-write pipeline not owned by a conflicting operation. | `Another operation is active` when that operation conflicts with Camera or image writing → `Camera unavailable` → `Output folder is not writable`. |
| **Start Recording** | Global slot free; Camera Streaming; sequence location writable. | `Another operation is active` → `Camera unavailable` → `Output folder is not writable`. |
| **Start Droplet Dataset Capture** | Global slot free; Camera Streaming; fixed processing configuration loadable; Dataset location writable. | `Another operation is active` → `Camera unavailable` → `Processing configuration unavailable` → `Output folder is not writable`. |
| **Assign Class** | Dataset loaded and writable; not R-locked; at least one applicable crop selected; class definition exists. | `Dataset is in use by Training/Model Test` → `No Dataset selected` → `No Droplet Crop selected` → `Number of Classes not selected`. |
| **Start Training** | Global slot free; Dataset selected/readable; Labeled crops exist/readable; Architecture, compatible Weights, Model Name, and Save Location valid; training runtime available; temporary/final output writable. | `Another operation is active` → `No dataset selected` → `No Labeled Droplet Crops` → `Required Droplet Crop is missing` → `No Architecture selected` → `No compatible Weights selected` → `No Model Name entered` → `Save location is not writable` → `Training environment unavailable`. |
| **Retry Save** | Training completed; temporary artifacts retained; prior automatic save failed; destination now writable. | `No completed temporary artifacts` → `Save location is not writable`. |
| **Start Model Test** | Global slot free; Active Model and Dataset available/readable; same class count; eligible Labeled crops readable; output writable; automatic GPU or CPU device available. | `Another operation is active` → `No Active Model` → `No dataset selected` → factual class-count mismatch → `No Labeled Droplet Crops` → `Required Droplet Crop is missing` → `Model Test runtime unavailable` → `Output folder is not writable`. GPU absence alone is never a reason. |
| **Set Active** | Valid nonactive model selected; package not locked; current Active Model not locked against replacement. | `No Active Model` → `Selected model is already Active` → `Model is in use by <operation>`. |
| **Send Test Sine Wave** | DAQ Ready; current Amplitude, Frequency, and Event Duration valid; continuous waveform not active. | `Continuous waveform has priority` → `DAQ unavailable` → `DAQ settings are invalid`. |
| **Open Profile** | Live is in pre-run. A locked or unavailable device does not prevent inspection; its profile values simply remain unapplied and are identified directly. | `Sorting is active`. File-schema errors appear after selection. |
| **Start Sorting** | Global slot free; Camera Streaming; hit boundary selected; Run location writable; Class-Based also has loadable Active Model and Hit Class; DAQ Ready only when DAQ Output is ON. | `Another operation is active` → `Camera unavailable` → `No active model` when Class-Based → `No Hit Class selected` when Class-Based → `No hit boundary selected` → `DAQ unavailable` only when DAQ Output is ON → `Output folder is not writable`. |
| **Start Sequence Test** | Global slot free; v2 sequence selected/readable; Trigger Every Droplet and Hit boundary calibration selected; Class-Based has compatible model and Hit Class; DAQ Ready when physical output checked; Run location writable. | `Another operation is active` → `No sequence selected` → `Unsupported OpenDSS v2 sequence` → `No Trigger Every Droplet selected` → `No Active Model` when Class-Based → `No Hit Class selected` when Class-Based → `No Hit boundary calibration selected` → `DAQ unavailable` when physical output checked → `Output folder is not writable`. |
| **Load selected Run** | A discoverable Run is selected and its summary can be opened. | `No Run selected` → direct unreadable Run reason. |
| **Open Droplet Log** | Selected Run has readable `events.csv`. | `Droplet Log unavailable`. |
| **Save Notes** | Selected inactive Run is readable; Notes edit mode active; Run Summary writable. | `Run is still active` → `Notes are not being edited` → `Run Summary is not writable`. |
| **Choose Default Data Root** | Folder picker can open; selected folder validates as writable. | Validation failure is shown after selection; the prior root remains active. |

Scientific measurements, class distribution, same-Dataset reuse in Model Test, user-selected Hit Class, and user-selected Hit boundary calibration never appear in a disabled-reason sequence as judgments.

---

## 17. Shared resource ownership and navigation effects

### 17.1 Ownership matrix

| Operation or action | Camera | DAQ | Dataset | Model Package | Run output | Global slot |
|---|---:|---:|---|---|---|---:|
| Single Image | Momentary exclusive | — | — | — | — | No |
| Image Sequence | Exclusive | — | — | — | — | Yes |
| Droplet Dataset Capture | Exclusive | — | New Dataset write | — | — | Yes |
| Label | — | — | Selected Dataset write | — | — | No |
| Training | — | — | Selected Dataset read/use lock | New package write at save | — | Yes during execution |
| Model Test | — | — | Selected Dataset read/use lock | Selected package read/use lock | Model Test output only | Yes |
| Model Library mutation | — | — | — | Selected package/registry write | — | No |
| Sequence Viewer | — | — | Read-only when applicable | — | Read-only when applicable | No |
| Live | Exclusive | Exclusive | — | Selected/Active package read/use lock when present | Exclusive write | Yes |
| Sequence Test, physical output on | — | Exclusive | — | Selected package read/use lock when present | Exclusive write | Yes |
| Sequence Test, physical output off | — | — | — | Selected package read/use lock when present | Exclusive write | Yes |
| Results Notes edit | — | — | — | — | Notes-only write on inactive Run | No |

### 17.2 Foreground navigation during an operation

When a long-running operation is active:

- its owning workspace may be left and reopened without stopping it;
- the global header continues to display the operation;
- Start actions for all other long-running operations are disabled;
- the specific Dataset, Model Package, Camera, DAQ, or Run output locks remain in force;
- passive work on unowned artifacts remains available;
- the owning workspace retains the only Pause/Resume/Stop controls for that operation;
- no second workspace may present itself as another owner of the operation.

### 17.3 Conflict examples

- Training on Dataset A prevents Label from changing Dataset A but does not prohibit opening and labeling Dataset B.
- Model Test using Model A prevents Library from deleting or renaming Model A; unrelated Model Packages remain manageable.
- Live using the Active Model prevents Set Active from replacing it until the Run ends.
- Image Sequence locks Camera settings but does not lock available DAQ settings.
- Sequence Test with physical output disabled locks neither Camera nor DAQ, although it still occupies the global long-running-operation slot.
- Results may read finalized or recoverable Runs but may edit Notes only after the selected Run is inactive.

---

## 18. Contextual workflow handoffs

Contextual links perform two actions only: navigate to the approved destination and preselect the named artifact.

| Source | Link | Destination | Preselection and resulting state |
|---|---|---|---|
| Droplet Dataset Capture Completed/Interrupted with recoverable Dataset | **Open in Label** | Data > Label | Selects the `dataset.json`; destination becomes Ready or Unavailable based on the same Dataset loader. |
| Label | **Use in Train** | Models > Train | Selects the current Dataset; Architecture and compatible Weights remain separate user selections. |
| Train after successful Model save | **Open in Model Test** | Models > Model Test | Selects the new Model Package; the source Dataset may remain selected when still available. |
| Model Library | **Open in Model Test** | Models > Model Test | Selects the current Model Package without changing Active Model. |
| Image Sequence Completed | **Open in Sequence Viewer** | Data > Sequence Viewer | Selects the completed `sequence.json` and displays its first frame. |
| Image Sequence Completed | **Open in Sequence Test** | Sort > Sequence Test | Selects the completed `sequence.json`; other test selections remain explicit. |
| Live or Sequence Test post-operation | **Open Run Summary** | Results > Runs | Selects the newly finalized or recoverable Run. |
| Results selected Run with full sequence | **Open Saved Sequence** | Data > Sequence Viewer | Selects the Run's sequence. The link is absent when no full sequence exists. |

No handoff starts the destination operation, creates a project, prevents direct navigation, or changes an unrelated global selection.

---

## 19. Fault and recovery interaction catalog

| Fault context | Immediate system behavior | Workspace message | Direct actions |
|---|---|---|---|
| Camera unavailable before capture or Live Start | Do not start; keep preview region. | **Camera unavailable** beside disabled action. | Restore Camera; no modal. |
| Camera disconnect during Image Sequence or Droplet Dataset Capture | Stop acquisition; flush recoverable writes; finalize metadata if possible. | `<operation> interrupted. The camera disconnected. <preservation result>.` | Open Sequence/Dataset when recoverable; Open Folder. |
| Camera disconnect during Live | Stop inference and new DAQ commands; flush Run data. | **Live Sorting interrupted. The camera disconnected.** | Open Run Summary/Folder; Start New Run after recovery. |
| DAQ unavailable before Live or physical-output Sequence Test | Do not start or issue a sine wave. | **DAQ unavailable**. | Restore DAQ; Sequence Test may explicitly disable physical output. |
| DAQ fault during Live or physical-output Sequence Test | Stop new DAQ output and interrupt/fail operation; flush Run data. | Direct DAQ reason and preservation result. | Open Run Summary/Folder; retry after DAQ recovery. |
| Model/Dataset class-count mismatch in Model Test | Do not start. | **The selected model has 2 output classes, but the selected Dataset defines 3 classes**, or converse. | Select compatible model or Dataset. |
| Unsupported v2 schema | Do not partially load; preserve current valid selection. | **Unsupported OpenDSS v2 schema** with selected path when useful. | Select another artifact; Open Folder when useful. |
| Output folder becomes unwritable during operation | Stop new writes/process as required; preserve partial data. | Direct file-write reason; never claim completion. | Open Folder; select valid location for a new operation. |
| Label or Notes atomic save fails | Keep prior canonical JSON intact. | Direct write/permission reason and statement that the change was not saved. | Retry save or correct permissions. |
| Training runtime failure | Stop trainer; preserve diagnostics/temp output as applicable. | Direct runtime/process reason. | Open Diagnostics; Retry Training after correction. |
| Model Package mutation fails | Leave prior package/registry state authoritative. | Direct copy/rename/delete/permission reason. | Retry or choose another location. |

A hardware or persistence fault is communicated once in the relevant workspace. Repeated modal fault dialogs, notification centers, and scientific-policy language are not used.

---

## 20. Interaction acceptance checklist

### 20.1 Shell and state

- Every launch opens Data > Capture with all three Capture sections collapsed.
- The once-per-session Camera-unavailable choice is a shell-global modal overlay above the entire shell and current workspace; Capture remains selected beneath it, Yes continues without Camera with ordinary unavailable status still visible, and No closes OpenDSS.
- The header, panel, workspace, and disabled actions agree on Camera, DAQ, Active Model, and current operation state.
- Navigation during a long-running operation does not stop it or create another owner.
- Returning to the owning workspace restores its current state and controls.
- A conflicting long-running Start action shows **Another operation is active**.

### 20.2 Capture

- The three Capture section headings remain visible beside one shared preview; sections expand independently and none is open by default.
- During an accepted capture, the active section remains expanded and the other two headings are disabled.
- Single Image creates exactly one TIFF.
- Image Sequence Pause preserves the same sequence and numbering.
- Droplet Dataset Capture works without a model or DAQ and never assigns Labels, Decisions, or Predicted Classes.

### 20.3 Label and model workflows

- Label supports two and three classes with stable Class IDs and editable Class Names.
- Skip, Remove from Dataset, Restore, relabel, bulk label, and Undo are available without quality judgments.
- Training exposes Faster and More Accurate only; no Advanced Training Parameters appear.
- Training exposes GPU-default/CPU-selectable Compute Device and records requested/effective device; an unavailable GPU request falls back to CPU with a direct explanation, while a CPU request stays CPU. Model Test continues to select GPU automatically when compatible and otherwise uses CPU.
- Model Test permits the source Dataset without warning and blocks only class-count incompatibility and other technical faults.
- Successful model save makes that model Active.

### 20.4 Sorting

- Live pre-run and active sorting are states of one workspace.
- Starting Live closes and locks the hardware panel and replaces configuration with the monitoring panel.
- Trigger Every Droplet remains a first-class choice in Live and Sequence Test.
- Predicted Class, Decision, and Observed Route are displayed and recorded separately.
- Observed Route supports Hit, Waste, and Unresolved.
- Live uses Pause and Stop; Sequence Test uses Stop without adding Pause.
- Physical DAQ Output in Sequence Test is explicit and begins enabled.
- No confidence threshold, scientific approval state, or software arming state appears.

### 20.5 Results, files, and faults

- Results lists Live and Sequence Test Runs only.
- Run Summary includes the Unresolved column in Decision-versus-Observed Route.
- Setup Profiles use Open, Save, and Save As as ordinary v2 files.
- Unsupported legacy artifacts are not converted or partially interpreted.
- Every disabled primary action has one direct reason.
- Interrupted and Failed operations show one contextual banner and direct recovery actions.
- A failed write is never presented as success.

---

## 21. Explicitly absent interaction elements

The low-fidelity design shall not contain or imply:

- a Home dashboard;
- separate Sort Setup navigation;
- editable detector settings;
- editable Droplet Crop settings;
- editable routing-algorithm settings;
- editable internal timing settings;
- Advanced Training Parameters;
- CPU/GPU selection controls outside the approved Train Compute Device selector;
- confidence-threshold controls;
- scientific quality gates or class-balance warnings;
- model approval, candidate, rejected, promoted, or certified states;
- a managed Setup Profile list with Import, Export, or Delete actions;
- legacy conversion, migration, or read-only compatibility screens;
- Training or Model Test entries in Results;
- projects, accounts, roles, cloud storage, collaboration, telemetry, or update controls;
- an integrated first-release per-event Run browser;
- first-class Run charts;
- placeholder buttons for excluded features;
- repository components exposed as navigation categories;
- a GUI control labeled as a safety-rated Emergency Stop.

---

## 22. Requirements and source trace

### 22.1 Approved Product Model trace

| Source section | Interaction coverage in this document |
|---|---|
| **PM §1 — Purpose and authority** | Authority order and prohibition on repository-led navigation/UX decisions. |
| **PM §§3.1–3.5 — Governing product principles** | Technical blocking, scientific authority, terminology separation, ordinary local files, and reproducibility. |
| **PM §4 — Approved primary navigation** | Shell navigation, no Home, startup workspace, and contextual rather than mandatory handoffs. |
| **PM §5 — Global application shell** | Status header, workspace regions, shell-owned Camera/DAQ panel, immediate apply, and locking. |
| **PM §§7.1–7.3 — Capture, Label, Sequence Viewer** | Sections 6–8 low-fidelity layouts and interaction behavior. |
| **PM §§7.4–7.6 — Train, Model Test, Library** | Sections 9–11, including no Advanced Training Parameters, the GPU-default/CPU-selectable Train Compute Device, and automatic Model Test GPU/CPU behavior. |
| **PM §7.7 — Sort > Live** | Section 12 pre-run, start transition, running, paused, and post-operation presentations. |
| **PM §7.8 — Sort > Sequence Test** | Section 13 placement, optional physical output, trigger modes, and Run creation. |
| **PM §§7.9–7.10 — Results and Settings** | Sections 14–15 Runs-only review and reduced Settings. |
| **PM §§8–10 — Artifact, Setup Profile, and scientific event models** | Ordinary-file profile interactions; artifact handoffs; separate Predicted Class, Decision, and Observed Route; Unresolved. |
| **PM §11 — Editable and fixed configuration** | Hardware-only technical editing and explicit absence of detector/crop/routing/timing/training controls. |
| **PM §§12–14 — Dependencies, operation model, faults** | Enablement matrix, lifecycle state machine, resource ownership, contextual faults, and recovery actions. |
| **PM §§15–18 — Persistence, legacy, boundaries, ownership** | Atomic writes, v2-only loading, excluded interactions, and authoritative state owners. |
| **PM §19 — D-001 through D-019** | All approved decisions are implemented without reopening alternatives. |

### 22.2 Detailed Workflow Specification trace

| Source section | Retained nonconflicting interaction requirements |
|---|---|
| **WF §§4, 6 — Principles and controlled terminology** | User-directed operation, technical blocking only, ordinary files, and approved scientific terms. |
| **WF §§8–9 — Shell and operation rules** | Header values, direct disabled reasons, camera-unavailable preview, passive navigation, lifecycle, and Stop. |
| **WF §10 — Default storage** | Default folders, user-selected writable locations, timestamp fallback, and ordinary file ownership. |
| **WF §§11–13 — Capture workflows** | Single Image fields/output, Image Sequence Duration/Pause/frames, Droplet Dataset Capture counts/artifacts/recovery. |
| **WF §14 — Label** | Dataset selection, two/three classes, stable IDs, Class Names, bulk actions, Skip, Remove, Restore, Undo, Image Counts, and atomic save. |
| **WF §15 — Train** | MobileNet/Faster or EfficientNet/More Accurate Architecture, compatible Weights, fixed split/seed, GPU-default/CPU-selectable Compute Device with requested/effective status, metrics, Stop, model package, and automatic activation. Compute Device is not a hyperparameter. |
| **WF §16 — Model Test** | Model/Dataset selection, source-Dataset permission, class-count blocking, metrics, confusion matrix, and predictions CSV, as amended for GPU fallback. |
| **WF §17 — Model Library** | Set Active, Import, Export, Duplicate, Rename, Delete, metadata, and package-use protection. |
| **WF §18 — Sequence Viewer** | Supported inputs, frame-navigation controls, no hardware, no DAQ output. |
| **WF §19 — Sequence Test** | Trigger Every Droplets, explicit physical output, maximum-rate processing, no Camera, event logging, and Run creation, as relocated under Sort and amended for Unresolved. |
| **WF §§20–21 — Sorting setup and Live** | Run fields, Hit Class, Hit boundary calibration, test sine wave, optional full sequence, Live event flow, Pause, Stop, counters, persistence, and fault stop, consolidated into Live. |
| **WF §22 — Results** | Run list, Run Summary, Notes, file actions, and Decision-versus-Observed Route presentation, amended for Unresolved. |
| **WF §§24–30 — File contracts** | Canonical v2 files and artifact-selection interactions. |
| **WF §§31–34 — Persistence, recovery, errors** | Background writes, flush points, partial files, atomic JSON, crash recovery, direct errors, and no false success. |
| **WF §§36–43 — Camera-free use, provenance, nonfunctional requirements, exclusions** | Hardware-free workspaces, responsiveness, data integrity, scientific transparency, offline behavior, and excluded controls. |
| **WF §§44–72 — Acceptance scenarios** | Interaction acceptance coverage across capture, labeling, model workflows, sorting, Results, faults, and mutual exclusion. |
| **WF §§73–78 — Ownership and engineering discretion** | Single authoritative domain owners; exact Qt classes, layout measurements, and internal implementations remain engineering choices. |

---

## 23. Handoff boundary

This specification is sufficient to proceed to:

- low-fidelity clickable prototyping;
- detailed component and form-field specification;
- event/command and view-model contracts;
- application-state and resource-lock implementation design;
- interaction-level acceptance tests.

Those activities must preserve this specification, the approved information architecture, and the authority order stated at the beginning. They must not introduce alternate navigation, new scientific policy, unapproved controls, or polished visual styling as a substitute for implementation detail.

---

## Source citations

### Approved v2 Product Model

- **PM §1 — Purpose and authority:** the approved model controls D-001 through D-019, the detailed workflow supplies nonconflicting requirements, and the repository is implementation evidence only. fileciteturn0file1L12-L24
- **PM §4 — Approved navigation and startup:** final hierarchy, no Home, startup at Data > Capture with all three Capture sections collapsed, no last-workspace restore, and contextual rather than mandatory workflow links. fileciteturn0file1L114-L147
- **PM §5 — Global shell and hardware panel:** header, shell-owned Camera/DAQ panel, immediate valid changes, locking, and Live panel closure. fileciteturn0file1L151-L201
- **PM §§7.1–7.6 — Data and Models workspaces:** shared Capture preview and collapsible sections, Label, Sequence Viewer, fixed Training controls, automatic Model Test acceleration/fallback, and Model Library behavior. fileciteturn0file1L282-L426
- **PM §§7.7–7.10 — Sort, Results, and Settings:** one stateful Live workspace, Sequence Test under Sort, Runs-only Results, and reduced Settings. fileciteturn0file1L428-L586
- **PM §§8–11 — Artifacts, Setup Profiles, event semantics, and configuration boundary:** ordinary v2 files, Open/Save/Save As Profiles, Unresolved Observed Route, and Camera/DAQ-only technical editing. fileciteturn0file1L590-L785
- **PM §§12–16 — Dependencies, operation lifecycle, faults, persistence, and v2-only strategy:** technical prerequisites, one long-running slot, direct recovery, and no migration UI. fileciteturn0file1L789-L946
- **PM §§17–20 — First-release boundaries, state ownership, decision register, and amendments:** excluded UI, authoritative owners, D-001 through D-019, and required workflow amendments. fileciteturn0file1L950-L1046

### Detailed User Workflow Specification

- **WF §§7–9 — Navigation, shell, and operation rules:** persistent workspaces, global status, disabled reasons, hardware-unavailable behavior, concurrency, lifecycle, and Stop. fileciteturn0file0L288-L459
- **WF §§11–14 — Data workflows:** Single Image, Image Sequence, Droplet Dataset Capture, and Label behavior and artifacts. fileciteturn0file0L510-L1028
- **WF §§15–17 — Model workflows:** Training, Model Test, and Model Library requirements retained where not superseded by the approved model. fileciteturn0file0L1034-L1469
- **WF §§18–19 — Sequence review and testing:** Sequence Viewer, Trigger Every Droplets, optional physical DAQ output, processing, and Run creation. fileciteturn0file0L1473-L1657
- **WF §§20–22 — Sorting and Results:** run fields, Hit Class, Hit boundary calibration, Send Test Sine Wave, Live actions/counters/faults, and Run review. fileciteturn0file0L1661-L2259
- **WF §§23–30 — Settings and file contracts:** Settings requirements and canonical Dataset, Sequence, Model, Run, Droplet Log, and Setup Profile contracts as amended by the approved model. fileciteturn0file0L2263-L2680
- **WF §§31–34 — Persistence, recovery, and errors:** background writes, flush points, atomic JSON, crash recovery, and plain-language errors. fileciteturn0file0L2684-L2829
- **WF §§35–43 — Launch, camera-free use, provenance, nonfunctional requirements, and exclusions:** offline startup, hardware-free workspaces, reproducibility, responsiveness, integrity, and first-release boundaries. fileciteturn0file0L2833-L3032
- **WF §§44–78 — Acceptance and ownership:** end-to-end acceptance scenarios, authoritative services, contract tests, repository alignment, and engineering discretion. fileciteturn0file0L3036-L3419
