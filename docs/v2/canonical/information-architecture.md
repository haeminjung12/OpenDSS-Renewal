# OpenDSS v2 Information Architecture and Screen Inventory

**File:** `information-architecture.md`  
**Version:** 1.1  
**Date:** July 23, 2026  
**Status:** Current canonical baseline  
**Authority:** The Approved v2 Product Model controls D-001 through D-019. The July 23, 2026 UI/UX Design Amendment controls the UI, layout, naming, interaction, and workflow matters it explicitly changes. The Detailed User Workflow Specification supplies requirements that do not conflict with those sources. The existing repository is not an authority for navigation or UX.

This document uses the simplest interpretation consistent with the approved product model. It defines information architecture, workspaces, states, contextual handoffs, and ownership/lock behavior. It does not add a Home screen, a wizard, scientific quality gates, or unapproved controls.

---

## 0. Amendment-controlled screen inventory

This section is the current compact inventory for the July 23, 2026 amendment. Later detailed sections remain applicable only where they agree with it.

| Surface | Current composition and interaction |
|---|---|
| **Shell** | Maximized startup; no fixed minimum or restored-window validation target; content fills the maximized window's full available work area through the bottom; compact one-line status header; Hardware docked at the bottom of the resizable left navigation. |
| **Data > Capture** | Shared Camera preview plus fixed Single Image, Image Sequence, and Droplet Dataset Capture headings with independently collapsible bodies. |
| **Data > Label** | Dominant Droplet Crop grid; approximately 536 px-at-100%-Text-Size outer-collapsible right panel ordered as Load Dataset, Dataset Summary, Label, Filter, and bottom-right Save As; configured Datasets may switch between two and three classes. |
| **Data > Sequence Viewer** | Current/total frame, Previous/Next, direct seek, zoom, pan, Fit, and 1:1; no automatic playback controls or lifecycle. |
| **Models > Train** | Pre-start Dataset, an existing Library model selected read-only for Name, Architecture, and Starting Weights, Compute Device, and Output Location; GPU requested by default with CPU selectable; requested/effective device status; Dataset Summary in a main white region; separate main white Results region below for live progress, the two approved plots, and completion tables; atomic creation and activation of a new named package without mutating the source model. |
| **Models > Model Test** | Uses the Active Model with no local Model selector; Dataset Summary in a main white region and a separate main white Results region below for approved metrics, confusion matrix, and prediction summaries. |
| **Models > Library** | Minimal Model rows; explicit Set Active; Add Model and complete-package Import Model; confirmed Remove Model to the Recycle Bin subject to active, in-use, registry, and package-integrity locks; collapsible Selected Model panel. |
| **Sort > Live** | Camera preview with Start Camera and Start Sorting below it; Setup Profile, Run Information, Trigger & Timing, Output & Recording, and Running collapsible sections. |
| **Sort > Sequence Test** | Live panel language without Camera controls; dedicated Sequence Test section, bounded memory load, editable Processing FPS, and optional physical DAQ checkbox off by default. |
| **Results > Runs** | Loaded Run in the center; right-side collapsible Runs list with explicit Load button. |
| **Settings** | Centered Storage, Application Information, Diagnostics, and Visuals only; Text Size offers exactly Small (80%), Medium (100%, default), and Large (125%); 200% is validation-only. At Medium, body text, standard control text, and button text use 16 px. Body and standard controls retain approximately 20 px line height; buttons retain approximately 18–20 px line height. Ordinary field and settings labels use 15 px with approximately 18 px line height. Captions, status, warning, and metadata use 13 px with approximately 16–18 px line height. |

The navigation hierarchy remains `Data`, `Models`, `Sort`, `Results`, and `Settings`; there is no Home or separate Sort Setup workspace.

Contextual handoffs use the revised destinations and Active Model policy:

- completed Image Sequence → Open in Sequence Viewer or Open in Sequence Test;
- completed Droplet Dataset Capture → Open in Label;
- Train save success → Active Model confirmation and optional Open in Model Test;
- Library → Set Active before opening Model Test or Sequence Test with another Model;
- completed Live or Sequence Test Run → Open in Results;
- all technical diagnostic detail → Settings > Diagnostics.

The Hardware panel is the only shell-level Camera/DAQ settings surface. It is docked at the bottom of the resizable left-navigation column, matches that column's width, expands upward within it, opens and closes through a right-aligned chevron, shows no redundant visible Close action, and follows the ownership and arbitration rules defined by the interaction-state specification.

## 1. Application shell

### 1.1 Shell composition

The OpenDSS v2 shell has five persistent structural elements:

1. **Global status header** — a compact, always-visible projection of authoritative application state.
2. **Primary navigation** — direct domain navigation for Data, Models, Sort, Results, and Settings.
3. **Workspace region** — the main content area for the selected workspace or Capture mode.
4. **Operation-side panel** — a workspace-owned right panel for current inputs, status, counters, and actions.
5. **Bottom Hardware panel** — a shell-owned slide-out panel containing the only user-editable technical settings.

A single contextual fault banner appears inside the affected workspace when an operation is interrupted or fails. OpenDSS does not use a notification center or repeated modal fault sequence.

### 1.2 Global status header

The header displays these four status areas:

| Status area | Values or presentation |
|---|---|
| **Camera** | `Unavailable`, `Connected`, or `Streaming` |
| **DAQ** | `Unavailable`, `Ready`, or `Active` |
| **Active Model** | `No Active Model` or the current Model Name |
| **Current Activity** | `Idle`, `Capturing Image`, `Recording Sequence`, `Droplet Dataset Capture`, `Labeling`, `Training`, `Testing Model`, `Testing Sequence`, `Sorting`, or `Paused` |

The header remains visible during every workspace and while the hardware panel is closed. It is a projection of authoritative domain state, not a second editable source of truth.

### 1.3 Primary navigation

Primary navigation is persistent and directly opens the selected domain workspace. It does not encode a mandatory workflow sequence and does not contain a Home item.

### 1.4 Workspace region

The workspace region contains the primary scientific or operational content:

- a live camera preview in Capture and Live;
- a crop browser in Label;
- a sequence viewer in Sequence Viewer;
- selectors, progress, and metrics in Train and Model Test;
- model list and metadata in Library;
- Run list and Run Summary in Results;
- reduced storage, application information, and diagnostics content in Settings.

When a camera-dependent workspace is unavailable, its preview region remains in place and displays **Camera unavailable** rather than changing to a named alternate mode.

### 1.5 Operation-side panel

The operation-side panel is part of the current workspace, not part of primary navigation.

- In **Capture**, it contains three independently collapsible sections for Single Image, Image Sequence, and Droplet Dataset Capture. Multiple idle sections may be expanded. During any active Single Image, Image Sequence, or Droplet Dataset Capture operation, that section remains expanded and cannot be collapsed while the other two headings stay visible but disabled. After Completed, Interrupted, or Failed, the other headings re-enable and the result section remains expanded until the user collapses it.
- In **Live**, it contains run configuration before Start, then is replaced by the live sorting monitor when sorting begins.
- In **Train**, **Model Test**, and **Sequence Test**, it contains artifact selection, start/stop actions, progress, and completion actions.
- In **Label**, **Library**, and **Results**, it contains the selected-item details and actions appropriate to that workspace.

The exact outer-panel titles are **Capture**, **Label**, **Train**, **Model Test**, **Library**, **Live**, **Sequence Test**, and **Runs**. Each uses a light top strip with its title left-aligned while expanded and the existing narrow chevron toggle fixed on the panel's right edge at vertical center, with the same screen x/y position in expanded and collapsed states. The outer toggle is a fixed **28 × 36 px** icon control with a **14 px** chevron and does not grow with Text Size. When collapsed width is insufficient, only the fixed chevron may remain visible. The directly draggable default width is approximately **536 px at 100% Text Size**, not fixed, and scales with Text Size. Settings has no outer collapsible panel.

The panel must not duplicate Camera or DAQ technical controls.

### 1.6 Docked Hardware panel

The shell-level panel contains:

```text
Hardware Configuration
├── Camera
└── DAQ
    └── Output Configuration
        ├── Output Channel
        ├── Amplitude
        ├── Frequency
        ├── Event Duration
        ├── Decision-to-trigger Delay
        └── Continuous configured waveform
```

**Camera** and **DAQ** are titled collapsible groups. **Output Configuration** is nested inside DAQ and contains the listed output controls. This is visual/product hierarchy only; actual clickable group collapse must use the existing DESIGN/FUNCTIONAL seam and does not authorize new runtime handlers.

It is the only location for user-editable Camera and DAQ technical settings. Values are shared across workspaces; individual workspaces and Settings do not maintain duplicate copies. **DAQ Output Channel** is a DAQ setting in this panel and remains distinct from **Hit boundary calibration**. Continuous configured waveform Start/Stop and factual output state also reside in Hardware > DAQ and use the same physical channel as event-triggered finite sine waves.

Hardware > DAQ exposes Amplitude from 0–10 Vpp (default 5 Vpp, centered at 0 V with extrema `-Vpp/2` and `+Vpp/2`, increment 1 Vpp), Frequency from 1–1000 kHz (default 10 kHz, increment 1 kHz), Event Duration from 1–500 ms (default 5 ms, increment 1 ms), and Decision-to-trigger Delay from 0–500 ms (default 0 ms, increment 1 ms). Amplitude and Frequency apply to continuous, event-triggered, and test sine output. Event Duration applies to event-triggered and test finite sine waves, not continuous output. Decision-to-trigger Delay is measured from accepted `Decision = Hit` and applies only to decision-triggered output.

