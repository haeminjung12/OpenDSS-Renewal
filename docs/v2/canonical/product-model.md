# Open Droplet Sorting Suite (OpenDSS)
## Approved v2 Product Model

**Document ID:** ODSS-PM-002  
**Version:** 1.1  
**Status:** Approved product model  
**Date:** July 23, 2026  
**Primary platform:** Windows 11  

---

## 1. Purpose and authority

This document consolidates the approved OpenDSS v2 product decisions D-001 through D-019.

It is the controlling product model for the next design phase: information architecture, screen inventory, low-fidelity interaction design, and application-state definition.

Use the documents in this order:

1. **This approved product model** governs all decisions recorded here.
2. **OpenDSS v2 UI/UX Design Amendment, July 23, 2026** controls every UI, layout, naming, interaction, or workflow matter that it explicitly changes.
3. **OpenDSS Detailed User Workflow Specification, Version 1.1** remains authoritative for detailed requirements that do not conflict with the first two sources.
4. **The existing OpenDSS repository** is implementation evidence and a source of reusable components; it does not define the new product structure.

Where this document conflicts with the original workflow specification, this document supersedes it.

This document is a product model, not a complete replacement requirements specification. The downstream canonical specifications were revised with Version 1.1 to incorporate the approved UI/UX amendment.

### 1.1 Approved UI/UX amendment integration

The July 23, 2026 UI/UX amendment is incorporated into this product model without reopening D-001 through D-019. Where an older statement in this document conflicts with the following integrated decisions, this subsection controls:

- OpenDSS launches maximized. A restored window has a hard minimum logical size of **1600 × 900**, preserves **16:9** during manual resizing, and retains Minimize, Maximize/Restore, and Close.
- The compact one-line status header presents Camera, DAQ, Active Model, and Current Activity with an icon, label, value, readiness color, and a non-color readiness cue.
- The former right-side shell panel is replaced by a bottom-left overlay **Hardware panel** opened and closed by an arrow. It does not push the workspace, span the window, or appear in Settings.
- Startup remains `Data > Capture`. OpenDSS attempts Camera connection automatically. If unavailable, one session-only prompt asks `Camera unavailable. Continue?`; Yes continues without Camera and No closes OpenDSS. `Start Camera` controls streaming.
- Normal visible failures use the message `Error`; technical details go to the program log under `Settings > Diagnostics`. Paths appear only after confirmed writes, and failure never projects success.
- Capture retains one shared Camera preview and three independently collapsible bodies under permanently visible headings: Single Image, Image Sequence, and **Droplet Dataset Capture**. Multiple bodies may be open while idle; an active section remains open while the other headings remain visible but disabled.
- Workspace outer right panels collapse as a whole and share one consistent expanded width, represented by the current 390 px implementation token. Inner disclosure sections retain intrinsic stacking where applicable.
- Label is dominated by the Droplet Crop grid. Its fixed-width, outer-collapsible right panel contains, in order, **Load Dataset**, **Dataset Summary**, **Label**, **Filter**, and bottom-right **Save As**. Class identity uses blue, orange, and purple for Classes 0, 1, and 2; red and green are not Class identity colors.
- **Sequence Viewer** is frame-navigation only. It has no Play, Pause, automatic playback, speed control, or playback lifecycle state.
- Train collects Dataset, Faster or More Accurate, Model Name, and Save Location before Start. Successful Training automatically saves the Model Package and makes it Active. Save failure retains temporary artifacts, exposes Retry Save, and does not activate the Model. Dataset Summary remains a main white region, with a separate main white Results region below it for the two approved live plots and specified completion tables.
- Model Test and Sequence Test always use the Active Model and have no local Model selector. The Active Model cannot be replaced or mutated while Model Test, Live, or Sequence Test uses it. Model Test keeps Dataset Summary as a main white region and places its approved metrics, confusion matrix, and prediction summaries in a separate main white Results region below.
- Library selection is distinct from activation. An Active Model row has a green check plus textual or accessible meaning; Set Active remains explicit; selected Model details live in one collapsible panel.
- Live stays in one workspace from setup through completion. Trigger Every Droplet and DAQ Output are independent toggles. DAQ Output OFF still permits nonphysical processing, logging, and Run persistence without DAQ readiness.
- Hit boundary calibration uses one clicked image point, a horizontal line extending right, and `Top is Hit` or `Bottom is Hit`. It affects Observed Route only and is saved in the Setup Profile and Run Summary.
- Sequence Test accepts OpenDSS Image Sequence folders containing `sequence.json`, uses editable Processing FPS defaulted from recorded FPS, loads through a bounded memory buffer, and keeps Physical DAQ Output off by default.
- Results keeps the loaded Run in the center while a right-side Runs panel requires explicit selection plus Load. Settings contains only Storage, Application Information, Diagnostics, and Visuals; Visuals contains only the application-wide Text Size preference from 80% through 200%, default 100%.

