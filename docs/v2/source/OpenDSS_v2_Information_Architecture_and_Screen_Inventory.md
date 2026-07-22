# OpenDSS v2 Information Architecture and Screen Inventory

**File:** `OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md`  
**Status:** Consolidated OpenDSS v2 design baseline  
**Authority:** The Approved v2 Product Model controls D-001 through D-019. The Detailed User Workflow Specification supplies requirements that do not conflict with those decisions. The existing repository is not an authority for navigation or UX.

This document uses the simplest interpretation consistent with the approved product model. It defines information architecture, workspaces, states, contextual handoffs, and ownership/lock behavior. It does not add a Home screen, a wizard, scientific quality gates, or unapproved controls.

---

## 1. Application shell

### 1.1 Shell composition

The OpenDSS v2 shell has five persistent structural elements:

1. **Global status header** — a compact, always-visible projection of authoritative application state.
2. **Primary navigation** — direct domain navigation for Data, Models, Sort, Results, and Settings.
3. **Workspace region** — the main content area for the selected workspace or Capture mode.
4. **Operation-side panel** — a workspace-owned right panel for current inputs, status, counters, and actions.
5. **Shared Camera/DAQ drawer** — a shell-owned slide-out drawer containing the only user-editable technical settings.

A single contextual fault banner appears inside the affected workspace when an operation is interrupted or fails. OpenDSS does not use a notification center or repeated modal fault sequence.

### 1.2 Global status header

The header displays these four status areas:

| Status area | Values or presentation |
|---|---|
| **Camera** | `Unavailable`, `Connected`, or `Streaming` |
| **DAQ** | `Unavailable`, `Ready`, or `Active` |
| **Active Model** | `No Active Model` or the current Model Name |
| **Current Activity** | `Idle`, `Capturing Image`, `Recording Sequence`, `Capturing Dataset`, `Labeling`, `Training`, `Testing Model`, `Playing Sequence`, `Testing Sequence`, `Sorting`, or `Paused` |

The header remains visible during every workspace and while the hardware drawer is closed. It is a projection of authoritative domain state, not a second editable source of truth.

### 1.3 Primary navigation

Primary navigation is persistent and directly opens the selected domain workspace. It does not encode a mandatory workflow sequence and does not contain a Home item.

### 1.4 Workspace region

The workspace region contains the primary scientific or operational content:

- a live camera preview in Capture and Live;
- a crop browser in Label;
- a sequence viewer in Sequence Player;
- selectors, progress, and metrics in Train and Model Test;
- model list and metadata in Library;
- Run list and Run Summary in Results;
- reduced storage, application information, and diagnostics content in Settings.

When a camera-dependent workspace is unavailable, its preview region remains in place and displays **Camera unavailable** rather than changing to a named alternate mode.

### 1.5 Operation-side panel

The operation-side panel is part of the current workspace, not part of primary navigation.

- In **Capture**, it changes with the selected mode and then changes from setup fields to operation status, counters, and Pause/Resume/Stop while a sequence or Dataset is being captured.
- In **Live**, it contains run configuration before Start, then is replaced by the live sorting monitor when sorting begins.
- In **Train**, **Model Test**, and **Sequence Test**, it contains artifact selection, start/stop actions, progress, and completion actions.
- In **Label**, **Library**, **Results**, and **Settings**, it contains the selected-item details and actions appropriate to that workspace.

The panel must not duplicate Camera or DAQ technical controls.

### 1.6 Shared Camera/DAQ drawer

The shell-level drawer contains:

```text
Hardware
├── Camera
└── DAQ
```

It is the only location for user-editable Camera and DAQ technical settings. Values are shared across workspaces; individual workspaces and Settings do not maintain duplicate copies. **DAQ Output Channel** is a DAQ setting in this drawer and remains distinct from **Hit Outlet Direction**.

Valid changes apply immediately when the corresponding device is available and not owned by an operation. Invalid changes are rejected, and the last successfully applied value remains active. There is no separate Apply workflow and no separate software arming state.

The following are not drawer controls:

- droplet-detection parameters;
- Droplet Crop parameters beyond the fixed artifact contract;
- routing-algorithm parameters;
- internal tracking or synchronization timing;
- training hyperparameters.

Those values are fixed qualified application configuration and are recorded in provenance where required.

### 1.7 Drawer interaction conditions

These are drawer interaction conditions, not additional Camera or DAQ status values.

| Condition | Drawer behavior | Control behavior | User-facing explanation |
|---|---|---|---|
| **Idle** | Drawer may be opened or closed. Its ordinary open/closed presentation may persist during navigation. | The available device section is editable. A valid change applies immediately. | No message is required. |
| **Unavailable** | Drawer can open so the unavailable device and its status remain visible. | The unavailable device section is disabled. The other device section remains independently usable if available and idle. | **Camera unavailable** or **DAQ unavailable**. |
| **Locked** | The drawer may remain open for non-Live operations, but the section owned by an operation is read-only. This includes Starting, Paused, and Stopping periods. | Settings for the owned device cannot change; settings for an unowned device remain available. | For example, **Camera settings are locked while Dataset Capture is active**. |
| **Active** | During a non-Live operation, the owned section remains locked while status continues to update. During Live Sorting, the complete drawer closes and cannot be opened until the Run ends. | No owned Camera or DAQ setting can change. | The active operation is identified directly; conflicting actions show **Another operation is active**. |

After the owning operation ends, the corresponding controls return to their current availability state.

### 1.8 Startup workspace

Every application launch opens:

```text
Data > Capture > Single Image
```

OpenDSS does not remember or restore the previously opened workspace.

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
├──────────────────┴──────────────────────────────────────┴────────────────────────┤
│ One contextual fault banner appears inside the affected workspace when needed.  │
└──────────────────────────────────────────────────────────────────────────────────┘
                                                           ┌──────────────────────┐
                                                           │ SLIDE-OUT HARDWARE   │
                                                           │ Camera               │
                                                           │ DAQ                  │
                                                           └──────────────────────┘
```

---

## 2. Navigation hierarchy

### 2.1 Approved hierarchy

```text
Data
├── Capture
│   ├── Single Image
│   ├── Image Sequence
│   └── Dataset Capture
├── Label
└── Sequence Player

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

`Single Image`, `Image Sequence`, and `Dataset Capture` are three equally prominent mode selectors inside one shared Capture workspace. Live pre-run, running, paused, and completed/interrupted presentations are states of one Live workspace, not separate navigation items.

### 2.2 Navigation item purposes