Valid changes apply immediately when the corresponding device is available and permitted by the operation-arbitration rules. During continuous output, supported Amplitude and Frequency edits retune immediately. Unsupported values or live retunes are rejected with a direct explanation, and the last successfully applied value remains active. Event Duration and Decision-to-trigger Delay changes affect only future, not-yet-issued event output. Send Test Sine Wave uses current Amplitude, Frequency, and Event Duration without Decision-to-trigger Delay and creates no Run or event. There is no separate Apply workflow and no separate software arming state.

This authority synchronization does not implement or authorize changes to protected NI-DAQmx mechanics. Such changes require a separate functional work order and the repository's protected-asset characterization, regression, performance, hardware-in-the-loop, justification, and rollback evidence.

The following are not panel controls:

- droplet-detection parameters;
- Droplet Crop parameters beyond the fixed artifact contract;
- routing-algorithm parameters;
- internal tracking or synchronization timing;
- training hyperparameters.

Those values are fixed qualified application configuration and are recorded in provenance where required.

### 1.7 Panel interaction conditions

These are panel interaction conditions, not additional Camera or DAQ status values.

| Condition | Panel behavior | Control behavior | User-facing explanation |
|---|---|---|---|
| **Idle** | Panel may be opened or closed. Its ordinary open/closed presentation may persist during navigation. | The available device section is editable. A valid change applies immediately. | No message is required. |
| **Unavailable** | Panel can open so the unavailable device and its status remain visible. | The unavailable device section is disabled. The other device section remains independently usable if available and idle. | **Camera unavailable** or **DAQ unavailable**. |
| **Locked** | The panel remains docked and may be opened. A device field that the current operation does not permit remains read-only. | Camera acquisition fields lock when owned. DAQ controls follow the explicit arbitration rules rather than a blanket ownership lock. | For example, **Camera settings are locked while Droplet Dataset Capture is active**. |
| **Active** | During Live or Sequence Test, the panel remains available and factual status continues to update. | Continuous configured waveform may start while the operation owns DAQ. It has priority over event-triggered finite sine waves; suppressed output is recorded as not issued and discarded rather than queued. Supported Amplitude/Frequency edits retune it immediately. | An invalid or unavailable action or live retune shows the direct factual reason and retains the last applied value. |

After the owning operation ends, the corresponding controls return to their current availability state.

### 1.8 Startup workspace

Every application launch opens:

```text
Data > Capture
```

OpenDSS does not remember or restore the previously opened workspace.

When Camera connection is unavailable, the once-per-session `Camera unavailable. Continue?` prompt is a shell-global modal overlay above the entire shell and current workspace while `Data > Capture` remains selected beneath it. Yes continues without Camera and leaves ordinary unavailable status visible; No closes OpenDSS.

### 1.9 Low-fidelity shell diagram

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│ Camera | DAQ | Active Model | Current Activity                  [ Hardware ]      │
├──────────────────┬──────────────────────────────────────┬────────────────────────┤
│ PRIMARY NAV      │ WORKSPACE REGION                     │ OPERATION-SIDE PANEL   │
│                  │                                      │                        │
│ Data             │ Live preview / crop browser /        │ Inputs, selected-item │
│ Models           │ sequence viewer / metrics /          │ details, status,      │
│ Sort             │ Run content / settings content       │ counters, actions     │
│ Results          │                                      │                        │
│ Settings         │                                      │                        │
│ ┌──────────────┐ │                                      │                        │
│ │ Hardware   ˄ │ │                                      │                        │
│ │ Camera / DAQ │ │                                      │                        │
│ └──────────────┘ │                                      │                        │
├──────────────────┴──────────────────────────────────────┴────────────────────────┤
│ One minimal `Error` presentation appears inside the affected workspace when needed.  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Navigation hierarchy

### 2.1 Approved hierarchy

```text
Data
├── Capture
│   ├── Single Image
│   ├── Image Sequence
│   └── Droplet Dataset Capture
├── Label
└── Sequence Viewer

Models
├── Train
├── Model Test
└── Library

Sort
├── Live
└── Sequence Test

Results
└── Runs

Settings
```

`Single Image`, `Image Sequence`, and `Droplet Dataset Capture` are three collapsible operation sections inside one shared Capture workspace. All three headings remain fixed and visible, sections expand independently, multiple sections may be open while idle, and none is expanded by default. Expanded bodies divide the remaining panel height equally and scroll independently. An active section remains expanded while the other headings are disabled; its result remains expanded after completion, interruption, or failure. The sections are not navigation items. Live pre-run, running, paused, and completed/interrupted presentations are states of one Live workspace, not separate navigation items.

### 2.2 Navigation item purposes

| Navigation item | Purpose |
|---|---|
| **Data** | Groups source-image acquisition, Dataset labeling, and visual review of recorded Image Sequences. |
| **Data > Capture** | Provides one shared live-camera preview and three collapsible right-panel capture sections. |
| **Data > Capture > Single Image** | Captures and saves exactly one full-frame TIFF image. |
| **Data > Capture > Image Sequence** | Records an ordered full-frame TIFF sequence and finalizes `sequence.json`. |
| **Data > Capture > Droplet Dataset Capture** | Records a full sequence while fixed qualified processing creates one unlabeled Droplet Crop per detected droplet and finalizes `dataset.json`. |
| **Data > Label** | Defines two or three classes and assigns stable Class IDs to Droplet Crops in one Dataset. |
| **Data > Sequence Viewer** | Navigates frame by frame, seeks directly, zooms, pans, fits, and shows 1:1 without hardware or automatic playback. |
| **Models** | Groups model creation, observational testing, and local Model Package management. |
| **Models > Train** | Trains one existing Library-defined two-class or three-class model selected read-only for identity, architecture, and initialization, creating a new named Model Package without mutating the source. |
| **Models > Model Test** | Applies a selected model to a compatible labeled Dataset and reports classification measurements without changing Active Model state. |
| **Models > Library** | Creates model identities, imports and manages complete valid OpenDSS v2 Model Packages, and controls the one global Active Model. |
| **Sort** | Groups live physical sorting and reprocessing of recorded Image Sequences through sorting logic. |
| **Sort > Live** | Combines pre-run configuration, active sorting, Pause, Stop, and post-run actions in one stateful live-camera workspace. |
| **Sort > Sequence Test** | Processes a recorded Image Sequence through fixed detection/crop processing, routing logic, visual route tracking, optional model inference, optional physical DAQ output, and Run persistence. |
| **Results** | Contains persisted outputs from sorting operations only. |
| **Results > Runs** | Lists and reviews Live Sorting and Sequence Test Runs, including Run Summary, Droplet Log, Droplet Crops, and an optional saved Image Sequence. |
| **Settings** | Provides Storage, Application Information, Diagnostics, and Visuals without duplicating Camera or DAQ controls; Text Size offers exactly Small (80%), Medium (100%, default), and Large (125%); 200% is validation-only. At Medium, body text, standard control text, and button text use 16 px. Body and standard controls retain approximately 20 px line height; buttons retain approximately 18–20 px line height. Ordinary field and settings labels use 15 px with approximately 18 px line height. Captions, status, warning, and metadata use 13 px with approximately 16–18 px line height. |

---

## 3. Complete workspace inventory