---

## 2. Product definition

OpenDSS is a local, offline Windows 11 scientific application for image-based droplet analysis, model development, and physical droplet sorting.

The primary user is one laboratory scientist operating one local application instance. The product assumes no accounts, roles, cloud workspace, collaboration server, or project-management hierarchy.

OpenDSS supports two primary scientific goals:

```text
MODEL DEVELOPMENT

Droplet Dataset Capture
    → Label Dataset
    → Train Model
    → optional Model Test
    → Active Model
```

```text
PHYSICAL SORTING

Open Live workspace
    → configure run and hardware while viewing the camera
    → Start Sorting
    → monitor Live Sorting
    → Droplet Log
    → Run Summary
```

OpenDSS executes the scientist's choices and records factual measurements and provenance. It does not approve datasets, judge model quality, certify experiments, or decide whether a result is scientifically acceptable.

---

## 3. Governing product principles

### 3.1 User-directed operation

The scientist chooses the operation, Dataset, model, Hit Class, Hit boundary calibration, Trigger Every Droplet, hardware configuration, and output location.

The system blocks only technically impossible or incompatible operations.

### 3.2 Scientific authority remains with the scientist

OpenDSS may show:

- image and class counts;
- training and Model Test metrics;
- Class Scores;
- Decision counts;
- Observed Route counts;
- inference time;
- camera frame rate;
- hardware and file errors.

OpenDSS does not assign approval, rejection, certification, suitability, promotion, or quality states.

### 3.3 Prediction, decision, and observation remain separate

```text
Predicted Class
    → Decision
        → Observed Route
```

- **Predicted Class** is the class with the largest model output score.
- **Decision** is the system's Hit or Waste routing action.
- **Observed Route** is the visually derived route: Hit, Waste, or Unresolved.

A prediction is not an observation. A routing command is not proof of the physical route.

### 3.4 Ordinary local files

Datasets, sequences, model packages, Setup Profiles, and Runs remain normal user-owned Windows files and folders.

The product operates offline and performs no telemetry, update checks, automatic uploads, or required network requests.

### 3.5 Reproducibility without workflow bureaucracy

OpenDSS records the effective software version, model identity, hardware configuration, processing configuration, timestamps, and experimental selections needed to reconstruct an operation.

It does not require projects, tickets, approval states, or user-facing Dataset versions.

---

## 4. Approved primary navigation

```text
Data
├── Capture
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

There is no Home screen and no separate Reports workspace.

On every application launch, OpenDSS opens:

```text
Data > Capture
```

The application does not remember or restore the previously opened workspace.

Workspaces remain directly accessible. Contextual links may carry the selected artifact into the next workflow, but OpenDSS is not a mandatory wizard.

---

## 5. Global application shell

The application shell contains:

1. primary navigation;
2. a compact global status header;
3. the current workspace;
4. a shared slide-out Camera/DAQ hardware panel.

### 5.1 Global status header

The header displays:

```text
Camera | DAQ | Active Model | Current Activity
```

Representative values are:

```text
Camera: Unavailable | Connected | Streaming
DAQ: Unavailable | Ready | Active
Active Model: No Active Model | <Model Name>
Current Activity: Idle | Capturing Image | Recording Sequence |
                  Droplet Dataset Capture | Labeling | Training |
                  Testing Model | Testing Sequence |
                  Sorting | Paused