| Navigation item | Purpose |
|---|---|
| **Data** | Groups source-image acquisition, Dataset labeling, and visual review of recorded Image Sequences. |
| **Data > Capture** | Provides one live-camera workspace with three equal capture modes and a shared preview. |
| **Data > Capture > Single Image** | Captures and saves exactly one full-frame TIFF image. |
| **Data > Capture > Image Sequence** | Records an ordered full-frame TIFF sequence and finalizes `sequence.json`. |
| **Data > Capture > Dataset Capture** | Records a full sequence while fixed qualified processing creates one unlabeled Droplet Crop per detected droplet and finalizes `dataset.json`. |
| **Data > Label** | Defines two or three classes and assigns stable Class IDs to Droplet Crops in one Dataset. |
| **Data > Sequence Player** | Visually plays, pauses, steps, scrubs, changes speed, and zooms a recorded Image Sequence without hardware. |
| **Models** | Groups model creation, observational testing, and local Model Package management. |
| **Models > Train** | Trains a Faster or More Accurate two-class or three-class model from Labeled Droplet Crops. |
| **Models > Model Test** | Applies a selected model to a compatible labeled Dataset and reports classification measurements without changing Active Model state. |
| **Models > Library** | Discovers and manages valid OpenDSS v2 Model Packages and the one global Active Model. |
| **Sort** | Groups live physical sorting and reprocessing of recorded Image Sequences through sorting logic. |
| **Sort > Live** | Combines pre-run configuration, active sorting, Pause, Stop, and post-run actions in one stateful live-camera workspace. |
| **Sort > Sequence Test** | Processes a recorded Image Sequence through fixed detection/crop processing, routing logic, visual route tracking, optional model inference, optional physical DAQ output, and Run persistence. |
| **Results** | Contains persisted outputs from sorting operations only. |
| **Results > Runs** | Lists and reviews Live Sorting and Sequence Test Runs, including Run Summary, Droplet Log, Droplet Crops, and an optional saved Image Sequence. |
| **Settings** | Provides Storage, Application Information, and Diagnostics without duplicating Camera or DAQ controls. |

---

## 3. Complete workspace inventory

| Workspace or distinct mode | User goal | Main content | Primary action | Secondary actions | Required artifact or hardware | Output artifact | Next likely action |
|---|---|---|---|---|---|---|---|
| **Data > Capture > Single Image** | Save one full camera frame. | Shared live preview; equal Capture mode selector; File Name; Save Location; saved-path feedback. | **Capture Image** | Change File Name or Save Location; switch Capture mode. | Camera Streaming; writable image location; no conflicting operation. | One TIFF file; no Dataset, model output, DAQ output, or Run. | Capture another image or switch to Image Sequence or Dataset Capture. |
| **Data > Capture > Image Sequence** | Record ordered full-frame source data. | Shared live preview; Name; Experiment Type; Notes; optional Duration; Save Location; elapsed time; frame count; recording status. | **Start Recording** | Pause; Resume; Stop; open completed sequence in Sequence Player; send completed sequence to Sequence Test. | Camera Streaming; writable sequence location; global long-running-operation slot. | `sequence.json` and numbered full-frame TIFF files. | **Open in Sequence Player** or **Open in Sequence Test**. |
| **Data > Capture > Dataset Capture** | Create one OpenDSS Dataset without model-dependent selection or labeling. | Shared live preview; Dataset Name; Experiment Type; Notes; optional Duration; Save Location; elapsed time; frame count; detected-droplet count; saved Droplet Crop count. Fixed detection and crop processing are active but not editable. | **Start Dataset Capture** | Pause; Resume; Stop; open the completed Dataset in Label. | Camera Streaming; writable Dataset location; loadable fixed qualified processing configuration; global long-running-operation slot. No model or DAQ. | `dataset.json`, a full-frame sequence, and one 64 × 64 grayscale PNG Droplet Crop per detection, initially Unlabeled. | **Open in Label**. |
| **Data > Label** | Define two or three classes and assign Class IDs to Droplet Crops. | Dataset selector; crop grid and selected-crop view; Class IDs and editable Class Names; Image Counts; filters for class, Unlabeled, Skipped, and Removed. | **Assign Class 0, Class 1, or Class 2** to the selected crop(s). | Define two or three classes when first required; relabel; bulk label; Skip; Remove from Dataset; restore Removed; Undo; filter; open in Train. | Readable v2 `dataset.json`; referenced crops readable; selected Dataset not locked by Training or Model Test. No hardware. | Updated labels, class definitions, counts, and crop states in the same `dataset.json`; no user-facing Dataset version. | **Use in Train**. |
| **Data > Sequence Player** | Inspect a recorded Image Sequence visually. | Current frame; frame number; total frame count; timeline; playback position; speed; zoom. | **Play** after a sequence is loaded; **Open Sequence** when empty. | Pause; Previous Frame; Next Frame; scrub timeline; change playback speed; zoom. | Standalone `sequence.json`, a Dataset-referenced sequence, or a Run sequence. No camera, DAQ, model, or training environment. | No new scientific artifact and no DAQ output. | Continue visual review or navigate directly to another workspace. |
| **Models > Train** | Train a two-class or three-class droplet-classification model. | Dataset selector; Dataset and class summary; eligible image counts; Model Type (`Faster` or `More Accurate`); automatic device status; elapsed time; epoch and overall progress; loss, accuracy, per-class accuracy, macro F1, and estimated time remaining. No Advanced Training Parameters. | **Start Training**; after technical completion, **Save Model**. | Stop Training; select Model Type; name model; choose Save Location; open saved model in Model Test or Library. | Compatible labeled Dataset with readable Labeled crops; bundled training environment; writable temporary and final locations; global long-running-operation slot. GPU is optional automatic acceleration; CPU is the fallback. | Model Package containing `metadata.json`, `checkpoint.pth`, and `model.onnx`; saved model becomes Active Model. | **Open in Model Test** or review in **Models > Library**. |
| **Models > Model Test** | Measure classification behavior of one model on one compatible labeled Dataset. | Model selector; Dataset selector; class compatibility; automatic execution-device status; progress; Overall Accuracy; Per-Class Accuracy; Confusion Matrix. The model's source Dataset is permitted without warning when technically compatible. | **Start Model Test** | Stop Model Test; open or export predictions CSV; select another compatible model or Dataset. | Readable two-class or three-class Model Package; compatible labeled Dataset with the same class count; readable Labeled crops; writable output location; global long-running-operation slot. GPU acceleration is optional and automatic; CPU fallback never blocks the operation. | `model_test_summary.json` and per-image `predictions.csv`; not a Run and not listed in Results. | Review/export results or select another model or Dataset. |
| **Models > Library** | Manage valid OpenDSS v2 Model Packages and choose the global Active Model. | Model list; Model Name; Model Type/architecture; class count; Class IDs and Class Names; creation date; source Dataset; Active state; package location; training metadata and metrics. | **Set Active** | Import Model; Export Model; Duplicate Model; Rename Model; Delete Model; **Open in Model Test**. | Valid OpenDSS v2 Model Packages for package operations. No hardware. Packages in use are protected from conflicting mutation. | ModelRegistry updates and, depending on action, copied, renamed, duplicated, imported, exported, or deleted package files. | **Open in Model Test** or open **Sort > Live** with the selected Active Model. |
| **Sort > Live — pre-run** | Configure and start a Live Sorting Run while viewing the live camera. | Live camera view; Run Name; Experiment Type; Notes; optional Duration; Save Location; Trigger Mode; Active Model when applicable; Hit Class for Class-Based Sorting; Hit Outlet Direction; Record Full Image Sequence; Open/Save/Save As Setup Profile; Send Test Pulse; Start Sorting. Camera and DAQ settings are available only in the shared drawer. | **Start Sorting** | Open Profile; Save Profile; Save Profile As; Send Test Pulse; change run selections; open the hardware drawer while idle. | Camera Streaming; DAQ Ready; Hit Outlet Direction; writable Run location; global slot. Class-Based Sorting also requires a loadable Active Model and Hit Class. Trigger Every Droplet does not require a model. | On Start, a new Run folder, initial `run_summary.json`, recoverable Droplet Log, and configuration snapshot. | **Sort > Live — running**. |
| **Sort > Live — running** | Monitor physical sorting and intervene with Pause or Stop. | Live camera view; Run status; elapsed time; Trigger Mode; Active Model when present; Hit Class when applicable; Hit Outlet Direction; Total Droplets; Predicted Class counts when a model is present; Decision Hit/Waste; Observed Hit/Waste/Unresolved; Inference Time; camera FPS. Model inference uses the qualified CPU path. Hardware drawer is closed and locked. | **Pause** | **Stop**. | Open Live Run owning Camera, DAQ, Run output, global slot, and selected model use lock when applicable. GPU is not required. | Incrementally persisted Droplet Crops and events; updated `run_summary.json`; optional full Image Sequence. | Resume from Pause, Stop, automatic Duration completion, or fault transition. |
| **Sort > Live — paused** | Hold the same Run without inference, new DAQ output, or new event finalization. | Live camera preview remains active; status `Paused`; elapsed active time and counters remain stable; previous configuration remains visible as read-only Run context. Hardware drawer remains hidden and locked. | **Resume** | **Stop**. | The same open Run and owned Camera/DAQ resources. | Flushed recoverable Run state; no new events while paused. | Return to running or finalize the Run. |
| **Sort > Live — completed/interrupted** | Review the immediate outcome and move to persisted Results or start another Run. | Final status; stop reason or one contextual fault banner; whether recoverable data was preserved; direct actions. | **Open Run Summary** when available; otherwise **Open Run Folder**. | Open Run Folder; Start New Run; direct recovery action shown by the fault banner. | Finalized or recoverable Run folder. | Completed, user-stopped, Interrupted, or Failed Run artifacts as available. | **Results > Runs — selected Run** or **Start New Run**, which returns to Live pre-run. |
| **Sort > Sequence Test** | Reprocess a recorded Image Sequence through sorting logic and optionally issue physical DAQ output. | Sequence selector; Model selector; Trigger Mode; Hit Class when Class-Based Sorting; Hit Outlet Direction; explicit **Physical DAQ Output Enabled** control, initially checked; progress; counters; Stop; completion actions. Fixed detection, crop, routing-algorithm, and internal timing configuration are not editable. | **Start Sequence Test** | Stop Sequence Test; enable or disable physical DAQ output; select model when applicable; open completed Run in Results. | Readable v2 Image Sequence; writable Run location; global slot. Class-Based Sorting requires a model. Trigger Every Droplet may omit the model. DAQ Ready is required only when Physical DAQ Output is enabled. No camera. | Run folder with `run_summary.json`, `events.csv`, Droplet Crops, and source-sequence reference. | **Open Run Summary** in Results. |
| **Results > Runs — list** | Find a Live Sorting or Sequence Test Run. | Run list showing Run Name, operation type, start timestamp, duration, persisted status (`Completed`, `Stopped`, `Interrupted`, or `Failed`), model name when present, and Total Droplets. | **Open selected Run** | Select another Run. | Discoverable Live Sorting or Sequence Test Run folders. No hardware. | No new artifact. | **Results > Runs — selected Run**. |
| **Results > Runs — selected Run** | Review factual Run provenance and outputs without changing historical event data. | Run information; model/routing snapshot; hardware and fixed processing configuration; Total Droplets; Predicted Class counts; Decision counts; Observed Hit/Waste/Unresolved counts; Decision-versus-Observed Route matrix with an Unresolved column; Notes; file links. | **Open Droplet Log** | Open Run Folder; open a referenced Droplet Crop; Open Saved Sequence when present; Edit Notes; Save Notes. | Readable `run_summary.json`; related Run files as available. No hardware. | Notes updates in `run_summary.json`; historical events remain unchanged. | **Open Saved Sequence** in Sequence Player when a full sequence exists. |
| **Settings** | Manage storage preferences and inspect local application/runtime information. | Storage; Application Information; Diagnostics; default data root; OpenDSS and schema versions; runtime and driver availability; diagnostic-folder access. | **Choose Default Data Root** | Open data root in Windows Explorer; open diagnostic folder. | No hardware. A selected data root must be a valid writable location before it becomes the default. | Updated storage/application preferences; no scientific artifact. | Return directly to any workspace. |