| Workspace or distinct section | User goal | Main content | Primary action | Secondary actions | Required artifact or hardware | Output artifact | Next likely action |
|---|---|---|---|---|---|---|---|
| **Data > Capture > Single Image** | Save one full camera frame. | Shared live preview; collapsible Single Image section; File Name; Save Location; saved-path feedback. | **Capture Image** | Change File Name or Save Location; expand or collapse Capture sections. | Camera Streaming; writable image location; no conflicting operation. | One TIFF file; no Dataset, model output, DAQ output, or Run. | Capture another image or expand another Capture section. |
| **Data > Capture > Image Sequence** | Record ordered full-frame source data. | Shared live preview; Name; Experiment Type; Notes; optional Duration; Save Location; elapsed time; frame count; recording status. | **Start Recording** | Pause; Resume; Stop; open completed sequence in Sequence Viewer; send completed sequence to Sequence Test. | Camera Streaming; writable sequence location; global long-running-operation slot. | `sequence.json` and numbered full-frame TIFF files. | **Open in Sequence Viewer** or **Open in Sequence Test**. |
| **Data > Capture > Droplet Dataset Capture** | Create one OpenDSS Dataset without model-dependent selection or labeling. | Shared live preview; Dataset Name; Experiment Type; Notes; optional Duration; Save Location; elapsed time; frame count; detected-droplet count; saved Droplet Crop count. Fixed detection and crop processing are active but not editable. | **Start Droplet Dataset Capture** | Pause; Resume; Stop; open the completed Dataset in Label. | Camera Streaming; writable Dataset location; loadable fixed qualified processing configuration; global long-running-operation slot. No model or DAQ. | `dataset.json`, a full-frame sequence, and one 64 × 64 grayscale PNG Droplet Crop per detection, initially Unlabeled. | **Open in Label**. |
| **Data > Label** | Select two or three classes, switch a configured Dataset between them, and assign stable Class IDs to Droplet Crops. | Dominant crop grid; approximately 536 px-at-100%-Text-Size outer-collapsible right panel ordered as Load Dataset, Dataset Summary, Label, Filter, and bottom-right Save As. Dataset Summary shows total/labeled counts and the switchable two-or-three-class selection. Label shows a vertically adjustable selected-crop preview that can be enlarged or reduced and exactly Class 0, Class 1, Class 2, Exclude, Undo, Previous, and Next; all three Class actions remain visible and Class 2 is disabled for a two-class Dataset. Filter shows class counts plus Excluded and Unreviewed as applicable. | **Assign Class 0, Class 1, or Class 2** to the selected crop; **Exclude** when applicable. | Switch the class schema with explicit resolution of incompatible existing labels; relabel; Undo; Previous/Next; filter; Save As; open in Train. | Readable v2 `dataset.json`; referenced crops readable; selected Dataset not locked by Training or Model Test. No hardware. | Normal changes update the current `dataset.json`; a schema switch never silently reassigns or discards labels; Save As creates and loads an independent Dataset copy, not a version-history entry. | **Use in Train**. |
| **Data > Sequence Viewer** | Inspect a recorded Image Sequence visually. | Current frame; frame number; total frame count; direct seek; zoom; pan; Fit; 1:1. | **Open Sequence** when empty; **Next Frame** when ready. | Previous/Next; seek; zoom; pan; Fit; 1:1. | Standalone `sequence.json`, a Dataset-referenced sequence, or a Run sequence. No camera, DAQ, model, or training environment. | No new scientific artifact and no DAQ output. | Continue visual review or navigate directly to another workspace. |
| **Models > Train** | Train one existing Library-defined two-class or three-class droplet-classification model into a new named package without mutating the source. | Dataset Summary in a main white region; selected Library model with Name, Architecture (`MobileNetV3-Small` or `EfficientNet-B0`), and Starting Weights shown read-only; Compute Device with GPU selected by default and CPU selectable; requested/effective device status; Output Location; separate main white Results region below for progress/timing, Training/Validation Loss plot, Validation Accuracy plot, and completion tables. | **Start Training** after all pre-start fields are valid. | Stop Training; Retry Save after final-save failure; open the saved Active Model in Model Test or Library. | Compatible labeled Dataset; existing compatible Library model; bundled training environment; writable temporary and final locations; global long-running-operation slot. A GPU request may fall back to effective CPU with a direct explanation; a CPU request stays CPU. | Atomically created new named Model Package containing `metadata.json`, `checkpoint.pth`, and `model.onnx`, including requested and effective device; successful save makes it Active, while the source model remains unchanged. | **Open in Model Test** or review in **Models > Library**. |
| **Models > Model Test** | Measure classification behavior of the Active Model on one compatible labeled Dataset. | Read-only Active Model; Dataset Summary in a main white region; class compatibility and automatic execution-device status; separate main white Results region below for progress, Overall Accuracy, Per-Class Accuracy, Confusion Matrix, and prediction summaries. | **Start Model Test** | Stop Model Test; open/export predictions CSV; select another compatible Dataset. | Active two-class or three-class Model Package; compatible labeled Dataset; writable output; global slot. | `model_test_summary.json` and per-image `predictions.csv`; not a Run and not listed in Results. | Review/export results or Set Active in Library before testing another Model. |
| **Models > Library** | Create and manage valid OpenDSS v2 Model Packages and choose the global Active Model. | Model list; Model Name; Architecture (`MobileNetV3-Small` or `EfficientNet-B0`); Starting Weights identity/checksum; class count; Class IDs and Class Names; creation date; source Dataset; Active state; package location; training metadata and metrics. | **Add Model** or **Set Active** | Import Model; Remove Model; Export Model; Duplicate Model; Rename Model; **Open in Model Test**. | Add Model requires a nonblank unique Name, a supported Architecture, and approved Starting Weights. Import accepts only a complete supported package selected through `metadata.json`. No hardware. Active, in-use, registry, or package-integrity locks block conflicting removal or mutation. | ModelRegistry updates and, depending on action, a created identity, imported complete package, copied, renamed, duplicated, exported, or confirmed move of the OpenDSS-owned complete package folder to the Windows Recycle Bin. The application performs no model conversion and accepts no raw weights or bare ONNX file. | **Open in Model Test** or open **Sort > Live** with the selected Active Model. |
| **Sort > Live — pre-run** | Configure and start a Live Sorting Run while viewing the live camera. | Camera preview; Start Camera/Start Sorting action bar; Setup Profile, Run Information, Trigger & Timing, Output & Recording, and collapsed Running sections. Trigger Every Droplet and DAQ Output are independent toggles; hit boundary calibration is visible on the preview. | **Start Sorting** | Profile actions; Send Test Sine Wave; change pre-run selections; open Hardware panel while idle. | Camera Streaming; hit boundary; writable Run location; global slot. Class-Based Sorting requires Active Model and Hit Class. DAQ Ready is required only when DAQ Output is ON. | New Run folder, initial `run_summary.json`, recoverable Droplet Log, and configuration snapshot. | **Sort > Live — running**. |
| **Sort > Live — running** | Monitor physical sorting, adjust the approved effective configuration, and intervene with Pause or Stop. | Live camera view; Run status; elapsed time; editable Active Model, Hit Class, Trigger Every Droplet, DAQ Output, and hit-boundary fields; current effective-configuration identity; Total Droplets; Predicted Class counts when a model is present; Decision Hit/Waste; Observed Hit/Waste/Unresolved; Inference Time; camera FPS. The docked Hardware panel remains available. | **Pause** | **Stop**. | Open Live Run owning Camera, DAQ, Run output, global slot, and current model use lock when applicable. GPU is not required. | Initial configuration snapshot, timestamped effective-configuration history, incrementally persisted Droplet Crops/events, updated `run_summary.json`, and optional full Image Sequence; every event identifies its effective configuration. | Adjust an approved field, Pause, Stop, allow Duration completion, or transition on fault. |
| **Sort > Live — paused** | Hold the same Run without inference, event-triggered finite sine waves, or new event finalization. | Live camera preview remains active; status `Paused`; elapsed active time and counters remain stable; the approved editable configuration and docked Hardware panel remain available. | **Resume** | **Stop**. | The same open Run and owned Camera/DAQ resources. | Flushed recoverable Run state; accepted configuration changes remain timestamped; no new events while paused. | Return to running or finalize the Run. |
| **Sort > Live — completed/interrupted** | Review the immediate outcome and move to persisted Results or start another Run. | Final status; stop reason or one minimal `Error` presentation; whether recoverable data was preserved; direct actions. | **Open Run Summary** when available; otherwise **Open Run Folder**. | Open Run Folder; Start New Run; direct recovery action shown by the fault banner. | Finalized or recoverable Run folder. | Completed, user-stopped, Interrupted, or Failed Run artifacts as available. | **Results > Runs — selected Run** or **Start New Run**, which returns to Live pre-run. |
| **Sort > Sequence Test** | Reprocess a valid OpenDSS Image Sequence through sorting logic and optionally issue physical DAQ output. | Active Model context; custom sequence picker; first frame/count/recorded FPS; editable Processing FPS; available memory; bounded buffer; Load to Memory; load status; unchecked Physical DAQ Output; progress and achieved FPS. | **Load to Memory**, then **Start** after success. | Stop; change Processing FPS before Start; enable physical DAQ; open completed Run in Results. | Folder containing `sequence.json`; successful bounded-buffer allocation; writable Run location; global slot. DAQ Ready only when physical output is enabled. No camera. | Run folder with `run_summary.json`, `events.csv`, Droplet Crops, source reference, Processing FPS, and achieved FPS. | **Open Run Summary** in Results. |
| **Results > Runs — list** | Find a Live Sorting or Sequence Test Run. | Run list showing Run Name, operation type, start timestamp, duration, persisted status (`Completed`, `Stopped`, `Interrupted`, or `Failed`), model name when present, and Total Droplets. | **Load selected Run** | Select another Run. | Discoverable Live Sorting or Sequence Test Run folders. No hardware. | No new artifact. | **Results > Runs — selected Run**. |
| **Results > Runs — selected Run** | Review factual Run provenance and outputs without changing historical event data. | Run information; model/routing snapshot; hardware and fixed processing configuration; Total Droplets; Predicted Class counts; Decision counts; Observed Hit/Waste/Unresolved counts; Decision-versus-Observed Route matrix with an Unresolved column; Notes; file links. | **Open Droplet Log** | Open Run Folder; open a referenced Droplet Crop; Open Saved Sequence when present; Edit Notes; Save Notes. | Readable `run_summary.json`; related Run files as available. No hardware. | Notes updates in `run_summary.json`; historical events remain unchanged. | **Open Saved Sequence** in Sequence Viewer when a full sequence exists. |
| **Settings** | Manage storage and the sole visual preference while inspecting local application/runtime information. | Storage; Application Information; Diagnostics; Visuals; default data root; OpenDSS and schema versions; runtime and driver availability; diagnostic-folder access; one Text Size dropdown with exactly Small (80%), Medium (100%), and Large (125%). Medium is the default; 200% is validation-only and not selectable. At Medium, body text, standard control text, and button text use 16 px. Body and standard controls retain approximately 20 px line height; buttons retain approximately 18–20 px line height. Ordinary field and settings labels use 15 px with approximately 18 px line height. Captions, status, warning, and metadata use 13 px with approximately 16–18 px line height. | **Choose Default Data Root** | Open data root in Windows Explorer; open diagnostic folder; set Text Size. | No hardware. A selected data root must be a valid writable location before it becomes the default. | Updated SettingsRepository storage or Text Size preference state; no scientific artifact. SettingsRepository and the UI integration must expose only 80, 100, and 125. Before publication or persistence, legacy persisted values must normalize as 90 → 100 and 150/175/200 → 125. The UI must never silently expose an unsupported value. | Return directly to any workspace. |