```

### 5.2 Shared hardware panel

The slide-out panel is owned by the application shell and persists across workspaces.

```text
Hardware
├── Camera
└── DAQ
```

The panel provides the only user-editable technical settings in the first release.

- Camera and DAQ values are shared across workspaces.
- A workspace does not keep its own duplicate copy of hardware settings.
- Valid changes apply immediately while the corresponding device is available and not owned by an active operation.
- Invalid changes are rejected and the last successfully applied value remains active.
- Camera controls lock while a camera-owning operation is running.
- DAQ controls lock while a DAQ-owning operation is running.
- During Live Sorting, the panel closes and remains unavailable until the Run ends.
- The global status header remains visible while the panel is closed.

The panel's open/closed presentation may remain stable during ordinary navigation, but it is not a saved scientific artifact and is not restored as a startup requirement.

---

## 6. Goal-to-workspace product map

### 6.1 Develop a model

```text
USER GOAL
Develop a droplet-classification model
    → Data > Capture > Droplet Dataset Capture
        → Capture Dataset
            → Dataset
                → Data > Label
                    → assign two or three classes and labels
                        → labeled Dataset
                            → Models > Train
                                → Train Faster or More Accurate model
                                    → Model Package
                                        → automatically becomes Active Model
                                            → optional Models > Model Test
```

### 6.2 Run the physical sorter

```text
USER GOAL
Physically sort droplets
    → Sort > Live
        → view live camera and configure run
            → choose Trigger Every Droplet, model when required,
              Hit Class, Hit boundary calibration, and output options
                → Start Sorting
                    → Run
                        → events.csv Droplet Log
                        → run_summary.json Run Summary
                            → Results > Runs
```

### 6.3 Record or inspect image data

```text
USER GOAL
Capture one image
    → Data > Capture > Single Image
        → TIFF
```

```text
USER GOAL
Record full-frame source data
    → Data > Capture > Image Sequence
        → Image Sequence
            → Data > Sequence Viewer
            → Sort > Sequence Test
```

### 6.4 Test a model

```text
USER GOAL
Measure classification behavior on a labeled Dataset
    → Models > Model Test
        → Model + compatible labeled Dataset
            → Model Test summary and predictions CSV
```

### 6.5 Reprocess a recorded sequence

```text
USER GOAL
Apply sorting logic to recorded frames
    → Sort > Sequence Test
        → Sequence + optional/required Model + optional physical DAQ output
            → Run
                → Results > Runs
```

---

## 7. Workspace model

## 7.1 Data > Capture

Capture is one shared live-camera workspace with one Camera preview and three collapsible operation sections in the right-side panel:

```text
┌──────────────────────────────┬──────────────────────────────┐
│ LIVE CAMERA PREVIEW          │ ▸ Single Image               │
│                              │ ▸ Image Sequence             │
│                              │ ▸ Droplet Dataset Capture            │
└──────────────────────────────┴──────────────────────────────┘
```

The live camera view remains the primary visual area for all three capture types. All three section headings remain fixed and visible. Sections expand and collapse independently, multiple sections may be open, and all three are collapsed when the workspace first opens. Expanded bodies divide the remaining panel height equally and scroll independently. An active section is forced open while the other headings are disabled; after Completed, Interrupted, or Failed, that section remains expanded until the user collapses it.

| Section | Primary output | Primary action |
|---|---|---|
| Single Image | One TIFF | Capture Image |
| Image Sequence | `sequence.json` and numbered TIFF frames | Start Recording |
| Droplet Dataset Capture | `dataset.json`, full-frame sequence, and one Droplet Crop per detection | Start Droplet Dataset Capture |

Camera settings are accessed through the shared hardware panel, not duplicated inside any capture section.

During Image Sequence or Droplet Dataset Capture:

- the live preview remains visible;
- the active section remains expanded and the other two section headings remain visible but disabled;
- the workspace shows elapsed time and operation counters;
- Pause, Resume, and Stop operate on the same capture;
- camera controls remain unavailable until the operation ends.

Droplet Dataset Capture is model-independent. It does not assign labels, predictions, Hit/Waste meaning, or scientific acceptance.

Droplet detection, crop generation, and associated internal timing use fixed qualified application configuration. These parameters are not user-editable.

## 7.2 Data > Label

Label opens one OpenDSS v2 `dataset.json` through the authoritative Dataset contract.

For a Dataset without configured classes, the scientist selects exactly one initial class schema:

```text
2 Classes: Class IDs 0 and 1
3 Classes: Class IDs 0, 1, and 2
```

Once configured, the class count and Class IDs are immutable; the normal product does not switch or migrate a configured Dataset between two and three classes. Class Names remain editable.

The Droplet Crop grid dominates the main area. One fixed-width, outer-collapsible right panel contains, in order:

1. **Load Dataset**, a static, always-expanded card/header;
2. **Dataset Summary**, showing total and labeled counts, the initial two-or-three-class setup for an unconfigured Dataset, or the immutable configured class schema without class-count switching;
3. **Label**, showing the selected-crop preview and exactly **Class 0**, **Class 1**, **Class 2**, **Exclude**, **Undo**, **Previous**, and **Next**; Class 2 is unavailable for a two-class Dataset;
4. **Filter**, showing class list/count filters and **Excluded** and **Unreviewed** when applicable;
5. **Save As** at the bottom-right.

Class assignment supports initial labeling and relabeling. **Exclude** retains the crop artifact and excludes it from Training and Model Test. The former **Skip**, **Remove**, and **Restore** actions are not part of this composition.

Normal label changes continue saving to the current Dataset under the existing Dataset persistence contract. **Save As** creates an independent Dataset copy and makes that copy the current loaded Dataset. It is not Dataset version history.

Only Labeled crops are eligible for Training and Model Test.

## 7.3 Data > Sequence Viewer

Sequence Viewer opens a standalone v2 `sequence.json`, a Dataset sequence, or a Run sequence.

It provides Previous and Next frame actions, direct frame seek, zoom, pan, Fit, and 1:1. It has no automatic playback or frame-navigation lifecycle.

It requires no camera, DAQ, model, or training environment and never produces DAQ output.

## 7.4 Models > Train

Training accepts one compatible labeled Dataset and one Model Type:

```text
Faster
More Accurate
```

There are no user-editable Advanced Training Parameters in the first public release.

The application uses versioned qualified training configurations and records the complete effective parameter set in model metadata.

Training always uses:

```text
70% Training
15% Validation
15% Internal Test
Seed 1729
```

Compute device selection is automatic:

```text
Compatible bundled GPU environment available
    → GPU