### 3.1 Shared sorting semantics

| Trigger Mode | Model behavior | Decision behavior | Observed Route behavior |
|---|---|---|---|
| **Class-Based Sorting** | A model is required. The largest Class Score determines Predicted Class. | Predicted Class equal to Hit Class produces **Decision = Hit**; all other predictions produce **Decision = Waste**. | Visual trajectory is evaluated independently as **Hit**, **Waste**, or **Unresolved**. |
| **Trigger Every Droplet** | A model is optional. When present, classification still runs and Predicted Class and Class Scores are logged; when absent, those fields remain empty. | Every detected droplet produces **Decision = Hit**. Classification does not control the Decision. | Visual trajectory is still evaluated independently as **Hit**, **Waste**, or **Unresolved**. |

Physical DAQ output follows the Decision only in operations where physical output is enabled and the DAQ is Ready.

### 3.2 Setup Profile behavior in Live pre-run

- A Setup Profile is one ordinary OpenDSS v2 file operated through **Open Profile**, **Save Profile**, and **Save Profile As**.
- A Profile may carry Camera settings, DAQ settings, Active Model reference, Trigger Mode, Hit Class, Hit Outlet Direction, Record Full Image Sequence, Run Name, and default Save Location.
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
| **Failed** | One contextual banner states that capture or file writing failed and does not claim success. | **Capture Image** enabled only after the blocker clears. | The current camera or file-write reason. | Restore the camera or choose a writable location, then capture again. |