### 3.1 Shared sorting semantics

| Trigger Every Droplet | Model behavior | Decision behavior | Observed Route behavior |
|---|---|---|---|
| **Class-Based Sorting** | A model is required. The largest Class Score determines Predicted Class. | Predicted Class equal to Hit Class produces **Decision = Hit**; all other predictions produce **Decision = Waste**. | Visual trajectory is evaluated independently as **Hit**, **Waste**, or **Unresolved**. |
| **Trigger Every Droplet** | A model is optional. When present, classification still runs and Predicted Class and Class Scores are logged; when absent, those fields remain empty. | Every detected droplet produces **Decision = Hit**. Classification does not control the Decision. | Visual trajectory is still evaluated independently as **Hit**, **Waste**, or **Unresolved**. |

Physical DAQ output follows the Decision only in operations where physical output is enabled and the DAQ is Ready.

All physical DAQ output is sine-wave output. Operation-requested output is an event-triggered finite sine wave. Continuous configured waveform is a shared Hardware > DAQ action on that same physical channel and may start while Live or Sequence Test owns DAQ. While active, it has priority and suppresses operation-requested event-triggered finite sine waves without stopping processing, classification, decisions, or logging; each suppressed output is recorded as suppressed/not issued and discarded rather than queued. Event output resumes after it stops only when DAQ Output is enabled and DAQ is Ready. Its explicit Stop, application exit, disconnect, or fault stops and resets it to 0 V. HardwareCoordinator owns the single factual output state, and OperationCoordinator owns arbitration.

### 3.2 Setup Profile behavior in Live pre-run

- A Setup Profile is one ordinary OpenDSS v2 file operated through **Open Profile**, **Save Profile**, and **Save Profile As**.
- A Profile may carry Camera settings, DAQ settings, Active Model reference, Trigger Every Droplet, Hit Class, Hit boundary calibration, Record Full Image Sequence, Run Name, and default Save Location.
- Opening a Profile loads all readable values. OpenDSS does not silently substitute a missing model; Class-Based Sorting remains unavailable until a valid model is selected, while Trigger Every Droplet may proceed without one.
- Fixed detector, crop, routing-algorithm, and internal timing configuration is not an editable Profile field.

---

## 4. Workspace state inventory

### 4.1 State-use rules

- **Empty** means the workspace has no selected artifact yet; it is not an error.
- **Unavailable** means the requested action is technically blocked by hardware, file compatibility, resource ownership, or another direct prerequisite.
- **Ready** means the workspace has sufficient prerequisites for its primary operation.
- **Starting**, **Running**, **Paused**, and **Stopping** are used only where the operation genuinely has those lifecycle phases.
- **Completed** means the operation finalized normally. A user Stop that finalizes cleanly may be represented here while the persisted Run records its specific stop reason or `Stopped` status.
- **Interrupted** means an operation ended before normal completion and recoverable output may exist.
- **Failed** means a technical error prevented normal completion or a requested file mutation.
- A disabled action presents one direct reason. OpenDSS does not add a separate readiness checklist.

### 4.2 Data > Capture > Single Image

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Unavailable** | The preview remains visible. With no camera it reads **Camera unavailable**; otherwise the current file or resource blocker is shown beside Capture Image. | **Capture Image** disabled. | **Camera unavailable**, **Output folder is not writable**, or **Another operation is active**. | Connect/restore the camera, select a writable location, or stop the conflicting operation. |
| **Ready** | Live preview, File Name, Save Location, and normal Camera status. | **Capture Image** enabled. | — | Capture one frame. |
| **Completed** | The saved TIFF path is confirmed without opening another workflow. | **Capture Image** enabled again. | — | Capture another image or use the confirmed path outside OpenDSS. |
| **Failed** | `Error`; technical details are logged. | **Capture Image** enabled only after the blocker clears. | The current camera or file-write reason. | Restore the camera or choose a writable location, then capture again. |

### 4.3 Data > Capture > Image Sequence

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Unavailable** | Live-preview region plus setup fields; the blocking condition is shown at Start Recording. | **Start Recording** disabled. | **Camera unavailable**, **Output folder is not writable**, or **Another operation is active**. | Restore the prerequisite. |
| **Ready** | Streaming preview and editable metadata, optional Duration, and Save Location. | **Start Recording** enabled. | — | Start the sequence. |
| **Starting** | Sequence folder creation and writer initialization; the Image Sequence section remains expanded and the other Capture headings are disabled. | **Stop** enabled once the operation has been accepted. | Start Recording is disabled because **Recording is starting**. | Continue into Running or stop. |
| **Running** | Preview, elapsed time, frame count, and recording status; the other Capture headings remain visible but disabled. | **Pause** enabled. | Other Capture actions are disabled because **Recording is active**. | Pause, Stop, or allow Duration to expire. |
| **Paused** | Preview continues; frame writing and active-recording time stop; the same sequence remains open. | **Resume** enabled. | Start Recording is disabled because **The current sequence is paused**. | Resume or Stop. |
| **Stopping** | New frame writes have stopped and `sequence.json` is being finalized. | No new primary action. | **Sequence is being finalized**. | Continue to Completed, Interrupted, or Failed presentation. |
| **Completed** | Final frame count, stop reason, saved location, and direct sequence actions. | **Open in Sequence Viewer** enabled. | — | Open in Sequence Viewer or Sequence Test. |
| **Interrupted** | `Error`; technical details are logged. | **Open Sequence** when recoverable; otherwise **Open Folder**. | A direct device or file reason. | Inspect preserved output, correct the cause, or start a new recording. |
| **Failed** | `Error`; technical details are logged. | **Start Recording** enabled after the cause clears. | The direct camera or file failure. | Open the folder if partial files exist, correct the cause, and retry. |

### 4.4 Data > Capture > Droplet Dataset Capture

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Unavailable** | Preview region and Dataset setup fields; fixed processing is described as application configuration, not editable controls. | **Start Droplet Dataset Capture** disabled. | **Camera unavailable**, **Processing configuration unavailable**, **Output folder is not writable**, or **Another operation is active**. | Restore the camera/configuration, choose a writable folder, or stop the conflicting operation. |
| **Ready** | Streaming preview; Dataset metadata; optional Duration; Save Location. | **Start Droplet Dataset Capture** enabled. | — | Start capture. |
| **Starting** | Dataset folder, sequence writer, crop writer, and metadata recovery state are initialized; the Droplet Dataset Capture section remains expanded and the other Capture headings are disabled. | **Stop** enabled once accepted. | Start is disabled because **Droplet Dataset Capture is starting**. | Continue into Running or stop. |
| **Running** | Preview, elapsed time, full-frame count, detected-droplet count, and saved Droplet Crop count; the other Capture headings remain visible but disabled. | **Pause** enabled. | Other Capture actions are disabled because **Droplet Dataset Capture is active**. | Pause, Stop, or allow Duration to expire. |
| **Paused** | Preview continues; frame writing, detection, and Droplet Crop creation stop; the same Dataset remains open. | **Resume** enabled. | Start is disabled because **Droplet Dataset Capture is paused**. | Resume or Stop. |
| **Stopping** | New acquisition and detection stop while writers flush and `dataset.json` is finalized. | No new primary action. | **Dataset is being finalized**. | Continue to Completed, Interrupted, or Failed presentation. |
| **Completed** | Final counts, saved location, and completed Dataset actions. | **Open in Label** enabled. | — | Open the Dataset in Label. |
| **Interrupted** | `Error`; technical details are logged. | **Open Dataset** when recoverable; otherwise **Open Folder**. | The direct camera, processing, or write failure. | Inspect preserved data, fix the cause, or start a new capture. |
| **Failed** | `Error`; technical details are logged. | **Start Droplet Dataset Capture** enabled after recovery. | The current direct technical failure. | Open preserved files when available, correct the cause, and retry. |

### 4.5 Data > Label

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | An empty Dataset selection state with no crop browser content. | **Open Dataset** enabled; class-assignment actions disabled. | **No dataset selected**. | Select a v2 `dataset.json` or use **Open in Label** from Droplet Dataset Capture. |
| **Unavailable** | Dataset identity or attempted file path plus one direct incompatibility or lock message. | Class-assignment actions disabled. | **Dataset is in use by Training**, **Dataset is in use by Model Test**, **Required Droplet Crop is missing**, or **Unsupported OpenDSS v2 schema**. | End the owning operation, restore the missing file, or select a supported Dataset. |
| **Ready** | Dominant crop grid and the ordered Load Dataset, Dataset Summary, Label, and Filter panel content; configured Datasets retain the switchable two-or-three-class selection. | **Assign Class** or **Exclude** enabled for a valid selection; Class 0/1/2 remain visible and Class 2 is disabled for a two-class Dataset. | For no selected crop: **No Droplet Crop selected**. | Switch the class schema with explicit resolution of incompatible existing labels; label, relabel, Exclude, Undo, navigate Previous/Next, filter, Save As, or open the current Dataset in Train. |
| **Failed** | `Error`; technical details are logged. | **Retry Save** or **Open Dataset**, depending on the failure. | The direct parse, missing-file, or write reason. | Correct the file or permissions and retry; do not treat unsaved changes as persisted. |