Otherwise
    → CPU
```

Model Name and Save Location are required before Training starts. When Training completes, the application saves the Model Package automatically and then makes it the global Active Model.

Low performance does not prevent saving or activation when the required artifacts were produced.

The workspace keeps Dataset Summary in a main white region. A separate main white **Results** region sits below it and contains the two approved live plots during Training and the approved completion tables after Training; this placement does not add new data semantics.

## 7.5 Models > Model Test

Model Test remains a first-class workspace.

It uses the authoritative Active Model and accepts one compatible labeled Dataset with the same class count. It has no local Model selector; another Model must first be made Active in Library.

It displays progress, overall accuracy, per-class accuracy, and a confusion matrix, and writes a summary plus per-image predictions CSV.

It does not change Active Model state.

The workspace keeps Dataset Summary in a main white region. A separate main white **Results** region sits below it and contains the approved metrics, confusion matrix, and prediction summaries; this placement does not add new data semantics.

Model Test uses the same automatic compute-device policy as Training:

```text
Compatible bundled GPU inference environment available
    → GPU acceleration
Otherwise
    → CPU fallback
```

GPU availability never blocks Model Test. The actual execution device is displayed and recorded.

## 7.6 Models > Library

Model Library discovers and manages valid OpenDSS v2 model packages.

Required actions remain:

```text
Set Active
Import Model
Export Model
Duplicate Model
Rename Model
Delete Model
```

Import accepts the approved v2 package contract only. It does not convert legacy packages.

One global Active Model is represented through the authoritative model registry.

## 7.7 Sort > Live

Live is one stateful workspace. There is no separate `Sort > Setup` workspace.

### Pre-run state

The workspace shows:

- live camera view on the left;
- run configuration on the right;
- access to the bottom Hardware panel.

Run configuration includes:

- Run Name;
- Experiment Type;
- Notes;
- optional Duration;
- Save Location;
- Active Model when applicable;
- Trigger Every Droplet;
- Hit Class for Class-Based Sorting;
- Hit boundary calibration;
- Record Full Image Sequence;
- Setup Profile actions;
- Send Test Pulse;
- Start Sorting.

The two first-class Trigger Every Droplets are:

```text
Class-Based Sorting
Trigger Every Droplet
```

Class-Based Sorting requires an Active Model. Trigger Every Droplet does not.

The underlying detection, crop, routing algorithm, and internal synchronization/timing values are fixed qualified application configuration. They are not exposed for editing.

DAQ Output Channel and supported pulse/waveform controls are DAQ hardware settings and therefore belong in the shared DAQ panel.

### Start transition

When Start Sorting is selected:

- OpenDSS creates the Run folder and initial structured files;
- the complete effective Run configuration is snapshotted;
- camera and DAQ controls lock;
- the hardware panel closes;
- pre-run configuration controls disappear;
- the right panel changes to the Live Sorting monitor.

### Running state

The left-side live stream remains visible.

The right-side monitor shows:

- Run status and elapsed time;
- Trigger Every Droplet;
- Active Model when present;
- Hit Class when applicable;
- Hit boundary calibration;
- Total Droplets;
- Predicted Class counts when a model is present;
- Decision Hit and Decision Waste;
- Observed Hit, Observed Waste, and Unresolved;
- inference time;
- camera FPS;
- Pause and Stop.

### Pause state

Pause keeps the live camera preview active but stops:

- inference;
- new DAQ output;
- new event finalization.

Camera and DAQ settings remain hidden and locked. Resume continues the same Run.

### Completion or interruption

The workspace shows the result and direct actions:

```text
Open Run Summary
Open Run Folder
Start New Run
```

Starting a new Run returns to the pre-run state with the previous values available for review.

## 7.8 Sort > Sequence Test

Sequence Test is located under Sort, not Models.

It processes a recorded v2 Image Sequence through:

- fixed droplet detection and crop processing;
- optional or required model inference depending on Trigger Every Droplet;
- routing logic;
- visual trajectory tracking;
- optional physical DAQ output;
- Run persistence.

A camera is never required.

Physical DAQ Output is a visible checkbox and is off by default. When enabled, DAQ Ready is required. When disabled, Sequence Test can process and create a Run without DAQ hardware.

Class-Based Sorting requires a model. Trigger Every Droplet may run without one.

Every Sequence Test creates a Run visible under Results.

## 7.9 Results > Runs

Results contains only:

```text
Live Sorting Runs
Sequence Test Runs
```

Training and Model Test history are not added to Results.

A selected Run displays:

- status and timestamps;
- experimental information and notes;
- model and routing snapshot;
- hardware and fixed processing configuration;
- Total Droplets;
- Predicted Class counts;
- Decision counts;
- Observed Hit, Waste, and Unresolved counts;
- Decision-versus-Observed Route matrix;
- links to `events.csv`, saved Droplet Crops, optional sequence, and the Run folder.

Run Notes remain editable. Historical event data remains immutable.

## 7.10 Settings

Settings remains a reduced workspace containing only:

```text
Storage
Application Information
Diagnostics
Visuals
```

It does not duplicate Camera or DAQ controls.

Representative functions are:

- view or change the default data root;
- open the data root in Windows Explorer;
- display application and schema versions;
- display runtime and driver availability;
- open the diagnostic folder;
- set application-wide Text Size from **80%** through **200%**, default **100%**.

**Visuals** contains only Text Size. Settings does not expose any other visual or application preference.

---

## 8. Artifact model

```text
Image Sequence
    ├── sequence.json
    └── numbered full-frame TIFF files