### 4.3 Data > Capture > Image Sequence

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Unavailable** | Live-preview region plus setup fields; the blocking condition is shown at Start Recording. | **Start Recording** disabled. | **Camera unavailable**, **Output folder is not writable**, or **Another operation is active**. | Restore the prerequisite. |
| **Ready** | Streaming preview and editable metadata, optional Duration, and Save Location. | **Start Recording** enabled. | — | Start the sequence. |
| **Starting** | Sequence folder creation and writer initialization; setup fields and mode switching are locked. | **Stop** enabled once the operation has been accepted. | Start Recording is disabled because **Recording is starting**. | Continue into Running or stop. |
| **Running** | Preview, elapsed time, frame count, and recording status. | **Pause** enabled. | Start Recording and mode switching are disabled because **Recording is active**. | Pause, Stop, or allow Duration to expire. |
| **Paused** | Preview continues; frame writing and active-recording time stop; the same sequence remains open. | **Resume** enabled. | Start Recording is disabled because **The current sequence is paused**. | Resume or Stop. |
| **Stopping** | New frame writes have stopped and `sequence.json` is being finalized. | No new primary action. | **Sequence is being finalized**. | Continue to Completed, Interrupted, or Failed presentation. |
| **Completed** | Final frame count, stop reason, saved location, and direct sequence actions. | **Open in Sequence Player** enabled. | — | Open in Sequence Player or Sequence Test. |
| **Interrupted** | One banner states what interrupted recording and whether frames and metadata were preserved. | **Open Sequence** when recoverable; otherwise **Open Folder**. | A direct device or file reason. | Inspect preserved output, correct the cause, or start a new recording. |
| **Failed** | One banner states that the sequence could not be finalized or written. | **Start Recording** enabled after the cause clears. | The direct camera or file failure. | Open the folder if partial files exist, correct the cause, and retry. |

### 4.4 Data > Capture > Dataset Capture

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Unavailable** | Preview region and Dataset setup fields; fixed processing is described as application configuration, not editable controls. | **Start Dataset Capture** disabled. | **Camera unavailable**, **Processing configuration unavailable**, **Output folder is not writable**, or **Another operation is active**. | Restore the camera/configuration, choose a writable folder, or stop the conflicting operation. |
| **Ready** | Streaming preview; Dataset metadata; optional Duration; Save Location. | **Start Dataset Capture** enabled. | — | Start capture. |
| **Starting** | Dataset folder, sequence writer, crop writer, and metadata recovery state are initialized; setup fields and mode switching lock. | **Stop** enabled once accepted. | Start is disabled because **Dataset Capture is starting**. | Continue into Running or stop. |
| **Running** | Preview, elapsed time, full-frame count, detected-droplet count, and saved Droplet Crop count. | **Pause** enabled. | Start and mode switching are disabled because **Dataset Capture is active**. | Pause, Stop, or allow Duration to expire. |
| **Paused** | Preview continues; frame writing, detection, and Droplet Crop creation stop; the same Dataset remains open. | **Resume** enabled. | Start is disabled because **Dataset Capture is paused**. | Resume or Stop. |
| **Stopping** | New acquisition and detection stop while writers flush and `dataset.json` is finalized. | No new primary action. | **Dataset is being finalized**. | Continue to Completed, Interrupted, or Failed presentation. |
| **Completed** | Final counts, saved location, and completed Dataset actions. | **Open in Label** enabled. | — | Open the Dataset in Label. |
| **Interrupted** | One banner states the direct cause and whether existing frames and crops were preserved in a recoverable Dataset. | **Open Dataset** when recoverable; otherwise **Open Folder**. | The direct camera, processing, or write failure. | Inspect preserved data, fix the cause, or start a new capture. |
| **Failed** | One banner states that capture or finalization failed; success is not claimed. | **Start Dataset Capture** enabled after recovery. | The current direct technical failure. | Open preserved files when available, correct the cause, and retry. |

### 4.5 Data > Label

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | An empty Dataset selection state with no crop browser content. | **Open Dataset** enabled; class-assignment actions disabled. | **No dataset selected**. | Select a v2 `dataset.json` or use **Open in Label** from Dataset Capture. |
| **Unavailable** | Dataset identity or attempted file path plus one direct incompatibility or lock message. | Class-assignment actions disabled. | **Dataset is in use by Training**, **Dataset is in use by Model Test**, **Required Droplet Crop is missing**, or **Unsupported OpenDSS v2 schema**. | End the owning operation, restore the missing file, or select a supported Dataset. |
| **Ready** | Crop grid, selected crop, stable Class IDs, editable Class Names, Image Counts, and filters. | **Assign Class** enabled for a valid selection. | For no selected crop: **No Droplet Crop selected**. | Label, Skip, Remove, restore, Undo, or open the same Dataset in Train. |
| **Failed** | One banner states that a Dataset could not be loaded or label changes could not be saved. | **Retry Save** or **Open Dataset**, depending on the failure. | The direct parse, missing-file, or write reason. | Correct the file or permissions and retry; do not treat unsaved changes as persisted. |

### 4.6 Data > Sequence Player

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | Empty viewer and playback controls without a frame. | **Open Sequence** enabled; Play disabled. | **No sequence selected**. | Select a standalone, Dataset, or Run sequence. |
| **Ready** | Current frame, frame number, total frames, timeline, speed, and zoom. | **Play** enabled. | — | Play, step, scrub, or continue visual review. |
| **Running** | Frames advance in order at the selected visual playback speed. | **Pause** enabled. | Open Sequence may be disabled while playback is active. | Pause or let playback reach the final frame. |
| **Paused** | Current frame remains displayed; stepping and scrubbing are available. | **Play** enabled. | — | Resume, step, scrub, or select another sequence. |
| **Completed** | Final frame remains displayed and the timeline is at the end. | **Previous Frame** and timeline scrubbing remain enabled. | — | Move to an earlier frame, then use Play, or select another sequence. |
| **Failed** | One banner identifies an unreadable `sequence.json` or missing/unreadable frame. | **Open Sequence** enabled; Play disabled. | The direct sequence or frame error. | Restore the file or select another valid sequence. |

### 4.7 Models > Train

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | Dataset selector with no Dataset summary. | **Start Training** disabled; Dataset selection remains available. | **No dataset selected**. | Select a labeled v2 Dataset. |
| **Unavailable** | Dataset summary plus the first direct technical blocker. | **Start Training** disabled. | **No Labeled Droplet Crops**, **Required Droplet Crop is missing**, **Training environment unavailable**, **Output folder is not writable**, or **Another operation is active**. | Correct the stated prerequisite. No class-balance or model-quality blocker is shown. |
| **Ready** | Dataset/classes/counts, Model Type, and factual automatic device selection. | **Start Training** enabled. | — | Start Training. |
| **Starting** | Training environment initialization and configuration snapshot; selection controls lock. | **Stop Training** enabled once accepted. | Start is disabled because **Training is starting**. | Continue into Running or stop. |
| **Running** | Elapsed time, epoch and overall progress, loss, accuracy, per-class accuracy, macro F1, estimated time remaining, and selected device. | **Stop Training** enabled. | Another Start is disabled because **Training is active**. | Continue or stop. |
| **Stopping** | Trainer termination and temporary-output finalization; no normal model-save claim. | No new primary action. | **Training is stopping**. | Continue to Interrupted or Failed unless a technically complete result exists. |
| **Completed** | Completed metrics and a Name Model/Save Location step. | **Save Model** enabled when the final location is writable. | **Save location is not writable** when applicable. | Save the package; it becomes Active Model, then open Model Test or Library. |
| **Interrupted** | One banner states that Training stopped before normal completion and whether diagnostic or temporary output remains. | **Start Training** enabled after resources release. | — | Correct the cause or change inputs, then start again. No normal package is created unless required artifacts were technically completed and explicitly saved. |
| **Failed** | One banner states the direct training-environment, input, or output failure. | **Retry Training** enabled after the cause clears. | The direct technical failure. | Open diagnostics when useful, correct the cause, and retry. |