### 4.6 Data > Sequence Viewer

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | Empty viewer without a frame. | **Open Sequence** enabled. | **No sequence selected**. | Select a standalone, Dataset, or Run sequence. |
| **Ready** | Current frame, frame number, total frames, direct seek, zoom, pan, Fit, and 1:1. | **Next Frame** enabled when a later readable frame exists. | — | Navigate, seek, or inspect the frame. |
| **Failed** | `Error`; technical details are logged. | **Open Sequence** enabled. | **Sequence unavailable**. | Restore the source or select another valid sequence. |

### 4.7 Models > Train

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | Dataset selector with no Dataset summary. | **Start Training** disabled; Dataset selection remains available. | **No dataset selected**. | Select a labeled v2 Dataset. |
| **Unavailable** | Dataset summary plus the first direct technical blocker. | **Start Training** disabled. | **No Labeled Droplet Crops**, **Required Droplet Crop is missing**, **Training environment unavailable**, **Output folder is not writable**, or **Another operation is active**. | Correct the stated prerequisite. No class-balance or model-quality blocker is shown. |
| **Ready** | Dataset Summary in a main white region; Architecture, compatible Weights, Compute Device with GPU selected by default and CPU selectable, requested/effective device status, Model Name, and Save Location; a separate main white Results region below. | **Start Training** enabled only when all pre-start inputs are valid. | A requested GPU that is unavailable falls back to effective CPU with a direct explanation; requested CPU remains CPU. | Start Training. |
| **Starting** | Training environment initialization and configuration snapshot; selection controls lock. | **Stop Training** enabled once accepted. | Start is disabled because **Training is starting**. | Continue into Running or stop. |
| **Running** | The Results region shows elapsed time, epoch/overall progress, estimated remaining time, requested and effective device, Training/Validation Loss plot, and Validation Accuracy plot. | **Stop Training** enabled. | Another Start is disabled because **Training is active**. | Continue or stop. |
| **Stopping** | Trainer termination and temporary-output finalization; no normal model-save claim. | No new primary action. | **Training is stopping**. | Continue to Interrupted or Failed unless a technically complete result exists. |
| **Saving** | Completed temporary artifacts while the package saves automatically to the preselected location. | No duplicate Start action. | **Model is saving**. | Continue to Completed or Save Failed. |
| **Completed** | The separate Results region shows the approved overall-results and per-class tables, optional confusion matrix, saved path, and Active Model confirmation. | **Open in Model Test** enabled. | — | Test the Active Model or open Library. |
| **Save Failed** | `Error`; technical details are logged. | **Retry Save** enabled. | The final package is not saved. | Correct storage access and retry the same save. |
| **Interrupted** | `Error`; technical details are logged. | **Start Training** enabled after resources release. | — | Correct the cause or change inputs, then start again. No normal package is created unless required artifacts were technically completed and explicitly saved. |
| **Failed** | `Error`; technical details are logged. | **Retry Training** enabled after the cause clears. | The direct technical failure. | Open diagnostics when useful, correct the cause, and retry. |

### 4.8 Models > Model Test

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | Read-only Active Model context and Dataset selector without result content. | **Start Model Test** disabled. | **No Active Model** or **No dataset selected**. | Set Active in Library or select a Dataset. |
| **Unavailable** | Loaded artifact summaries and the first direct incompatibility. | **Start Model Test** disabled. | For example: **The selected model has 2 output classes, but the selected Dataset defines 3 classes**; **No Labeled Droplet Crops**; **Output folder is not writable**; or **Another operation is active**. | Select compatible/readable artifacts, choose a writable location, or stop the conflicting operation. GPU unavailability is not a blocker because CPU fallback is automatic. |
| **Ready** | Compatible Dataset Summary in a main white region, factual planned device (GPU when available or CPU otherwise), and a separate main white Results region below. | **Start Model Test** enabled. | — | Start Model Test. |
| **Starting** | Artifact loading and test-output initialization; selectors lock. | **Stop Model Test** enabled once accepted. | Start is disabled because **Model Test is starting**. | Continue into Running or stop. |
| **Running** | Progress and factual execution-device status. | **Stop Model Test** enabled. | Another Start is disabled because **Model Test is active**. | Continue or stop. |
| **Stopping** | New inference stops and output is finalized as far as possible. | No new primary action. | **Model Test is stopping**. | Continue to Interrupted, Failed, or Completed if all required output finalized. |
| **Completed** | The Results region shows Overall Accuracy, Per-Class Accuracy, Confusion Matrix, prediction summaries, output location, and device used. | **Open Predictions CSV** enabled. | — | Review/export results or run another test. |
| **Interrupted** | `Error`; technical details are logged. | **Start Model Test** enabled after resources release. | — | Correct the cause or choose different artifacts and start again. |
| **Failed** | `Error`; technical details are logged. | **Retry Model Test** enabled after the cause clears. | The direct technical failure. | Correct the cause and retry. |

### 4.9 Models > Library

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | No discovered v2 Model Packages and no selected-model details. | **Import Model** enabled; Set Active disabled. | **No Active Model**. | Import a valid v2 Model Package or train a model. |
| **Ready** | Model list, selected-model provenance/metrics, package location, and Active state. | **Set Active** enabled for a valid nonactive selection when the current Active Model is not operation-locked. | **Selected model is already Active** when applicable. Set Active or package mutation actions that would replace, delete, or alter a model in use are disabled with **Model is in use by [operation]**. | Set Active, manage the package, or open it in Model Test. |
| **Failed** | `Error`; technical details are logged. | The relevant **Retry** or **Import Model** action remains available when safe. | The direct package or file-system reason. | Select a supported v2 package, correct permissions, or retry the requested operation. |

### 4.10 Sort > Live — pre-run

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Unavailable** | Live preview region, run configuration, Trigger Every Droplet, routing selections, profile actions, and hardware status. With no camera, the preview reads **Camera unavailable**. | **Start Sorting** disabled. | **Camera unavailable**, **DAQ unavailable**, **No active model** for Class-Based Sorting, **No Hit Class selected**, **No Hit boundary calibration selected**, **Output folder is not writable**, or **Another operation is active**. | Correct the one stated prerequisite. Trigger Every Droplet remains available without a model. |
| **Ready** | Streaming preview; complete run selections; hardware panel available; Start is technically ready. | **Start Sorting** enabled. | — | Start Sorting. |

### 4.11 Sort > Live — running

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Starting** | Run folder and initial structured files are created, the initial effective configuration is snapshotted, and the docked Hardware panel remains available under arbitration rules. | **Stop** enabled once accepted. | Start is disabled because **Sorting is starting**. | Continue into Running or stop. |
| **Running** | Live preview; Run status; elapsed time; editable Active Model, Hit Class, Trigger Every Droplet, DAQ Output, and hit-boundary fields; current effective-configuration identity; factual counters and timing. | **Pause** enabled. | An invalid committed change is rejected with its direct factual reason; the last valid effective configuration remains active. | Adjust an approved field, Pause, Stop, allow Duration to expire, or transition on fault. |
| **Stopping** | New inference and DAQ output have stopped; persistence queues flush; Run Summary and Droplet Log finalize. | No new primary action. | **Run is stopping**. | Continue to Completed, Interrupted, or Failed presentation. |

### 4.12 Sort > Live — paused

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Paused** | Live camera preview remains active; inference, event-triggered finite sine waves, and new event finalization are stopped; counters remain stable; the approved editable configuration and docked Hardware panel remain available. | **Resume** enabled. | Invalid changes retain the last valid effective configuration and show a direct reason. | Adjust an approved field, Resume the same Run, or Stop it. |

### 4.13 Sort > Live — completed/interrupted

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Completed** | Final Run status, stop reason, summary values, and saved location. A clean user Stop may appear in this presentation with its persisted stop reason/status. | **Open Run Summary** enabled. | — | Open Results or Start New Run. |
| **Interrupted** | `Error`; technical details are logged. | **Open Run Summary** when recoverable; otherwise **Open Run Folder**. | — | Inspect preserved output, correct the cause, or Start New Run. |
| **Failed** | `Error`; technical details are logged. | **Open Run Folder** when one exists. | — | Correct the technical cause, then Start New Run. |