Dataset
    ├── dataset.json
    ├── full-frame sequence
    └── Droplet Crops

Model Package
    ├── metadata.json
    ├── checkpoint.pth
    └── model.onnx

Setup Profile
    └── one ordinary v2 JSON file

Run
    ├── run_summary.json
    ├── events.csv
    ├── Droplet Crops
    └── optional full Image Sequence
```

### 8.1 Relationships

```text
Droplet Dataset Capture
    → Dataset
        → Label
        → Train
            → Model Package
        → Model Test with a compatible Model Package
```

```text
Image Sequence
    → Sequence Viewer
    → Sequence Test
        → Run
```

```text
Model Package
    → Model Library
    → Active Model
    → Model Test
    → Live Sorting
    → Sequence Test
```

```text
Setup Profile
    → Open in Live pre-run configuration
        → Camera/DAQ settings and run selections
            → Run configuration snapshot at Start
```

```text
Run
    → Run Summary
    → Droplet Log
    → Droplet Crops
    → optional Image Sequence
```

---

## 9. Setup Profile model

Setup Profiles are ordinary v2 JSON files.

The required application actions are:

```text
Open Profile
Save Profile
Save Profile As
```

There is no managed profile library and no separate Import, Export, or Delete workflow. Users may copy, rename, move, and delete profile files through Windows Explorer.

A profile may contain:

- Camera settings;
- DAQ settings;
- Active Model reference;
- Trigger Every Droplet;
- Hit Class;
- Hit boundary calibration;
- Record Full Image Sequence preference;
- Run Name;
- default Save Location.

Fixed detector, crop, routing-algorithm, and internal timing values are not user-editable profile fields. Their effective configuration or version is recorded automatically in Dataset and Run provenance.

If a model reference is unavailable:

- all other readable profile values load;
- the missing model is shown factually;
- OpenDSS does not silently substitute another model;
- Class-Based Sorting remains unavailable until a valid model is selected;
- Trigger Every Droplet may proceed without a model.

---

## 10. Scientific event model

Each finalized sorting event records, as applicable:

```text
Detection
Droplet Crop
Predicted Class
Class Scores
Decision
Observed Route
Inference Time
```

### 10.1 Decision values

```text
Hit
Waste
```

### 10.2 Observed Route values

```text
Hit
Waste
Unresolved
```

`Unresolved` is used when the system recorded the event and routing Decision but could not determine the visual route reliably.

An Unresolved observation does not change the earlier Decision or DAQ behavior.

### 10.3 Decision-versus-observation matrix

| Decision | Observed Hit | Observed Waste | Unresolved |
|---|---:|---:|---:|
| Hit | count | count | count |
| Waste | count | count | count |

The interface must not label this matrix as ground truth, actual destination, or routing accuracy.

---

## 11. Editable and fixed configuration

### 11.1 User-editable hardware settings

Only the following technical settings are user-editable:

```text
Camera settings
DAQ settings
```

They apply immediately while valid, available, and idle.

### 11.2 User-selectable run and workflow values

The following remain normal user selections rather than technical tuning parameters:

- Dataset;
- an initial two-class or three-class schema only for a Dataset that has not yet been configured;
- Class Names and labels;
- Model Type: Faster or More Accurate;
- Model and Active Model;
- Trigger Every Droplet;
- Hit Class;
- Hit boundary calibration;
- Physical DAQ Output for Sequence Test;
- Record Full Image Sequence;
- names, notes, Duration, and Save Location.
- application-wide Text Size from 80% through 200%.

### 11.3 Fixed qualified application configuration

The following are not user-editable in the first release:

- droplet-detection parameters;
- Droplet Crop parameters beyond the fixed artifact contract;
- routing-algorithm parameters;
- confidence or score thresholds;
- internal tracking and synchronization timing;
- training hyperparameters;
- training split and seed;
- manual CPU/GPU selection.

The effective values or a versioned configuration identifier must still be recorded where needed for reproducibility.

---

## 12. Hardware and compute dependencies

| Operation | Camera | DAQ | Model | GPU |
|---|---:|---:|---:|---:|
| Single Image | Required | No | No | No |
| Image Sequence | Required | No | No | No |
| Droplet Dataset Capture | Required | No | No | No |
| Label | No | No | No | No |
| Train | No | No | No | Optional automatic acceleration |
| Model Test | No | No | Required | Optional automatic acceleration; CPU fallback |
| Model Library | No | No | No | No |
| Sequence Viewer | No | No | No | No |
| Live — Class-Based | Required | Required | Required | No; qualified CPU inference |
| Live — Trigger Every Droplet | Required | Required | Optional | No; qualified CPU inference when present |
| Sequence Test with DAQ output | No | Required | Mode-dependent | Not required |
| Sequence Test without DAQ output | No | No | Mode-dependent | Not required |
| Results | No | No | No | No |

### 12.1 Physical-output model

No separate software arming state is added.

- Send Test Pulse requires DAQ Ready.
- Live Sorting requires the factual technical prerequisites for its selected Trigger Every Droplet.
- Sequence Test starts with Physical DAQ Output enabled and requires DAQ Ready unless the user disables output.
- OpenDSS Stop and fault handling are not safety-rated Emergency Stop functions and do not replace required physical safety controls.

---

## 13. Global operation model

Only one long-running operation may own mutable resources at a time:

```text
Image Sequence recording
Droplet Dataset Capture
Training
Model Test
Sequence Test
Live Sorting
```

Single Image is momentary but is blocked while another operation owns the camera or storage pipeline.

Passive navigation is permitted when it cannot alter the active operation.

Applicable lifecycle states are:

```text
Idle
Starting
Running
Paused
Stopping
Completed
Interrupted
Failed
```

Every long-running operation provides Stop. Duration, where present, is optional and blank means continue until Stop.

At operation start, OpenDSS captures the configuration needed to reproduce the operation. Later UI changes do not alter an active Run or capture.

---

## 14. Fault and recovery communication

Keep fault communication simple and contextual.

### 14.1 Missing prerequisite

Show a direct reason beside or within the disabled action:

```text
Camera unavailable
DAQ unavailable
No active model
No dataset selected
Output folder is not writable
Another operation is active
```

### 14.2 Interrupted or failed operation

Show one persistent workspace banner that states:

- what stopped;
- the direct reason when known;
- whether partial data was preserved;
- direct recovery actions.

Example:

```text
Droplet Dataset Capture interrupted
The camera disconnected. Existing frames and crops were preserved.