### 4.8 Models > Model Test

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | Model and Dataset selectors without result content. | **Start Model Test** disabled. | **No model selected** or **No dataset selected**. | Select both artifacts. |
| **Unavailable** | Loaded artifact summaries and the first direct incompatibility. | **Start Model Test** disabled. | For example: **The selected model has 2 output classes, but the selected Dataset defines 3 classes**; **No Labeled Droplet Crops**; **Output folder is not writable**; or **Another operation is active**. | Select compatible/readable artifacts, choose a writable location, or stop the conflicting operation. GPU unavailability is not a blocker because CPU fallback is automatic. |
| **Ready** | Compatible model/Dataset summary and factual planned device, GPU when available or CPU otherwise. | **Start Model Test** enabled. | — | Start Model Test. |
| **Starting** | Artifact loading and test-output initialization; selectors lock. | **Stop Model Test** enabled once accepted. | Start is disabled because **Model Test is starting**. | Continue into Running or stop. |
| **Running** | Progress and factual execution-device status. | **Stop Model Test** enabled. | Another Start is disabled because **Model Test is active**. | Continue or stop. |
| **Stopping** | New inference stops and output is finalized as far as possible. | No new primary action. | **Model Test is stopping**. | Continue to Interrupted, Failed, or Completed if all required output finalized. |
| **Completed** | Overall Accuracy, Per-Class Accuracy, Confusion Matrix, output location, and device used. | **Open Predictions CSV** enabled. | — | Review/export results or run another test. |
| **Interrupted** | One banner states that the test stopped before completion and whether partial output was preserved. | **Start Model Test** enabled after resources release. | — | Correct the cause or choose different artifacts and start again. |
| **Failed** | One banner states the direct model, Dataset, runtime, or write failure. | **Retry Model Test** enabled after the cause clears. | The direct technical failure. | Correct the cause and retry. |

### 4.9 Models > Library

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | No discovered v2 Model Packages and no selected-model details. | **Import Model** enabled; Set Active disabled. | **No model selected**. | Import a valid v2 Model Package or train a model. |
| **Ready** | Model list, selected-model provenance/metrics, package location, and Active state. | **Set Active** enabled for a valid nonactive selection when the current Active Model is not operation-locked. | **Selected model is already Active** when applicable. Set Active or package mutation actions that would replace, delete, or alter a model in use are disabled with **Model is in use by [operation]**. | Set Active, manage the package, or open it in Model Test. |
| **Failed** | One banner identifies a package parse/load, copy, rename, delete, registry, or permission failure. | The relevant **Retry** or **Import Model** action remains available when safe. | The direct package or file-system reason. | Select a supported v2 package, correct permissions, or retry the requested operation. |

### 4.10 Sort > Live — pre-run

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Unavailable** | Live preview region, run configuration, Trigger Mode, routing selections, profile actions, and hardware status. With no camera, the preview reads **Camera unavailable**. | **Start Sorting** disabled. | **Camera unavailable**, **DAQ unavailable**, **No active model** for Class-Based Sorting, **No Hit Class selected**, **No Hit Outlet Direction selected**, **Output folder is not writable**, or **Another operation is active**. | Correct the one stated prerequisite. Trigger Every Droplet remains available without a model. |
| **Ready** | Streaming preview; complete run selections; hardware drawer available; Start is technically ready. | **Start Sorting** enabled. | — | Start Sorting. |

### 4.11 Sort > Live — running

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Starting** | Run folder and initial structured files are created, effective configuration is snapshotted, the drawer closes, and the right panel changes toward monitoring. | **Stop** enabled once accepted. | Start is disabled because **Sorting is starting**. | Continue into Running or stop. |
| **Running** | Live preview; Run status; elapsed time; routing context; Total Droplets; Predicted Class counts when present; Decision counts; Observed Route counts including Unresolved; Inference Time; camera FPS. | **Pause** enabled. | Start and configuration controls are unavailable because **Sorting is active**. | Pause, Stop, allow Duration to expire, or transition on fault. |
| **Stopping** | New inference and DAQ output have stopped; persistence queues flush; Run Summary and Droplet Log finalize. | No new primary action. | **Run is stopping**. | Continue to Completed, Interrupted, or Failed presentation. |

### 4.12 Sort > Live — paused

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Paused** | Live camera preview remains active; inference, new DAQ output, and new event finalization are stopped; counters remain stable; drawer is hidden and locked. | **Resume** enabled. | Start and hardware settings are unavailable because **The current Run is paused**. | Resume the same Run or Stop it. |

### 4.13 Sort > Live — completed/interrupted

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Completed** | Final Run status, stop reason, summary values, and saved location. A clean user Stop may appear in this presentation with its persisted stop reason/status. | **Open Run Summary** enabled. | — | Open Results or Start New Run. |
| **Interrupted** | One persistent banner states what stopped, the direct cause, whether partial data was preserved, and direct recovery actions. | **Open Run Summary** when recoverable; otherwise **Open Run Folder**. | — | Inspect preserved output, correct the cause, or Start New Run. |
| **Failed** | One persistent banner states the direct failure and identifies any Run folder or partial files that remain. | **Open Run Folder** when one exists. | — | Correct the technical cause, then Start New Run. |

### 4.14 Sort > Sequence Test

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | Sequence, Model, Trigger Mode, Hit Class, Hit Outlet Direction, Physical DAQ Output, and output-location controls without a selected sequence. | **Start Sequence Test** disabled; Sequence selection enabled. | **No sequence selected**. | Select a v2 Image Sequence. |
| **Unavailable** | Selected artifact summaries plus the first direct blocker. | **Start Sequence Test** disabled. | **No model selected** for Class-Based Sorting, **DAQ unavailable** while Physical DAQ Output is enabled, **No Hit Outlet Direction selected**, **Output folder is not writable**, **Unsupported OpenDSS v2 sequence**, or **Another operation is active**. | Select/correct the required artifact, disable physical DAQ output when appropriate, restore DAQ, or stop the conflicting operation. |
| **Ready** | Valid sequence; mode-dependent model state; explicit physical-output state; routing selections. | **Start Sequence Test** enabled. | — | Start Sequence Test. |
| **Starting** | Run output initialization and sequence-loading status; inputs lock. | **Stop Sequence Test** enabled once accepted. | Start is disabled because **Sequence Test is starting**. | Continue into Running or stop. |
| **Running** | Processing progress, event/counter status, routing context, and physical-output state. | **Stop Sequence Test** enabled. | Another Start is disabled because **Sequence Test is active**. | Continue or stop. |
| **Stopping** | New processing and DAQ output stop; event and Run files finalize. | No new primary action. | **Sequence Test is stopping**. | Continue to Completed, Interrupted, or Failed presentation. |
| **Completed** | Final counts, status, and direct Run actions. | **Open Run Summary** enabled. | — | Open Results. |
| **Interrupted** | One banner states what interrupted the test, whether recoverable Run data was preserved, and the direct cause. | **Open Run Summary** when recoverable; otherwise **Open Run Folder**. | — | Inspect output, correct the cause, or start another test. |
| **Failed** | One banner states the direct sequence, model, DAQ, processing, or write failure. | **Retry Sequence Test** enabled after the cause clears. | The direct technical failure. | Correct the cause and retry; open any preserved Run folder. |