### 4.14 Sort > Sequence Test

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | Sequence, Model, Trigger Every Droplet, Hit Class, Hit boundary calibration, Physical DAQ Output, and output-location controls without a selected sequence. | **Start Sequence Test** disabled; Sequence selection enabled. | **No sequence selected**. | Select a v2 Image Sequence. |
| **Unavailable** | Selected artifact summaries plus the first direct blocker. | **Start Sequence Test** disabled. | **No Active Model** for Class-Based Sorting, **DAQ unavailable** while Physical DAQ Output is enabled, **No Hit boundary calibration selected**, **Output folder is not writable**, **Unsupported OpenDSS v2 sequence**, or **Another operation is active**. | Select/correct the required artifact, disable physical DAQ output when appropriate, restore DAQ, or stop the conflicting operation. |
| **Ready** | Valid sequence; mode-dependent model state; explicit physical-output state; routing selections. | **Start Sequence Test** enabled. | — | Start Sequence Test. |
| **Starting** | Run output initialization and sequence-loading status; inputs lock. | **Stop Sequence Test** enabled once accepted. | Start is disabled because **Sequence Test is starting**. | Continue into Running or stop. |
| **Running** | Processing progress, event/counter status, routing context, and physical-output state. | **Stop Sequence Test** enabled. | Another Start is disabled because **Sequence Test is active**. | Continue or stop. |
| **Stopping** | New processing and DAQ output stop; event and Run files finalize. | No new primary action. | **Sequence Test is stopping**. | Continue to Completed, Interrupted, or Failed presentation. |
| **Completed** | Final counts, status, and direct Run actions. | **Open Run Summary** enabled. | — | Open Results. |
| **Interrupted** | `Error`; technical details are logged. | **Open Run Summary** when recoverable; otherwise **Open Run Folder**. | — | Inspect output, correct the cause, or start another test. |
| **Failed** | `Error`; technical details are logged. | **Retry Sequence Test** enabled after the cause clears. | The direct technical failure. | Correct the cause and retry; open any preserved Run folder. |

### 4.15 Results > Runs — list

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | The Runs list states that no Live Sorting or Sequence Test Runs were found. | No Run-open action is enabled. | **No Runs found**. | Create a Run in Live or Sequence Test. |
| **Ready** | Discoverable Runs and their factual list columns. | **Load selected Run** enabled after selection. | Before selection: **No Run selected**. | Select and open a Run. |
| **Failed** | `Error`; technical details are logged. | Run opening is disabled for unreadable entries. | The direct file or storage reason. | Correct the data root, restore permissions/files, and reopen Runs. |

### 4.16 Results > Runs — selected Run

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Ready** | Run Summary, provenance, factual counts, Decision-versus-Observed Route matrix, Notes, and available file links. | **Open Droplet Log** enabled when `events.csv` is available. | For a missing optional sequence: **No saved Image Sequence for this Run**. | Review files, edit Notes, or open the saved sequence when present. |
| **Unavailable** | Run identity remains visible, but one required selected artifact cannot be read. | The affected file action is disabled; unaffected direct file actions remain available. | **Droplet Log unavailable**, **Run Summary cannot be read**, or another direct missing-file reason. | Open the Run folder, restore the file, or select another Run. |
| **Failed** | `Error`; technical details are logged. | **Retry Save Notes** or the unaffected file action. | The direct permission or file-system reason. | Correct permissions/path and retry; historical event data is not rewritten. |

### 4.17 Settings

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Ready** | Storage, Application Information, Diagnostics, Visuals, version information, runtime/driver availability, current data root, and one Text Size dropdown with exactly Small (80%), Medium (100%), and Large (125%). Medium is the default; 200% is validation-only and not selectable. At Medium, body text, standard control text, and button text use 16 px. Body and standard controls retain approximately 20 px line height; buttons retain approximately 18–20 px line height. Ordinary field and settings labels use 15 px with approximately 18 px line height. Captions, status, warning, and metadata use 13 px with approximately 16–18 px line height. No other setting is present. | **Choose Default Data Root** enabled. | — | Select a writable location, open the current data or diagnostic folder, or set application-wide Text Size. |
| **Failed** | `Error`; technical details are logged. | **Choose another location** or retry the direct folder action. | The direct permission or path reason. | Select a valid writable location or restore access. |

---

## 5. Contextual workflow links

Contextual links carry an artifact selection into another persistent workspace. They do not lock the user into a wizard, create a project, or prevent direct navigation.

| From | Contextual link | Preselected artifact | Destination behavior |
|---|---|---|---|
| **Droplet Dataset Capture** | **Open in Label** | The completed or recoverable v2 `dataset.json`. | Opens Data > Label with that Dataset selected. |
| **Label** | **Use in Train** | The currently open Dataset. | Opens Models > Train with the Dataset selected; Training still applies its own technical prerequisites. |
| **Train** | **Open in Model Test** | The newly saved Model Package; the current source Dataset may remain selected when still available. | Opens Models > Model Test without making Model Test mandatory. |
| **Model Library** | **Open in Model Test** | The selected Model Package. | Opens Models > Model Test with the model selected. |
| **Image Sequence** | **Open in Sequence Viewer** | The completed `sequence.json`. | Opens Data > Sequence Viewer at the sequence. |
| **Image Sequence** | **Open in Sequence Test** | The completed `sequence.json`. | Opens Sort > Sequence Test with the sequence selected. |
| **Live or Sequence Test Run** | **Open Run Summary** | The newly finalized or recoverable Run. | Opens Results > Runs with that Run selected. |
| **Results** | **Open Saved Sequence** | The Run's full Image Sequence, only when it exists. | Opens Data > Sequence Viewer with that sequence selected. |

The destination remains independently navigable, and the user may replace the preselected artifact before starting an operation.

---

## 6. Shared state and lock behavior

### 6.1 Ownership notation

- **X** — exclusive ownership while the operation is active.
- **W** — write ownership.
- **R-lock** — read/use lock that prevents conflicting replacement, deletion, or mutation.
- **Notes W** — Notes-only write access to an inactive Run; event data remains immutable.
- **—** — no ownership.
- **Global slot: Yes** — occupies the one long-running-operation slot.

### 6.2 Resource ownership matrix

| Operation | Camera | DAQ | Dataset write access or lock | Model Package write access or lock | Run output | Global long-running slot | Hardware panel and conflicting actions |
|---|---:|---:|---|---|---|---:|---|
| **Single Image** | X, momentary | — | — | — | — | No; momentary reservation | Camera controls are briefly unavailable during capture. Capture Image is disabled when another operation owns Camera or the storage pipeline. |
| **Image Sequence recording** | X | — | — | — | — | Yes | Camera section locks; DAQ section remains independently available if idle. All other long-running Start actions show **Another operation is active**. |
| **Droplet Dataset Capture** | X | — | W on the new Dataset | — | — | Yes | Camera section locks; DAQ remains independently available if idle. Label cannot modify the Dataset until capture finalizes. |
| **Label** | — | — | W on the selected Dataset | — | — | No | Panel is unaffected. Label writes are disabled while Training or Model Test holds an R-lock on the same Dataset. |
| **Training** | — | — | R-lock on the selected Dataset | W on the new Model Package during final save | — | Yes | Panel remains available because Training owns no hardware. Other long-running Start actions are disabled; Label cannot modify the selected Dataset. |
| **Model Test** | — | — | R-lock on the selected Dataset | R-lock on the selected Model Package | —; Model Test output is not a Run | Yes | Panel remains available. Label and conflicting model-package mutation are disabled for the selected artifacts. |
| **Model Library package mutation** | — | — | — | W on the selected package/registry for the mutation | — | No | Panel is unaffected. Delete, replace, rename, or duplicate actions are disabled when the package is in use by an operation. |
| **Sequence Viewer** | — | — | Read only when opening a Dataset sequence | — | Read only when opening a Run sequence | No | Panel is unaffected. Frame navigation does not own the global slot. |
| **Live Sorting** | X | X | — | R-lock on the currently effective Model Package when present; a valid Active Model change transfers effective use under ModelRegistry/OperationCoordinator | W | Yes | The docked panel remains available. Camera acquisition settings lock; approved DAQ actions follow the continuous/event-output arbitration rules. Conflicting starts and model-package mutations are disabled. |
| **Sequence Test — physical DAQ output enabled** | — | X | — | R-lock on the selected Model Package when present | W | Yes | The docked panel remains available. Continuous configured waveform may start and takes priority over event-triggered finite sine waves; Camera remains independently available if idle. Other long-running starts are disabled. |
| **Sequence Test — physical DAQ output disabled** | — | — | — | R-lock on the selected Model Package when present | W | Yes | Panel remains available because no hardware is owned. Other long-running starts are still disabled by the global slot. |
| **Results — edit Run Notes** | — | — | — | — | Notes W on a nonactive Run | No | Panel is unaffected. Only Notes change; `events.csv`, Droplet Crops, and historical event values remain unchanged. |

### 6.3 Lock effects

1. **Global long-running-operation slot**  
   Image Sequence recording, Droplet Dataset Capture, Training, Model Test, Sequence Test, and Live Sorting are mutually exclusive. A conflicting Start action is disabled with **Another operation is active**.

2. **Camera ownership**  
   Image Sequence, Droplet Dataset Capture, and Live Sorting lock Camera settings. Single Image reserves the Camera momentarily. Camera-independent workspaces remain usable.

3. **DAQ ownership**  
   Live Sorting always owns the DAQ. Sequence Test owns it only when Physical DAQ Output is enabled. Send Test Sine Wave requires DAQ Ready and creates no Run or event.

4. **Dataset ownership**  
   Label writes the selected Dataset. Training and Model Test hold an R-lock on the selected Dataset so Label cannot change it during either operation.

5. **Model Package ownership**  
   Training writes a new package only after technical completion and explicit save. Model Test, Live, and Sequence Test protect any selected package from conflicting mutation while in use. When an operation uses the current Active Model, Set Active cannot replace that selection until the operation ends.