[Open Dataset] [Open Folder]
```

### 14.3 Recovery

On launch, OpenDSS may discover incomplete v2 operation folders and expose direct open/recovery actions in the relevant workspace.

There is no notification center and no repeated modal-dialog fault workflow.

Hardware faults stop new impossible or unsafe output before the UI message is presented.

---

## 15. Persistence and provenance

Canonical files remain:

| Artifact | Canonical file |
|---|---|
| Image Sequence | `sequence.json` |
| Dataset | `dataset.json` |
| Model Package | `metadata.json` |
| Run Summary | `run_summary.json` |
| Droplet Log | `events.csv` |

Structured files use schema versions and should be written atomically.

Live and Sequence Test event persistence uses background queues so camera processing and inference are not blocked by disk I/O.

Run counts are derived from finalized events, not by repeatedly reading the CSV from the GUI.

Run Summary and Dataset metadata preserve the effective fixed processing configuration even though those settings are not user-editable.

---

## 16. Legacy artifact strategy

OpenDSS v2 provides no product-level backward compatibility.

It does not provide:

- legacy loaders;
- automatic conversion;
- migration screens;
- read-only legacy mode;
- compatibility branches in normal domain services.

Existing laboratory artifacts may be converted during engineering work into validated v2 artifacts. Those conversion scripts or manual procedures are internal development tools, not public product features.

When a user selects an unsupported artifact, OpenDSS reports that it is not a supported OpenDSS v2 schema and does not partially interpret it.

---

## 17. First-release boundaries

The first release includes:

- Single Image, Image Sequence, and Droplet Dataset Capture as first-class collapsible sections in one Capture workspace;
- two-class and three-class Datasets and Models;
- Faster and More Accurate training configurations;
- optional GPU acceleration for Training and Model Test;
- Model Library and automatic activation after successful model save;
- Class-Based Sorting and Trigger Every Droplet;
- Live Sorting and Sequence Test Runs;
- file-based Setup Profiles;
- simple Run review and direct file access.

The first release does not include:

- more than three classes;
- arbitrary model architectures;
- editable training hyperparameters;
- editable detector, crop, routing-algorithm, or internal timing parameters;
- confidence-threshold routing;
- multiple simultaneous cameras or sort paths;
- cloud, accounts, collaboration, or remote control;
- external arbitrary image import into Datasets;
- Dataset merging;
- integrated per-event Run browser;
- first-class charts;
- Training or Model Test history under Results;
- a Home screen;
- a separate Sort Setup workspace;
- a managed Setup Profile library;
- legacy compatibility or migration UI;
- telemetry or automatic updates;
- software Emergency Stop claims;
- placeholder controls for deferred features.

---

## 18. Product-state ownership boundary

The next architecture phase must preserve one authoritative owner for each domain state.

A suitable ownership boundary is:

| State or responsibility | Authoritative owner |
|---|---|
| Camera status and settings | HardwareCoordinator |
| DAQ status and settings | HardwareCoordinator |
| Active Model and model packages | ModelRegistry |
| Current Dataset and label persistence | DatasetService |
| Current Sequence and sequence loading | SequenceService |
| Current long-running operation and resource locks | OperationCoordinator |
| Training execution | TrainingService |
| Run discovery and persisted Run data | RunRepository |
| Storage and application-wide Text Size preferences | SettingsRepository |
| Global header snapshot | AppStateStore as a projection of domain owners, not a second authority |

Individual workspaces must not duplicate authoritative state.

This ownership table is an architecture constraint for the next phase, not a requirement to use these exact class names.

---

## 19. Approved decision register

| ID | Decision | Approved choice | Principal effect |
|---|---|---|---|
| D-001 | Primary navigation | Domain navigation: Data / Models / Sort / Results / Settings | Retains domain-oriented top level |
| D-002 | Physical DAQ arming | No separate software arming | Technical readiness remains the software prerequisite model |
| D-003 | Post-training activation | Saved model automatically becomes Active | Retains automatic global activation |
| D-004 | Model Test compute device | Optional GPU using Training's automatic policy; CPU fallback | GPU never blocks Model Test |
| D-005 | Observed Route | Add Unresolved | Prevents forced unsupported Hit/Waste observations |
| D-006 | Sequence Test location | `Sort > Sequence Test` | Groups it with routing and optional physical output |
| D-007 | Setup and Live | One Live workspace with pre-run and running states | Removes separate Sort Setup workspace |
| D-008 | Trigger Every Droplet | First-class Trigger Every Droplet | Retained in Live and Sequence Test |
| D-009 | Setup Profiles | Ordinary files: Open, Save, Save As | No managed profile library |
| D-010 | Legacy artifacts | Unsupported by product; convert during engineering work | V2-only public loaders |
| D-011 | Model Test placement | First-class `Models > Model Test` workspace | Retains dedicated test operation |
| D-012 | Supported classes | Two and three classes, selected only during initial Dataset class setup and immutable once configured | Retains both scientific workflows without class-count switching or migration |
| D-013 | Results scope | Runs only | Live Sorting and Sequence Test only |
| D-014 | Capture composition | One shared live Camera preview with three fixed, independently collapsible right-panel sections; all headings always visible, none expanded by default, expanded bodies sharing space and scrolling independently, and active/result visibility preserved | Shared Capture layout |
| D-015 | Advanced Training Parameters | Not editable | Qualified fixed configurations only |
| D-016 | Settings application | Camera and DAQ only; immediate while available and idle | Shared shell-level hardware panel |
| D-017 | Settings workspace | Retain reduced Settings | Storage, Application Information, Diagnostics, and Visuals; Visuals contains only application-wide Text Size from 80% through 200%, default 100%, owned by SettingsRepository preference state |
| D-018 | Startup | Open Data > Capture every launch with all three Capture sections collapsed; no last-workspace memory | Fixed Capture workspace startup |
| D-019 | Fault communication | Simple contextual communication | Disabled reason, one banner, direct recovery actions |

---

## 20. Required amendments to the original workflow specification

The later specification revision must, at minimum:

1. Move Sequence Test from Models to Sort.
2. Remove the separate Sort Setup workspace and merge its user-facing behavior into Live's pre-run state.
3. Add Observed Route = Unresolved to terminology, events, counters, matrices, CSV, Run Summary, and acceptance criteria.
4. Add optional automatic GPU acceleration with CPU fallback to Model Test.
5. Present Single Image, Image Sequence, and Droplet Dataset Capture as three independently collapsible right-panel sections beside one shared live Camera preview; keep every heading visible and start with all sections collapsed.
6. Add the shell-level slide-out Camera/DAQ panel and remove duplicated hardware settings from individual workspaces.
7. Remove user-editable detection, crop, routing-algorithm, timing, and Advanced Training controls.
8. Retain fixed effective configuration in Dataset, Model, and Run provenance.
9. Replace managed Setup Profile actions with Open, Save, and Save As for ordinary v2 files.
10. Narrow Setup Profile content to hardware settings and run selections.
11. Remove product-level legacy compatibility and migration requirements.
12. Retain Results for Runs only.
13. Retain a reduced Settings workspace for Storage, Application Information, Diagnostics, and Visuals; Visuals contains only application-wide Text Size from 80% through 200%, default 100%.
14. Define startup as `Data > Capture` on every launch, with all three Capture sections collapsed.
15. Use simple contextual fault and recovery communication.
16. Update all navigation paths, acceptance scenarios, and service responsibilities affected by these decisions.

---

## 21. Next design phase

The next approved task is to produce the OpenDSS v2 information architecture and complete screen/workspace inventory based on this product model.

That phase should define:

- the final shell structure;
- primary and secondary navigation;
- every workspace and mode;
- the right-panel transitions in Capture and Live;
- the shared hardware panel behavior;
- contextual links between artifacts and workflows;
- required screen states;
- operation ownership and lock behavior;
- which requirements from the original specification map to each workspace.

It should not reopen D-001 through D-019, add unapproved features, write production code, or begin polished visual styling.

---

## Source basis

1. *OpenDSS Detailed User Workflow Specification*, Version 1.0, July 21, 2026.
2. Approved OpenDSS v2 decision workshop, decisions D-001 through D-019.
3. OpenDSS_clean repository, used only as implementation evidence and a reusable-component source.