### 4.15 Results > Runs — list

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Empty** | The Runs list states that no Live Sorting or Sequence Test Runs were found. | No Run-open action is enabled. | **No Runs found**. | Create a Run in Live or Sequence Test. |
| **Ready** | Discoverable Runs and their factual list columns. | **Open selected Run** enabled after selection. | Before selection: **No Run selected**. | Select and open a Run. |
| **Failed** | One banner identifies a Run-discovery, storage-root, permissions, or index-read failure. | Run opening is disabled for unreadable entries. | The direct file or storage reason. | Correct the data root, restore permissions/files, and reopen Runs. |

### 4.16 Results > Runs — selected Run

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Ready** | Run Summary, provenance, factual counts, Decision-versus-Observed Route matrix, Notes, and available file links. | **Open Droplet Log** enabled when `events.csv` is available. | For a missing optional sequence: **No saved Image Sequence for this Run**. | Review files, edit Notes, or open the saved sequence when present. |
| **Unavailable** | Run identity remains visible, but one required selected artifact cannot be read. | The affected file action is disabled; unaffected direct file actions remain available. | **Droplet Log unavailable**, **Run Summary cannot be read**, or another direct missing-file reason. | Open the Run folder, restore the file, or select another Run. |
| **Failed** | One banner identifies a Notes save or direct file-open failure. | **Retry Save Notes** or the unaffected file action. | The direct permission or file-system reason. | Correct permissions/path and retry; historical event data is not rewritten. |

### 4.17 Settings

| State | What the user sees | Primary action | Direct disabled reason | Recovery or next action |
|---|---|---|---|---|
| **Ready** | Storage, Application Information, Diagnostics, version information, runtime/driver availability, and current data root. | **Choose Default Data Root** enabled. | — | Select a writable root or open the current data/diagnostic folder. |
| **Failed** | One banner states that a preference could not be saved or a folder could not be opened. | **Choose another location** or retry the direct folder action. | The direct permission or path reason. | Select a valid writable location or restore access. |

---

## 5. Contextual workflow links

Contextual links carry an artifact selection into another persistent workspace. They do not lock the user into a wizard, create a project, or prevent direct navigation.

| From | Contextual link | Preselected artifact | Destination behavior |
|---|---|---|---|
| **Dataset Capture** | **Open in Label** | The completed or recoverable v2 `dataset.json`. | Opens Data > Label with that Dataset selected. |
| **Label** | **Use in Train** | The currently open Dataset. | Opens Models > Train with the Dataset selected; Training still applies its own technical prerequisites. |
| **Train** | **Open in Model Test** | The newly saved Model Package; the current source Dataset may remain selected when still available. | Opens Models > Model Test without making Model Test mandatory. |
| **Model Library** | **Open in Model Test** | The selected Model Package. | Opens Models > Model Test with the model selected. |
| **Image Sequence** | **Open in Sequence Player** | The completed `sequence.json`. | Opens Data > Sequence Player at the sequence. |
| **Image Sequence** | **Open in Sequence Test** | The completed `sequence.json`. | Opens Sort > Sequence Test with the sequence selected. |
| **Live or Sequence Test Run** | **Open Run Summary** | The newly finalized or recoverable Run. | Opens Results > Runs with that Run selected. |
| **Results** | **Open Saved Sequence** | The Run's full Image Sequence, only when it exists. | Opens Data > Sequence Player with that sequence selected. |

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

| Operation | Camera | DAQ | Dataset write access or lock | Model Package write access or lock | Run output | Global long-running slot | Hardware drawer and conflicting actions |
|---|---:|---:|---|---|---|---:|---|
| **Single Image** | X, momentary | — | — | — | — | No; momentary reservation | Camera controls are briefly unavailable during capture. Capture Image is disabled when another operation owns Camera or the storage pipeline. |
| **Image Sequence recording** | X | — | — | — | — | Yes | Camera section locks; DAQ section remains independently available if idle. All other long-running Start actions show **Another operation is active**. |
| **Dataset Capture** | X | — | W on the new Dataset | — | — | Yes | Camera section locks; DAQ remains independently available if idle. Label cannot modify the Dataset until capture finalizes. |
| **Label** | — | — | W on the selected Dataset | — | — | No | Drawer is unaffected. Label writes are disabled while Training or Model Test holds an R-lock on the same Dataset. |
| **Training** | — | — | R-lock on the selected Dataset | W on the new Model Package during final save | — | Yes | Drawer remains available because Training owns no hardware. Other long-running Start actions are disabled; Label cannot modify the selected Dataset. |
| **Model Test** | — | — | R-lock on the selected Dataset | R-lock on the selected Model Package | —; Model Test output is not a Run | Yes | Drawer remains available. Label and conflicting model-package mutation are disabled for the selected artifacts. |
| **Model Library package mutation** | — | — | — | W on the selected package/registry for the mutation | — | No | Drawer is unaffected. Delete, replace, rename, or duplicate actions are disabled when the package is in use by an operation. |
| **Sequence Player** | — | — | Read only when opening a Dataset sequence | — | Read only when opening a Run sequence | No | Drawer is unaffected. Playback does not own the global slot. |
| **Live Sorting** | X | X | — | R-lock on the selected/Active Model when present | W | Yes | The complete drawer closes and remains unavailable until the Run ends. All conflicting starts and model-package mutations are disabled. |
| **Sequence Test — physical DAQ output enabled** | — | X | — | R-lock on the selected Model Package when present | W | Yes | DAQ section locks; Camera section remains independently available if idle. Other long-running starts are disabled. |
| **Sequence Test — physical DAQ output disabled** | — | — | — | R-lock on the selected Model Package when present | W | Yes | Drawer remains available because no hardware is owned. Other long-running starts are still disabled by the global slot. |
| **Results — edit Run Notes** | — | — | — | — | Notes W on a nonactive Run | No | Drawer is unaffected. Only Notes change; `events.csv`, Droplet Crops, and historical event values remain unchanged. |

### 6.3 Lock effects

1. **Global long-running-operation slot**  
   Image Sequence recording, Dataset Capture, Training, Model Test, Sequence Test, and Live Sorting are mutually exclusive. A conflicting Start action is disabled with **Another operation is active**.

2. **Camera ownership**  
   Image Sequence, Dataset Capture, and Live Sorting lock Camera settings. Single Image reserves the Camera momentarily. Camera-independent workspaces remain usable.

3. **DAQ ownership**  
   Live Sorting always owns the DAQ. Sequence Test owns it only when Physical DAQ Output is enabled. Send Test Pulse requires DAQ Ready and creates no Run.

4. **Dataset ownership**  
   Label writes the selected Dataset. Training and Model Test hold an R-lock on the selected Dataset so Label cannot change it during either operation.