6. **Run ownership**  
   Live and Sequence Test exclusively write their Run output until finalization. Results reads finalized or recoverable Runs and may update Notes only after the Run is no longer active.

7. **Configuration snapshot and Live change history**
   At operation start, OpenDSS retains the initial effective configuration needed for provenance. Later navigation or idle-setting changes do not alter an active capture. During an active Live Run, valid committed changes to Active Model, Hit Class, Trigger Every Droplet, DAQ Output, and hit-boundary fields apply immediately, append a timestamped effective-configuration entry, and are referenced by every affected event.

---

## 7. Requirements-to-workspace trace

### 7.1 Detailed workflow specification trace

| Detailed workflow section | OpenDSS v2 implementation location | Consolidated coverage and controlling amendment |
|---|---|---|
| **WF §§7–9 — Primary navigation, global shell, operation/concurrency rules** | Application shell; all workspaces | Preserves direct persistent workspaces, global statuses, disabled reasons, one active long-running operation, and Stop. PM §§4–5 and §§13–14 replace the old navigation placement where necessary, add the shared hardware panel, define startup, and simplify fault communication. |
| **WF §11 — Capture a Single Image** | Data > Capture > Single Image | Preserves live preview, optional timestamp name, writable Save Location, one TIFF per action, no model/DAQ/Dataset/Run. |
| **WF §12 — Capture an Image Sequence** | Data > Capture > Image Sequence | Preserves optional Duration, Pause/Resume/Stop, numbered TIFF frames, `sequence.json`, and direct handoff to Sequence Viewer. PM §7.1 places it as one of three equal modes in the shared Capture workspace. |
| **WF §13 — Capture a Dataset** | Data > Capture > Droplet Dataset Capture | Preserves model-independent Droplet Dataset Capture, full sequence, one Droplet Crop per detection, counters, interruption recovery, and `dataset.json`. PM §§7.1 and 11 make detection/crop configuration fixed rather than user-editable. |
| **WF §14 — Label a Dataset** | Data > Label | Preserves switchable two-class or three-class setup, stable Class IDs, explicit handling of labels incompatible with the selected schema, editable Class Names, always-visible Class 0/1/2 actions, Exclude, Undo, Previous/Next, class/Excluded/Unreviewed filters, factual counts, current-Dataset persistence, and independent-copy Save As. |
| **WF §15 — Train a Model** | Models > Train | Preserves Dataset selection, fixed 70/15/15 split, seed 1729, metrics, Stop, atomic package saving, and automatic activation. Library owns the model's unique Name, supported Architecture, and approved Starting Weights; Train selects that existing Library model read-only and never mutates it, including during retraining. Compute Device is a normal selection with GPU requested by default and CPU selectable; requested/effective device are recorded, requested GPU falls back to effective CPU when unavailable, and requested CPU is never promoted. No technical hyperparameters are exposed. |
| **WF §16 — Model Test** | Models > Model Test | Preserves optional observational testing, same-Dataset permission, class-count blocking, metrics, confusion matrix, and predictions CSV. PM §7.5 adds automatic optional GPU acceleration with CPU fallback. |
| **WF §17 — Model Library** | Models > Library | Preserves Set Active, Export, Duplicate, Rename, package metadata, and technical package checks. Library owns Add Model and complete-package Import Model. Remove Model requires confirmation and moves the OpenDSS-owned complete package folder to the Recycle Bin subject to active, in-use, registry, and package-integrity locks. PM §§7.6 and 16 prohibit application conversion and raw-weight or bare-ONNX import; the repository converter remains a standalone development utility. |
| **WF §18 — Sequence Viewer** | Data > Sequence Viewer | Preserves opening standalone, Dataset, or Run sequences; previous/next, direct seek, zoom, pan, Fit, and 1:1; no hardware or DAQ output. |
| **WF §19 — Sequence Test** | Sort > Sequence Test | Preserves recorded-sequence processing, both Trigger Every Droplets, optional physical DAQ output, Run creation, and no camera requirement. PM D-005 and D-006 add Observed Route = Unresolved and move the workspace from Models to Sort. |
| **WF §§20–21 — Configure Sorting and Live Sorting** | Sort > Live pre-run, running, paused, and completed/interrupted | All user-facing setup moves into Live pre-run. Live retains camera view, Trigger Every Droplet, Hit Class, Hit boundary calibration, optional full-sequence recording, Send Test Sine Wave, Pause, Stop, counters, persistence, and fault handling. PM §§7.7, 9, and 11 move Camera/DAQ controls to the bottom Hardware panel, make Setup Profiles ordinary files, and hide fixed processing controls. |
| **WF §22 — Review Runs** | Results > Runs list and selected Run | Preserves Live Sorting and Sequence Test Run discovery, Run Summary, Droplet Log, Droplet Crops, Notes, direct file access, and Decision-versus-Observed Route. PM §§7.9 and 10 add Unresolved and retain Results for Runs only. |
| **WF §23 — Settings** | Docked Hardware panel plus reduced Settings | Camera and DAQ technical settings remain in the shell panel. Settings retains only Storage, Application Information, Diagnostics, and Visuals; Text Size offers exactly Small (80%), Medium (100%, default), and Large (125%), with 200% validation-only. At Medium, body text, standard control text, and button text use 16 px. Body and standard controls retain approximately 20 px line height; buttons retain approximately 18–20 px line height. Ordinary field and settings labels use 15 px with approximately 18 px line height. Captions, status, warning, and metadata use 13 px with approximately 16–18 px line height. Detection, crop, sorting-algorithm, internal timing, and training-hyperparameter controls are not exposed. |
| **WF §§24–30 — Artifact and file contracts** | Capture, Label, Train, Library, Sequence Viewer, Live, Sequence Test, and Results | Preserves canonical v2 files: `sequence.json`, `dataset.json`, `metadata.json`, `run_summary.json`, and `events.csv`. PM §§8–10 update Setup Profiles to ordinary files and add Unresolved to events and matrices. |
| **WF §§31–34 — Persistence, recovery, and error handling** | All long-running workspaces; Results; contextual fault banner | Preserves background persistence, atomic structured-file writes, recoverable partial output, factual errors, and no false success. PM §14 constrains presentation to direct disabled reasons, one contextual banner, and direct recovery actions. |
| **WF §§35–36 — Installation, first launch, and camera-free use** | Application startup; workspace availability states | Preserves offline local operation and camera-free use of Label, Train, Model Test, Library, Sequence Viewer, Sequence Test when its DAQ condition is satisfied, Results, and Settings. PM §4 requires every launch to open Data > Capture with all three Capture sections collapsed. |
| **WF §§37–43 — Provenance, nonfunctional requirements, and exclusions** | All artifact-producing workspaces and application boundaries | Preserves reproducibility, responsiveness, data integrity, scientific transparency, offline behavior, and explicit exclusions. PM §§11, 15–17 resolve the v2-only, fixed-configuration, no-Home, no-managed-profile-library boundaries. |
| **WF §§44–72 — End-to-end acceptance scenarios** | Corresponding workspaces and state transitions in this inventory | The scenarios remain coverage targets after applying the approved navigation, Unresolved, Model Test GPU, profile, Settings, and fixed-control amendments. |
| **WF §§73–78 — Application-layer ownership and repository alignment** | Shared-state and lock matrix; architecture boundary | Preserves one authoritative owner per domain and treats repository components as implementation evidence only, never as navigation categories. |

### 7.2 Approved product-model decision trace

| Decision | Implemented by |
|---|---|
| **D-001 — Domain primary navigation** | Section 2 hierarchy: Data / Models / Sort / Results / Settings. |
| **D-002 — No separate software DAQ arming** | Live and Sequence Test use factual DAQ readiness; Send Test Sine Wave requires DAQ Ready. |
| **D-003 — Saved model automatically becomes Active** | Train Completed state and Library global Active Model behavior. |
| **D-004 — Model Test optional GPU with CPU fallback** | Models > Model Test inventory and Ready/Unavailable states; GPU absence never blocks Start. |
| **D-005 — Observed Route adds Unresolved** | Live monitor, Sequence Test output, Run Summary counts, and Decision-versus-Observed Route matrix. |
| **D-006 — Sequence Test under Sort** | Section 2 navigation and Sort > Sequence Test workspace. |
| **D-007 — One Live workspace** | Live pre-run, running, paused, and completed/interrupted state presentations; no Sort Setup navigation item. |
| **D-008 — Trigger Every Droplet remains first-class** | Live pre-run and Sequence Test controls and technical prerequisites. |
| **D-009 — Setup Profiles are ordinary files** | Live pre-run actions: Open Profile, Save Profile, Save Profile As; no managed profile library. |
| **D-010 — No product-level legacy support** | V2-only Dataset, Sequence, Model Package, Setup Profile, and Run loaders; direct unsupported-schema errors. |
| **D-011 — Model Test remains first-class** | Models > Model Test is a direct persistent workspace. |
| **D-012 — Two-class and three-class workflows** | Label, Train, Model Test, Live, Sequence Test, Library, and Results terminology and metadata. |
| **D-013 — Results contains Runs only** | Results > Runs lists Live Sorting and Sequence Test only. |
| **D-014 — Shared Capture composition** | One shared live Camera preview sits beside three independently collapsible operation sections; all headings remain visible and none is expanded by default. |
| **D-015 — No Advanced Training Parameters** | Train exposes Dataset, an existing Library model selected read-only for identity, supported Architecture, and approved Starting Weights, a GPU-default/CPU-selectable Compute Device with requested/effective status, Output Location, progress, metrics, Stop, and atomic new-package save. Technical hyperparameters remain qualified configuration and are not exposed. |
| **D-016 — Camera/DAQ settings only; immediate while available and ownership/arbitration-permitted** | Shared shell-level hardware panel and its idle/unavailable/locked/active behavior, including supported Amplitude/Frequency retune during active continuous output while Camera ownership restrictions remain in force. |
| **D-017 — Reduced Settings** | Storage, Application Information, Diagnostics, and Visuals only; Text Size offers exactly Small (80%), Medium (100%, default), and Large (125%), with 200% validation-only. At Medium, body text, standard control text, and button text use 16 px. Body and standard controls retain approximately 20 px line height; buttons retain approximately 18–20 px line height. Ordinary field and settings labels use 15 px with approximately 18 px line height. Captions, status, warning, and metadata use 13 px with approximately 16–18 px line height. |
| **D-018 — Fixed startup workspace** | Every launch opens Data > Capture with all three Capture sections collapsed; no last-workspace restore. |
| **D-019 — Simple contextual faults** | Direct disabled reasons, one persistent workspace banner, and direct recovery actions. |