5. **Model Package ownership**  
   Training writes a new package only after technical completion and explicit save. Model Test, Live, and Sequence Test protect any selected package from conflicting mutation while in use. When an operation uses the current Active Model, Set Active cannot replace that selection until the operation ends.

6. **Run ownership**  
   Live and Sequence Test exclusively write their Run output until finalization. Results reads finalized or recoverable Runs and may update Notes only after the Run is no longer active.

7. **Configuration snapshot**  
   At operation start, OpenDSS snapshots the effective configuration needed for provenance. Later navigation or idle-setting changes do not alter an active capture or Run.

---

## 7. Requirements-to-workspace trace

### 7.1 Detailed workflow specification trace

| Detailed workflow section | OpenDSS v2 implementation location | Consolidated coverage and controlling amendment |
|---|---|---|
| **WF §§7–9 — Primary navigation, global shell, operation/concurrency rules** | Application shell; all workspaces | Preserves direct persistent workspaces, global statuses, disabled reasons, one active long-running operation, and Stop. PM §§4–5 and §§13–14 replace the old navigation placement where necessary, add the shared hardware drawer, define startup, and simplify fault communication. |
| **WF §11 — Capture a Single Image** | Data > Capture > Single Image | Preserves live preview, optional timestamp name, writable Save Location, one TIFF per action, no model/DAQ/Dataset/Run. |
| **WF §12 — Capture an Image Sequence** | Data > Capture > Image Sequence | Preserves optional Duration, Pause/Resume/Stop, numbered TIFF frames, `sequence.json`, and direct handoff to Sequence Player. PM §7.1 places it as one of three equal modes in the shared Capture workspace. |
| **WF §13 — Capture a Dataset** | Data > Capture > Dataset Capture | Preserves model-independent Dataset Capture, full sequence, one Droplet Crop per detection, counters, interruption recovery, and `dataset.json`. PM §§7.1 and 11 make detection/crop configuration fixed rather than user-editable. |
| **WF §14 — Label a Dataset** | Data > Label | Preserves two-class and three-class labeling, stable Class IDs, editable Class Names, Skip, Remove from Dataset, restore, Undo, bulk labeling, filters, Image Counts, and in-place Dataset persistence. |
| **WF §15 — Train a Model** | Models > Train | Preserves Dataset selection, Faster/More Accurate, fixed 70/15/15 split, seed 1729, automatic GPU/CPU selection, metrics, Stop, model naming/saving, and automatic activation. PM §§7.4 and 11 remove Advanced Training Parameters and all user-editable hyperparameters. |
| **WF §16 — Model Test** | Models > Model Test | Preserves optional observational testing, same-Dataset permission, class-count blocking, metrics, confusion matrix, and predictions CSV. PM §7.5 adds automatic optional GPU acceleration with CPU fallback. |
| **WF §17 — Model Library** | Models > Library | Preserves Set Active, Import, Export, Duplicate, Rename, Delete, package metadata, and technical package checks. PM §§7.6 and 16 restrict imports to supported v2 packages and add no legacy conversion UI. |
| **WF §18 — Sequence Player** | Data > Sequence Player | Preserves opening standalone, Dataset, or Run sequences; Play/Pause/step/scrub/speed/zoom; no hardware or DAQ output. |
| **WF §19 — Sequence Test** | Sort > Sequence Test | Preserves recorded-sequence processing, both Trigger Modes, optional physical DAQ output, Run creation, and no camera requirement. PM D-005 and D-006 add Observed Route = Unresolved and move the workspace from Models to Sort. |
| **WF §§20–21 — Configure Sorting and Live Sorting** | Sort > Live pre-run, running, paused, and completed/interrupted | All user-facing setup moves into Live pre-run. Live retains camera view, Trigger Mode, Hit Class, Hit Outlet Direction, optional full-sequence recording, Send Test Pulse, Pause, Stop, counters, persistence, and fault handling. PM §§7.7, 9, and 11 move Camera/DAQ controls to the shared drawer, make Setup Profiles ordinary files, and hide fixed processing controls. |
| **WF §22 — Review Runs** | Results > Runs list and selected Run | Preserves Live Sorting and Sequence Test Run discovery, Run Summary, Droplet Log, Droplet Crops, Notes, direct file access, and Decision-versus-Observed Route. PM §§7.9 and 10 add Unresolved and retain Results for Runs only. |
| **WF §23 — Settings** | Shared Camera/DAQ drawer plus reduced Settings | Camera and DAQ technical settings move to the shell drawer. Settings retains only Storage, Application Information, and Diagnostics. Detection, crop, sorting-algorithm, internal timing, and training controls are not exposed. |
| **WF §§24–30 — Artifact and file contracts** | Capture, Label, Train, Library, Sequence Player, Live, Sequence Test, and Results | Preserves canonical v2 files: `sequence.json`, `dataset.json`, `metadata.json`, `run_summary.json`, and `events.csv`. PM §§8–10 update Setup Profiles to ordinary files and add Unresolved to events and matrices. |
| **WF §§31–34 — Persistence, recovery, and error handling** | All long-running workspaces; Results; contextual fault banner | Preserves background persistence, atomic structured-file writes, recoverable partial output, factual errors, and no false success. PM §14 constrains presentation to direct disabled reasons, one contextual banner, and direct recovery actions. |
| **WF §§35–36 — Installation, first launch, and camera-free use** | Application startup; workspace availability states | Preserves offline local operation and camera-free use of Label, Train, Model Test, Library, Sequence Player, Sequence Test when its DAQ condition is satisfied, Results, and Settings. PM §4 requires every launch to open Single Image. |
| **WF §§37–43 — Provenance, nonfunctional requirements, and exclusions** | All artifact-producing workspaces and application boundaries | Preserves reproducibility, responsiveness, data integrity, scientific transparency, offline behavior, and explicit exclusions. PM §§11, 15–17 resolve the v2-only, fixed-configuration, no-Home, no-managed-profile-library boundaries. |
| **WF §§44–72 — End-to-end acceptance scenarios** | Corresponding workspaces and state transitions in this inventory | The scenarios remain coverage targets after applying the approved navigation, Unresolved, Model Test GPU, profile, Settings, and fixed-control amendments. |
| **WF §§73–78 — Application-layer ownership and repository alignment** | Shared-state and lock matrix; architecture boundary | Preserves one authoritative owner per domain and treats repository components as implementation evidence only, never as navigation categories. |

### 7.2 Approved product-model decision trace

| Decision | Implemented by |
|---|---|
| **D-001 — Domain primary navigation** | Section 2 hierarchy: Data / Models / Sort / Results / Settings. |
| **D-002 — No separate software DAQ arming** | Live and Sequence Test use factual DAQ readiness; Send Test Pulse requires DAQ Ready. |
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
| **D-014 — Three equal Capture modes** | Single Image, Image Sequence, and Dataset Capture are equal selectors in one shared live-view workspace. |
| **D-015 — No Advanced Training Parameters** | Train exposes only Dataset, Model Type, factual device/status, progress, metrics, Stop, and model save. |
| **D-016 — Camera/DAQ settings only; immediate while idle** | Shared shell-level hardware drawer and its idle/unavailable/locked/active behavior. |
| **D-017 — Reduced Settings** | Storage, Application Information, and Diagnostics only. |
| **D-018 — Fixed startup workspace** | Every launch opens Data > Capture > Single Image; no last-workspace restore. |
| **D-019 — Simple contextual faults** | Direct disabled reasons, one persistent workspace banner, and direct recovery actions. |

---

## 8. Removed or hidden interface elements

| Removed or hidden element | OpenDSS v2 disposition |
|---|---|
| **Separate Sort Setup** | Removed as a workspace and navigation item. Its user-facing run configuration is the pre-run state of **Sort > Live**. |
| **Editable detector settings** | Hidden from the user. Droplet detection uses fixed qualified application configuration; the effective configuration or version is recorded in Dataset and Run provenance. |
| **Editable crop settings** | Hidden from the user. The Droplet Crop artifact contract remains fixed, including 64 × 64 grayscale PNG output where applicable; effective configuration is recorded. |
| **Editable routing-algorithm settings** | Hidden from the user. The qualified routing algorithm is not a tuning surface. User selections remain Trigger Mode, Hit Class, and Hit Outlet Direction. |
| **Editable internal timing settings** | Hidden from the user. Internal tracking and synchronization timing are fixed. Supported DAQ device/channel/pulse settings remain editable only as DAQ technical settings in the shared drawer. |
| **Advanced Training Parameters** | Removed. The user selects only **Faster** or **More Accurate**; the effective qualified hyperparameters, 70/15/15 split, seed 1729, and automatic device are recorded rather than edited. |
| **Managed Setup Profile library actions** | Removed. Setup Profiles are ordinary v2 files with **Open Profile**, **Save Profile**, and **Save Profile As**. File copy, rename, move, and deletion occur through normal Windows file ownership. |
| **Home screen** | Removed. Every launch opens **Data > Capture > Single Image**. |
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
- **PM §4 — Approved navigation and startup:** final hierarchy, no Home, startup at Data > Capture > Single Image, no last-workspace restore, and contextual rather than mandatory workflow links. fileciteturn0file1L114-L147
- **PM §5 — Global shell and hardware drawer:** header, shell-owned Camera/DAQ drawer, immediate valid changes, locking, and Live drawer closure. fileciteturn0file1L151-L201
- **PM §§7.1–7.3 — Capture, Label, and Sequence Player:** equal Capture modes, fixed capture processing, Label actions, and hardware-free Sequence Player. fileciteturn0file1L282-L348
- **PM §§7.4–7.6 — Train, Model Test, and Library:** no Advanced Training Parameters, automatic device policy, Model Test GPU fallback, and v2 Model Library behavior. fileciteturn0file1L350-L426
- **PM §§7.7–7.8 — Live and Sequence Test:** one Live workspace, pre-run-to-monitor transition, locks, counters including Unresolved, and Sequence Test under Sort. fileciteturn0file1L428-L540
- **PM §§7.9–7.10 — Results and Settings:** Runs-only Results and reduced Settings. fileciteturn0file1L542-L586
- **PM §§8–9 — Artifact and Setup Profile models:** v2 artifact relationships and ordinary-file Profile actions Open, Save, and Save As. fileciteturn0file1L590-L687
- **PM §§10–11 — Scientific event model and editable/fixed configuration:** Predicted Class/Decision/Observed Route separation, Unresolved, Camera/DAQ-only technical editing, and fixed processing/training configuration. fileciteturn0file1L699-L785
- **PM §§12–14 — Dependencies, operation lifecycle, faults, and recovery:** hardware/model prerequisites, one long-running slot, Stop, configuration snapshots, direct disabled reasons, one banner, and direct recovery. fileciteturn0file1L789-L895
- **PM §§16–19 — Legacy boundary, first-release boundaries, state ownership, and decision register:** v2-only product, excluded UI, authoritative owners, and D-001 through D-019. fileciteturn0file1L921-L1024
- **PM §20 — Required amendments:** explicit list of navigation, Unresolved, GPU, Capture, drawer, fixed-control, Profile, legacy, Results, Settings, startup, and fault amendments. fileciteturn0file1L1028-L1046

### Detailed User Workflow Specification

- **WF §§7–9 — Navigation, shell, and operation rules:** original persistent workspace, status, disabled-reason, hardware-unavailable, concurrency, and lifecycle requirements retained where not superseded. fileciteturn0file0L288-L324 fileciteturn0file0L328-L408 fileciteturn0file0L412-L459
- **WF §§11–14 — Data workflows:** Single Image, Image Sequence, Dataset Capture, and Label requirements and artifacts. fileciteturn0file0L510-L567 fileciteturn0file0L571-L695 fileciteturn0file0L699-L878 fileciteturn0file0L882-L1028
- **WF §§15–17 — Model workflows:** Training, Model Test, and Model Library requirements retained subject to the approved v2 amendments. fileciteturn0file0L1034-L1260 fileciteturn0file0L1264-L1380 fileciteturn0file0L1384-L1469
- **WF §§18–19 — Sequence review and testing:** Sequence Player and Sequence Test processing, Trigger Modes, optional DAQ output, and Run creation. fileciteturn0file0L1473-L1657
- **WF §§20–21 — Sorting configuration and Live Sorting:** run metadata, Trigger Mode, Hit Class, Hit Outlet Direction, test pulse, optional full sequence, Live event flow, Pause, Stop, persistence, counters, and faults, consolidated into one Live workspace. fileciteturn0file0L1661-L1882 fileciteturn0file0L1886-L2091
- **WF §22 — Results:** Run list, Run Summary, Decision-versus-Observed Route, Notes, and direct file actions. fileciteturn0file0L2095-L2259
- **WF §23 — Settings:** original setting groups and offline hardware behavior, narrowed by the approved v2 model. fileciteturn0file0L2263-L2345
- **WF §§24–30 — File contracts:** canonical artifact names and Dataset, Sequence, Model, Run, Droplet Log, and Setup Profile contracts, as amended for v2. fileciteturn0file0L2349-L2680
- **WF §§31–34 — Persistence, recovery, and errors:** background writes, flush points, atomic JSON, crash recovery, plain-language errors, and data preservation. fileciteturn0file0L2684-L2829
- **WF §§35–43 — Launch, camera-free use, provenance, nonfunctional requirements, and exclusions:** offline first launch, hardware-free workspaces, reproducibility, responsiveness, data integrity, and prohibited first-release additions. fileciteturn0file0L2833-L3032
- **WF §§44–72 — End-to-end acceptance scenarios:** workflow-level acceptance coverage for installation, capture, labeling, training, testing, sorting, results, Sequence Player/Test, faults, mutual exclusion, and offline behavior. fileciteturn0file0L3036-L3266
- **WF §§73–78 — Workflow ownership, repository alignment, and engineering discretion:** authoritative service boundaries, contract tests, repository-as-evidence rule, and implementation details that do not alter product structure. fileciteturn0file0L3269-L3419