---

## 8. Removed or hidden interface elements

| Removed or hidden element | OpenDSS v2 disposition |
|---|---|
| **Separate Sort Setup** | Removed as a workspace and navigation item. Its user-facing run configuration is the pre-run state of **Sort > Live**. |
| **Editable detector settings** | Hidden from the user. Droplet detection uses fixed qualified application configuration; the effective configuration or version is recorded in Dataset and Run provenance. |
| **Editable crop settings** | Hidden from the user. The Droplet Crop artifact contract remains fixed, including 64 × 64 grayscale PNG output where applicable; effective configuration is recorded. |
| **Editable routing-algorithm settings** | Hidden from the user. The qualified routing algorithm is not a tuning surface. User selections remain Trigger Every Droplet, Hit Class, and Hit boundary calibration. |
| **Editable internal timing settings** | Hidden from the user. Internal tracking and synchronization timing are fixed. Supported DAQ device/channel and sine-output settings remain editable only as DAQ technical settings in the bottom Hardware panel. |
| **Advanced Training Parameters** | Removed. Library owns model identity, supported Architecture, and approved Starting Weights. Train selects an existing Library model read-only and selects Compute Device as a normal execution choice, not a hyperparameter. The effective qualified hyperparameters, 70/15/15 split, and seed 1729 remain fixed and recorded. |
| **Managed Setup Profile library actions** | Removed. Setup Profiles are ordinary v2 files with **Open Profile**, **Save Profile**, and **Save Profile As**. File copy, rename, move, and deletion occur through normal Windows file ownership. |
| **Home screen** | Removed. Every launch opens **Data > Capture** with all three Capture sections collapsed. |
| **Training and Model Test history under Results** | Removed. Results contains only Live Sorting and Sequence Test Runs. Model Test writes its own summary and predictions CSV without becoming a Run. |
| **Legacy migration UI** | Removed. Unsupported artifacts receive a direct unsupported-v2-schema message; conversion is an internal engineering activity, not a product workflow. |

### 8.1 Terminology and boundary checks

- **Predicted Class**, **Decision**, and **Observed Route** remain separate in every Live, Sequence Test, Droplet Log, and Results presentation.
- **Observed Route** uses exactly **Hit**, **Waste**, or **Unresolved**.
- Model output values are **Class Scores**, not confidence controls.
- The interface adds no scientific quality gates, class-balance warnings, model approval states, confidence thresholds, projects, accounts, cloud features, placeholder controls, or repository-component navigation categories.

---

## Source citations

The section labels used in the trace tables refer to these controlling source sections.

### Approved v2 Product Model

- **PM §1 — Purpose and authority:** the approved model controls D-001 through D-019, the detailed workflow supplies nonconflicting requirements, and the repository is implementation evidence only. fileciteturn0file1L12-L24
- **PM §4 — Approved navigation and startup:** final hierarchy, no Home, startup at Data > Capture with all three Capture sections collapsed, no last-workspace restore, and contextual rather than mandatory workflow links. fileciteturn0file1L114-L147
- **PM §5 — Global shell and hardware panel:** header, shell-owned Camera/DAQ panel, immediate valid changes, locking, and Live panel closure. fileciteturn0file1L151-L201
- **PM §§7.1–7.3 — Capture, Label, and Sequence Viewer:** shared Capture preview and collapsible sections, fixed capture processing, Label actions, and hardware-free Sequence Viewer. fileciteturn0file1L282-L348
- **PM §§7.4–7.6 — Train, Model Test, and Library:** no Advanced Training Parameters, user-selected Train Compute Device with requested/effective status, automatic Model Test GPU fallback, and v2 Model Library behavior. fileciteturn0file1L350-L426
- **PM §§7.7–7.8 — Live and Sequence Test:** one Live workspace, pre-run-to-monitor transition, locks, counters including Unresolved, and Sequence Test under Sort. fileciteturn0file1L428-L540
- **PM §§7.9–7.10 — Results and Settings:** Runs-only Results and reduced Settings. fileciteturn0file1L542-L586
- **PM §§8–9 — Artifact and Setup Profile models:** v2 artifact relationships and ordinary-file Profile actions Open, Save, and Save As. fileciteturn0file1L590-L687
- **PM §§10–11 — Scientific event model and editable/fixed configuration:** Predicted Class/Decision/Observed Route separation, Unresolved, Camera/DAQ-only technical editing, and fixed processing/training configuration. fileciteturn0file1L699-L785
- **PM §§12–14 — Dependencies, operation lifecycle, faults, and recovery:** hardware/model prerequisites, one long-running slot, Stop, configuration snapshots, direct disabled reasons, one banner, and direct recovery. fileciteturn0file1L789-L895
- **PM §§16–19 — Legacy boundary, first-release boundaries, state ownership, and decision register:** v2-only product, excluded UI, authoritative owners, and D-001 through D-019. fileciteturn0file1L921-L1024
- **PM §20 — Required amendments:** explicit list of navigation, Unresolved, GPU, Capture, panel, fixed-control, Profile, legacy, Results, Settings, startup, and fault amendments. fileciteturn0file1L1028-L1046

### Detailed User Workflow Specification

- **WF §§7–9 — Navigation, shell, and operation rules:** original persistent workspace, status, disabled-reason, hardware-unavailable, concurrency, and lifecycle requirements retained where not superseded. fileciteturn0file0L288-L324 fileciteturn0file0L328-L408 fileciteturn0file0L412-L459
- **WF §§11–14 — Data workflows:** Single Image, Image Sequence, Droplet Dataset Capture, and Label requirements and artifacts. fileciteturn0file0L510-L567 fileciteturn0file0L571-L695 fileciteturn0file0L699-L878 fileciteturn0file0L882-L1028
- **WF §§15–17 — Model workflows:** Training, Model Test, and Model Library requirements retained subject to the approved v2 amendments. fileciteturn0file0L1034-L1260 fileciteturn0file0L1264-L1380 fileciteturn0file0L1384-L1469
- **WF §§18–19 — Sequence review and testing:** Sequence Viewer and Sequence Test processing, Trigger Every Droplets, optional DAQ output, and Run creation. fileciteturn0file0L1473-L1657
- **WF §§20–21 — Sorting configuration and Live Sorting:** run metadata, Trigger Every Droplet, Hit Class, Hit boundary calibration, test sine wave, optional full sequence, Live event flow, Pause, Stop, persistence, counters, and faults, consolidated into one Live workspace. fileciteturn0file0L1661-L1882 fileciteturn0file0L1886-L2091
- **WF §22 — Results:** Run list, Run Summary, Decision-versus-Observed Route, Notes, and direct file actions. fileciteturn0file0L2095-L2259
- **WF §23 — Settings:** original setting groups and offline hardware behavior, narrowed by the approved v2 model. fileciteturn0file0L2263-L2345
- **WF §§24–30 — File contracts:** canonical artifact names and Dataset, Sequence, Model, Run, Droplet Log, and Setup Profile contracts, as amended for v2. fileciteturn0file0L2349-L2680
- **WF §§31–34 — Persistence, recovery, and errors:** background writes, flush points, atomic JSON, crash recovery, plain-language errors, and data preservation. fileciteturn0file0L2684-L2829
- **WF §§35–43 — Launch, camera-free use, provenance, nonfunctional requirements, and exclusions:** offline first launch, hardware-free workspaces, reproducibility, responsiveness, data integrity, and prohibited first-release additions. fileciteturn0file0L2833-L3032
- **WF §§44–72 — End-to-end acceptance scenarios:** workflow-level acceptance coverage for installation, capture, labeling, training, testing, sorting, results, Sequence Viewer/Test, faults, mutual exclusion, and offline behavior. fileciteturn0file0L3036-L3266
- **WF §§73–78 — Workflow ownership, repository alignment, and engineering discretion:** authoritative service boundaries, contract tests, repository-as-evidence rule, and implementation details that do not alter product structure. fileciteturn0file0L3269-L3419
