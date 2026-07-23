# Open Droplet Sorting Suite (OpenDSS)
## Detailed User Workflow Specification

**Document ID:** ODSS-WF-001  
**Version:** 1.1  
**Status:** Product workflow baseline  
**Date:** July 23, 2026  
**Primary platform:** Windows 11  
**Product name:** Open Droplet Sorting Suite  
**Product abbreviation:** OpenDSS  

---

## 1. Purpose

This document defines the user-facing workflows, shared application behavior, data handoffs, saved artifacts, blocking conditions, and acceptance criteria for Open Droplet Sorting Suite (OpenDSS).

It is intended to serve as the source material for:

- a software requirements specification;
- a GUI and interaction specification;
- application-layer reconstruction;
- implementation work items;
- validation and acceptance testing;
- publication methods and supplementary software documentation.

This document is normative for the first public release unless a later approved specification supersedes it.

---

## 1.1 July 23, 2026 workflow amendment

The Approved v2 Product Model remains controlling. The July 23, 2026 UI/UX Design Amendment controls every workflow matter it explicitly changes. The following flows replace conflicting older flow fragments in this document.

### Camera startup

```text
Launch maximized at Data > Capture
→ attempt Camera connection
→ Camera available: continue
→ Camera unavailable: ask "Camera unavailable. Continue?" once
   → Yes: continue without Camera
   → No: close OpenDSS
```

`Start Camera` starts or stops streaming; connection and streaming remain distinct.

### Train

```text
Dataset
→ Faster or More Accurate
→ Model Name
→ Save Location
→ Start Training
→ Training completes
→ Model Package saves automatically
→ Model becomes Active
```

During Training, show progress, elapsed and remaining timing, automatic device, a Training/Validation Loss plot, and a Validation Accuracy plot. After success, show overall and per-class results, a confusion matrix when generated, the saved path, and Active Model confirmation. If final save fails, show `Error`, retain temporary artifacts, enable Retry Save, and do not activate the Model.

### Active Model use

Model Test and Sequence Test always use the Active Model. To use another Model, the user first selects it in Library and chooses Set Active. The Active Model is locked against replacement or mutation while Model Test, Live, or Sequence Test uses it.

### Live

```text
Review Setup Profile, Run Information, Trigger & Timing, and Output & Recording
→ Start Camera as needed
→ Start Sorting
→ collapse setup sections and expand Running
→ process, log, and persist one Run
→ Pause/Resume or Stop
→ review completion in the same workspace or open Results
```

Trigger Every Droplet and DAQ Output are independent toggles. DAQ Output OFF still permits all nonphysical processing and Run persistence. DAQ Output ON requires DAQ Ready. Hit boundary calibration affects Observed Route only and persists `boundary_y`, `hit_side`, `image_width`, and `image_height` in the Setup Profile and Run Summary.

### Sequence Test

```text
Load valid OpenDSS Image Sequence folder containing sequence.json
→ inspect first-frame preview, frame count, recorded FPS, and status
→ set Processing FPS (defaults to recorded FPS)
→ inspect available memory and choose/calculated bounded buffer
→ Load to Memory
→ Start after successful allocation
→ refill buffer while processing when necessary
→ Stop or complete
→ save Processing FPS and actual achieved FPS in Run Summary
```

Physical DAQ Output is a checkbox that is off by default. When off, Sequence Test processes and creates a Run without DAQ hardware. Allocation failure shows `Error`, logs details, cancels loading, and leaves Start disabled.

### Results

Selecting a Run in the right-side Runs panel changes selection only. Pressing Load replaces the loaded center detail. The detail contains identity, operation type, status, key counts, Predicted Class, Decision, Observed Route, their decision-versus-observation matrix, Notes, file actions, and collapsed provenance. It does not become a chart, event-browser, Training-history, or Model-Test-history workspace.

## 2. Product objective

OpenDSS is a scientific desktop application for laboratories that perform image-based droplet analysis and physical droplet sorting.

The product supports two equally important workflows:

```text
MODEL DEVELOPMENT

Droplet Dataset Capture
    → Label
    → Train
    → optional Model Test
    → Active Model
```

```text
PHYSICAL SORTING

Sort Setup
    → Live Sorting
    → Droplet Log
    → Run Summary
```

The intended primary user is a biomedical or laboratory scientist who may not be a software, machine-learning, camera, or data-acquisition engineer.

The first public release supports the exact experimental workflow qualified for the associated publication. It is not intended to be a general-purpose machine-learning studio or a universal hardware-control framework.

---

## 3. Normative language

The following terms have specific meanings in this document:

- **SHALL**: mandatory behavior.
- **SHALL NOT**: prohibited behavior.
- **SHOULD**: recommended behavior that may be changed for a documented technical reason.
- **MAY**: optional implementation behavior.
- **User**: the scientist or laboratory operator using OpenDSS.
- **System**: the OpenDSS desktop application and its bundled runtime components.

---

## 4. Governing product principles

### PR-001 — User-directed operation

The system SHALL execute the operation selected by the user. It SHALL make the minimum number of scientific and experimental decisions necessary to perform that operation.

### PR-002 — No scientific approval layer

The system SHALL NOT approve, reject, promote, certify, recommend, or score a dataset, model, run, or experimental design as scientifically acceptable.

The system MAY display factual measurements such as:

- image counts;
- class counts;
- training loss;
- validation accuracy;
- per-class accuracy;
- macro F1;
- confusion matrices;
- class scores;
- decision counts;
- observed-route counts;
- inference time;
- camera frame rate.

The user determines whether those measurements are acceptable.

### PR-003 — Technical blocking only

The system MAY block an operation only when the requested operation is technically impossible or incompatible with its defined file or hardware contract.

Examples include:

- the camera cannot stream;
- the DAQ is unavailable when physical output is required;
- a selected file cannot be parsed;
- a required image is missing;
- a model package cannot be opened;
- a model and dataset have different class counts during Model Test;
- the output folder is not writable;
- another mutually exclusive operation is already active.

The system SHALL NOT block an operation because of:

- class imbalance;
- low model accuracy;
- model collapse;
- use of the same dataset for training and Model Test;
- a user-selected Hit Class;
- a user-selected Hit boundary calibration;
- an experimental choice the system considers unusual.

### PR-004 — Factual terminology

The system SHALL keep the following concepts separate:

```text
Predicted Class
    → Decision
    → Observed Route
```

A model prediction is not a physical routing observation. A physical routing observation is not a model label.

### PR-005 — Offline operation

After installation, the application SHALL operate without an internet connection.

The application SHALL NOT:

- collect telemetry;
- upload diagnostics;
- perform update checks;
- contact external services;
- require a cloud account;
- require a network connection for normal use.

### PR-006 — Reproducibility without project-management overhead

The system SHALL record the settings and software information required to reconstruct how a dataset, model, or run was produced.

The system SHALL NOT require the user to create projects, accounts, dataset versions, approval states, or workflow tickets.

### PR-007 — Normal file ownership

OpenDSS files SHALL remain ordinary Windows files and folders that the user can inspect, copy, move, back up, and select through the standard Windows file picker.

---

## 5. Supported user and operating environment

### 5.1 User model

The first release SHALL assume:

- one local user at a time;
- no authentication;
- no user roles;
- no permission tiers;
- no shared cloud workspace;
- no collaboration server;
- no project hierarchy.

Advanced controls MAY be placed under an **Advanced** section, but they SHALL NOT require a separate technician login.

### 5.2 Supported platform

The first public release SHALL support Windows 11.

### 5.3 Supported laboratory dependency categories

The qualified installation uses:

- a supported Hamamatsu camera;
- Hamamatsu DCAM runtime/driver;
- supported National Instruments DAQ hardware;
- NI-DAQmx;
- an optional compatible NVIDIA GPU and NVIDIA display driver for accelerated training.

Specific qualified hardware models and driver versions are maintained separately from this workflow specification.

### 5.4 Installer boundary

The full offline installer SHALL include:

- OpenDSS;
- Qt and application runtime dependencies;
- the application-owned Python runtime;
- the CPU training environment;
- GPU training packages;
- CUDA runtime components required by the bundled training packages;
- training scripts;
- bundled base model weights;
- required application resources.

The installer SHALL NOT bundle:

- Hamamatsu DCAM;
- NI-DAQmx;
- the NVIDIA display driver.

Those remain external vendor prerequisites.

### 5.5 Updates and signing

- Automatic updates SHALL be disabled.
- Update checks SHALL not occur.
- The public installer and executable SHALL be code signed.
- The distribution SHALL retain the repository’s Apache License 2.0 licensing unless separately changed.

---

## 6. Controlled user-facing terminology

### 6.1 Product name

The public product name is:

> **Open Droplet Sorting Suite (OpenDSS)**

The technical identifier MAY remain:

```text
OpenDropletSortingSuite
```

### 6.2 Canonical terms

| Term | Definition |
|---|---|
| **Frame** | One full camera image. |
| **Image Sequence** | An ordered series of full-frame TIFF images. |
| **Droplet Crop** | One saved 64 × 64 PNG representing the first complete appearance of one detected droplet. |
| **Dataset** | The result of Droplet Dataset Capture, consisting of a full image sequence, Droplet Crops, class definitions, labels, and `dataset.json`. |
| **Class ID** | Immutable numerical class identifier: `0`, `1`, or `2`. |
| **Class Name** | User-editable text describing a Class ID. |
| **Label** | A Class ID assigned by the user to one Droplet Crop. |
| **Predicted Class** | The Class ID with the largest model output score. |
| **Class Score** | Raw numerical output emitted by the model for one class. |
| **Hit Class** | The user-selected Predicted Class that produces a Hit Decision in Class-Based Sorting. |
| **Decision** | The system’s Hit or Waste routing action. |
| **Hit boundary calibration** | The user-selected image y-axis direction corresponding to the physical Hit outlet. |
| **Observed Route** | Hit or Waste, derived from visually tracked y-axis trajectory. |
| **DAQ Output Channel** | National Instruments output channel used to issue the sorting signal. |
| **Inference Time** | Time used to execute model inference for one Droplet Crop. |
| **Setup Profile** | Importable and exportable configuration used for capture or sorting setup. |
| **Run** | One Live Sorting or Sequence Test operation. |
| **Run Summary** | Structured run-level information rendered from `run_summary.json`. |
| **Droplet Log** | Per-droplet CSV record stored as `events.csv`. |

### 6.3 Terms prohibited in the normal interface

The normal interface SHALL NOT use the following terms as substitutes for the approved terminology:

| Avoid | Use |
|---|---|
| Target Class | Hit Class, when routing is meant |
| Positive Class / Negative Class | Class 0, Class 1, Class 2 |
| Hit Channel | Hit boundary calibration or DAQ Output Channel |
| Actual Destination | Observed Route |
| Confidence | Class Score |
| Logits | Class Score |
| Promotion | Automatic Save / Set Active |
| Approved Model | Active Model |
| Manifest | Dataset File |
| Diagnostic Mode | No named mode |
| No-Camera Mode | No named mode |
| Report Artifact | Run Summary, Droplet Log, Droplet Crop, or Image Sequence |
| Reject Candidate | Not used |
| Class Balance Warning | Not used |

---

## 7. Primary navigation

The application SHALL use the following primary navigation structure:

```text
Data
├── Capture
├── Label
└── Sequence Viewer

Models
├── Train
├── Model Test
├── Sequence Test
└── Library

Sort
├── Setup
└── Live

Results
└── Runs

Settings
```

### NAV-001 — No separate Reports workspace

The system SHALL NOT expose a separate **Reports** workspace.

A selected Run SHALL display its Run Summary, Droplet Log, saved Droplet Crops, settings, notes, and optional Image Sequence inside **Results > Runs**.

### NAV-002 — Persistent workspaces

The product SHALL use persistent workspaces rather than a mandatory step-by-step wizard.

The system MAY provide contextual links such as **Open in Label** or **Use in Train**, but the user SHALL remain free to open workspaces directly.

---

## 8. Global application shell

### 8.1 Global status areas

A compact global header SHALL show four status areas:

```text
Camera
DAQ
Active Model
Current Activity
```

### 8.2 Camera status values

```text
Unavailable
Connected
Streaming
```

### 8.3 DAQ status values

```text
Unavailable
Ready
Active
```

### 8.4 Active Model status

```text
No Active Model
<Model Name>
```

### 8.5 Current Activity values

The application SHALL display one current activity, selected from the applicable set:

```text
Idle
Capturing Image
Recording Sequence
Droplet Dataset Capture
Labeling
Training
Testing Model
Testing Sequence
Sorting
Paused
```

### 8.6 Disabled controls

The application SHALL disable an action that cannot run.

It SHALL NOT require a separate readiness checklist panel.

A disabled control SHOULD expose one direct reason through a tooltip or adjacent text, such as:

```text
Camera unavailable
DAQ unavailable
No active model
No dataset selected
No sequence selected
Output folder is not writable
Another operation is active
```

### 8.7 Camera-unavailable display

When a camera-dependent workspace is opened without a camera, the preview region SHALL remain visible and display:

> **Camera unavailable**

Hardware-dependent controls SHALL be disabled.

The application SHALL NOT rename this condition as Diagnostic Mode or No-Camera Mode.

---

## 9. Global operation and concurrency rules

### OPS-001 — One active long-running operation

Only one of the following operations SHALL run at a time:

- Image Sequence recording;
- Droplet Dataset Capture;
- Training;
- Model Test;
- Sequence Test;
- Live Sorting.

Single Image capture is momentary but SHALL still be blocked while another operation owns the camera or storage pipeline.

### OPS-002 — Passive navigation

The user MAY navigate to read-only workspaces while an operation is active when doing so does not interfere with that operation.

The system SHALL prevent:

- starting another long-running operation;
- modifying a dataset currently used by Training or Model Test;
- deleting or replacing an active model used by an operation;
- changing hardware settings used by an active capture, Sequence Test, or Live Sorting operation.

### OPS-003 — Operation lifecycle

Long-running operations SHALL support the applicable states:

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

Not every workflow needs every state. For example, Model Test MAY omit Paused.

### OPS-004 — Stop ownership

The user SHALL be able to stop every long-running operation.

Where a Duration is specified, the operation SHALL also stop automatically when that duration expires.

---

## 10. Default user-data storage

### 10.1 Default root

The installer SHALL create:

```text
%USERPROFILE%\Documents\OpenDropletSortingSuite
```

Recommended subfolders are:

```text
OpenDropletSortingSuite\
├── Images\
├── Sequences\
├── Datasets\
├── Models\
├── ModelTests\
├── Runs\
└── Profiles\
```

### 10.2 User-selected locations

Every workflow that saves output SHALL use the default location unless the user selects another location through the standard Windows file picker.

The system SHALL accept valid writable locations outside the default root.

### 10.3 Timestamp fallback

When an optional object name is blank, the system SHALL use a local timestamp formatted consistently, for example:

```text
2026-07-21_15-42-18
```

### 10.4 Uninstall behavior

The uninstaller SHALL ask whether user data should be retained or removed.

User data SHALL NOT be silently deleted.

---

# PART II — DATA WORKFLOWS

## 11. Workflow DATA-01: Capture a Single Image

### 11.1 Purpose

Capture one full camera image and save it as TIFF.

### 11.2 Entry point

```text
Data > Capture > Single Image
```

### 11.3 Preconditions

| ID | Requirement |
|---|---|
| CAP-IMG-001 | The camera SHALL be connected and capable of streaming. |
| CAP-IMG-002 | A writable Save Location SHALL be available. |
| CAP-IMG-003 | No mutually exclusive operation SHALL be active. |

### 11.4 User inputs

| Field | Required | Default |
|---|---:|---|
| File Name | No | Timestamp |
| Save Location | No | `Documents\OpenDropletSortingSuite\Images` |

No Duration field is required for Single Image.

### 11.5 Primary flow

1. The user opens **Single Image**.
2. The application displays the live camera preview.
3. The user optionally changes File Name or Save Location.
4. The user selects **Capture Image**.
5. The system captures one full frame.
6. The system writes one TIFF file.
7. The system confirms the saved path without opening an additional workflow.

### 11.6 Output

```text
Images\
└── <name-or-timestamp>.tif
```

### 11.7 Alternate and error flows

- If the camera becomes unavailable before capture, **Capture Image** SHALL be disabled.
- If writing fails, the system SHALL show a direct file-write error and SHALL NOT claim that the image was saved.
- The application SHALL NOT create dataset metadata, run metadata, labels, classifications, or DAQ output.

### 11.8 Acceptance criteria

- One action creates exactly one TIFF.
- No model or DAQ is required.
- A blank name produces a timestamp filename.
- The file can be opened by standard image software.

---

## 12. Workflow DATA-02: Capture an Image Sequence

### 12.1 Purpose

Record a full-frame camera sequence as individually numbered TIFF files.

### 12.2 Entry point

```text
Data > Capture > Image Sequence
```

### 12.3 Preconditions

| ID | Requirement |
|---|---|
| CAP-SEQ-001 | The camera SHALL be connected and streaming. |
| CAP-SEQ-002 | The output folder SHALL be writable. |
| CAP-SEQ-003 | No mutually exclusive operation SHALL be active. |

### 12.4 User inputs

| Field | Required | Default |
|---|---:|---|
| Name | No | Timestamp |
| Experiment Type | No | Empty |
| Notes | No | Empty |
| Duration | No | Empty; continue until Stop |
| Save Location | No | `Documents\OpenDropletSortingSuite\Sequences` |

Duration SHALL accept an empty value.

### 12.5 Duration behavior

```text
Duration blank
    → recording continues until the user selects Stop

Duration specified
    → recording stops when the duration expires
    → the user may still stop earlier
```

An unspecified duration SHALL be stored as `null`, not `0`.

### 12.6 Primary flow

1. The user opens **Image Sequence**.
2. The live camera preview is displayed.
3. The user optionally enters capture information.
4. The user selects **Start Recording**.
5. The system creates a sequence folder.
6. Frames are written continuously as numbered TIFF files.
7. The user may:
   - select **Pause**;
   - select **Resume**;
   - select **Stop**;
   - allow the optional Duration to expire.
8. On completion, the system finalizes `sequence.json`.
9. The system shows the sequence location.

### 12.7 Pause behavior

While paused:

- the live camera preview SHALL continue;
- TIFF writing SHALL stop;
- the elapsed active-recording timer SHALL stop or clearly distinguish paused time;
- Resume SHALL continue the same sequence;
- a second sequence SHALL NOT be created.

### 12.8 Output structure

```text
Sequences\
└── <name-or-timestamp>\
    ├── sequence.json
    └── frames\
        ├── frame_00000001.tif
        ├── frame_00000002.tif
        └── ...
```

### 12.9 Sequence metadata

`sequence.json` SHALL record at least:

- schema version;
- sequence identifier;
- display name;
- Experiment Type;
- Notes;
- OpenDSS version;
- start and end timestamps;
- requested Duration or `null`;
- stop reason;
- frame count;
- frame filename convention or frame entries;
- frame timestamps sufficient for ordered playback;
- camera settings;
- image dimensions;
- camera-dependent pixel type or bit depth;
- acquisition frame rate information;
- sequence status.

### 12.10 Prohibited behavior

Image Sequence capture SHALL NOT:

- run droplet detection;
- create Droplet Crops;
- classify frames;
- require an Active Model;
- issue DAQ output;
- create `dataset.json`;
- appear under Results > Runs.

### 12.11 Acceptance criteria

- TIFF files are individually readable.
- Frame numbering preserves acquisition order.
- Sequence Viewer can open the completed sequence.
- Manual Stop and timed completion both finalize the sequence.
- A blank Duration records until Stop.
- Pausing does not terminate or split the sequence.

---

## 13. Workflow DATA-03: Capture a Dataset

### 13.1 Purpose

Record a full image sequence while detecting droplets and saving one Droplet Crop for each detected droplet.

### 13.2 Entry point

```text
Data > Capture > Droplet Dataset Capture
```

### 13.3 Product contract

Droplet Dataset Capture is classifier-independent.

It SHALL NOT require or use an Active Model to decide whether a crop is saved.

It SHALL NOT assign labels.

It SHALL NOT assign Hit or Waste meanings.

It SHALL NOT reject candidates.

### 13.4 Preconditions

| ID | Requirement |
|---|---|
| CAP-DATA-001 | The camera SHALL be connected and streaming. |
| CAP-DATA-002 | Droplet detection settings SHALL be loadable. |
| CAP-DATA-003 | The output folder SHALL be writable. |
| CAP-DATA-004 | No mutually exclusive operation SHALL be active. |

A model and DAQ SHALL NOT be prerequisites.

### 13.5 User inputs

| Field | Required | Default |
|---|---:|---|
| Dataset Name | No | Timestamp |
| Experiment Type | No | Empty |
| Notes | No | Empty |
| Duration | No | Empty; continue until Stop |
| Save Location | No | `Documents\OpenDropletSortingSuite\Datasets` |
| Camera settings | User-controlled | Current settings |
| Droplet-detection settings | User-controlled | Current settings |
| Droplet Crop settings | User-controlled within supported contract | Current settings |

### 13.6 Detection and crop rule

For each detected droplet:

1. The existing droplet-detection algorithm identifies the first frame in which the droplet is fully present in the frame.
2. The system creates exactly one Droplet Crop.
3. The crop is converted to the supported dataset format:
   - 64 × 64 pixels;
   - grayscale;
   - PNG.
4. The system records the source frame index and detection timestamp.
5. The crop begins in the `unlabeled` state.

The system SHALL NOT save adjacent-frame crops for the same droplet.

### 13.7 Primary flow

1. The user opens **Droplet Dataset Capture**.
2. The live camera preview is displayed.
3. The user optionally enters Dataset Name, Experiment Type, Notes, Duration, or Save Location.
4. The user selects **Start Droplet Dataset Capture**.
5. The system creates a dataset folder and begins:
   - writing full frames as numbered TIFF files;
   - running droplet detection;
   - writing one Droplet Crop per detected droplet;
   - collecting metadata in memory or recoverable temporary storage.
6. The interface displays at minimum:
   - live camera image;
   - elapsed time;
   - full-frame count;
   - detected-droplet count;
   - saved Droplet Crop count.
7. The user may Pause, Resume, or Stop.
8. If Duration is set, the system stops automatically when the Duration expires.
9. On stop or timed completion, the system finalizes `dataset.json`.
10. The completed Dataset becomes available in **Data > Label** and **Models > Train**.

### 13.8 Pause behavior

While paused:

- the camera preview SHALL continue;
- full-frame TIFF recording SHALL stop;
- droplet detection SHALL stop;
- Droplet Crop creation SHALL stop;
- Resume SHALL continue the same Droplet Dataset Capture.

### 13.9 Output structure

```text
Datasets\
└── <name-or-timestamp>\
    ├── dataset.json
    ├── sequence\
    │   ├── frame_00000001.tif
    │   ├── frame_00000002.tif
    │   └── ...
    └── crops\
        ├── droplet_000001.png
        ├── droplet_000002.png
        └── ...
```

### 13.10 Dataset metadata

`dataset.json` SHALL record:

- schema version;
- dataset ID;
- Dataset Name;
- Experiment Type;
- Notes;
- OpenDSS version;
- creation and update timestamps;
- capture start/end timestamps;
- requested Duration or `null`;
- stop reason;
- sequence folder and frame information;
- camera settings;
- droplet-detection settings;
- Droplet Crop settings;
- program settings that affect acquisition, detection, and cropping;
- class definitions, initially empty;
- one entry per Droplet Crop;
- image-state counts;
- label counts after labeling begins.

Each crop entry SHALL contain at least:

- crop ID;
- relative crop path;
- source frame index;
- detection timestamp;
- label ID or `null`;
- state.

### 13.11 Crop states at creation

Every new Droplet Crop SHALL initially have:

```text
state = unlabeled
label_id = null
```

### 13.12 One capture equals one dataset

One Droplet Dataset Capture operation SHALL create one Dataset.

The first release SHALL NOT merge multiple Droplet Dataset Captures into one Dataset through the GUI.

### 13.13 No external-image import

The first release SHALL NOT allow arbitrary external images to be added to a Dataset.

The authoritative entry point for a Dataset is an OpenDSS-generated `dataset.json`.

### 13.14 Interrupted capture

If Droplet Dataset Capture is interrupted by a technical fault, the system SHOULD preserve frames and crops already written.

When technically possible, it SHOULD finalize a recoverable `dataset.json` with an interrupted status rather than discard the data.

### 13.15 Acceptance criteria

- Droplet Dataset Capture runs without a model.
- Every completed detection produces one and only one Droplet Crop.
- No crop is automatically labeled.
- No crop is rejected by application policy.
- A full TIFF sequence and 64 × 64 PNG crops are preserved.
- `dataset.json` is created when the operation ends.
- The Dataset can be opened by Label, Train, and Model Test through the same dataset-loading contract.

---

## 14. Workflow DATA-04: Label a Dataset

### 14.1 Purpose

Allow the user to define two or three classes and assign Class IDs to Droplet Crops.

### 14.2 Entry point

```text
Data > Label
```

### 14.3 Preconditions

| ID | Requirement |
|---|---|
| LABEL-001 | A readable OpenDSS `dataset.json` SHALL be selected. |
| LABEL-002 | Referenced Droplet Crop files required for display SHALL exist and be readable. |
| LABEL-003 | The Dataset SHALL NOT be locked by Training or Model Test. |

A camera, DAQ, or model SHALL NOT be required.

### 14.4 Dataset selection

The user SHALL select a Dataset through:

- the standard Windows file picker;
- a recent-Datasets list, if implemented;
- a contextual **Open in Label** action after Droplet Dataset Capture.

The same dataset-loading implementation SHALL be used by Label, Train, and Model Test.

### 14.5 Class definition

When labeling begins for a Dataset without classes, the user SHALL select:

```text
2 Classes
or
3 Classes
```

The system SHALL create immutable Class IDs:

```text
2-class Dataset: 0, 1
3-class Dataset: 0, 1, 2
```

Default Class Names MAY be:

```text
Class 0
Class 1
Class 2
```

The user SHALL be able to edit Class Names.

Class Names SHALL be stored in `dataset.json`.

Class IDs SHALL never be renamed, reordered, or reassigned.

### 14.6 Class-count stability

After labels have been assigned, the Dataset’s Number of Classes SHALL remain fixed in the normal workflow.

This prevents existing labels from becoming undefined.

### 14.7 Required labeling actions

The Label workspace SHALL support:

- assign Class 0, Class 1, or Class 2;
- Skip;
- Remove from Dataset;
- Undo;
- relabel an already labeled crop;
- bulk-label selected crops;
- filter by class;
- filter Unlabeled;
- filter Skipped;
- filter Removed;
- review Skipped crops;
- restore a Removed crop to the Dataset.

The workspace SHALL NOT require per-crop notes.

### 14.8 State definitions

| State | Meaning | Training eligible |
|---|---|---:|
| **Unlabeled** | No Class ID assigned and not explicitly skipped | No |
| **Labeled** | A Class ID is assigned | Yes |
| **Skipped** | User deferred the crop for later review | No |
| **Removed** | Excluded from the Dataset while the PNG remains on disk | No |

### 14.9 Remove from Dataset behavior

Selecting **Remove from Dataset** SHALL:

- retain the PNG on disk;
- retain the crop entry in `dataset.json`;
- set the crop state to `removed`;
- exclude the crop from Training and Model Test;
- allow restoration.

### 14.10 Image Counts

The workspace SHALL display factual counts, for example:

```text
Class 0 — Empty                 942
Class 1 — Single cell           731
Class 2 — Multiple cells        114
Unlabeled                        83
Skipped                          19
Removed                           7
```

The heading SHALL be **Image Counts**, not a quality warning.

The system SHALL NOT warn about class imbalance or prevent labeling/training because of the counts.

### 14.11 Saving

Changes SHALL be saved to the selected `dataset.json`.

The system SHOULD use atomic file replacement so that a failed write does not leave truncated JSON.

### 14.12 Editing after training

Class Names and labels SHALL remain editable after the Dataset has been used for Training.

The system SHALL NOT create or require user-facing Dataset versions.

Existing model packages SHALL retain the Class Name snapshot stored when each model was trained; editing the Dataset SHALL NOT silently rewrite existing model packages.

### 14.13 Acceptance criteria

- A user can complete a two-class or three-class Dataset.
- Class IDs remain numerical and stable.
- Class Names are user-editable.
- Skip and Remove are distinct.
- Removed files remain on disk.
- Only Labeled crops enter Training and Model Test.
- Image Counts are shown without judgment or blocking.

---

# PART III — MODEL WORKFLOWS

## 15. Workflow MODEL-01: Train a Model

### 15.1 Purpose

Train a two-class or three-class droplet-classification model from a labeled OpenDSS Dataset.

### 15.2 Entry point

```text
Models > Train
```

### 15.3 Preconditions

| ID | Requirement |
|---|---|
| TRAIN-001 | A readable OpenDSS `dataset.json` SHALL be selected. |
| TRAIN-002 | The Dataset SHALL contain Labeled Droplet Crops. |
| TRAIN-003 | Referenced Labeled crops SHALL be readable. |
| TRAIN-004 | The bundled application-owned Python environment SHALL be usable. |
| TRAIN-005 | A writable temporary and final model location SHALL exist. |
| TRAIN-006 | No mutually exclusive operation SHALL be active. |

The system SHALL attempt the requested training and SHALL report technical failure if the trainer cannot proceed.

### 15.4 Standard user inputs

The normal interface SHALL contain:

```text
Dataset
Model Type
    Faster
    More Accurate
Advanced Parameters
Start Training
```

The user SHALL NOT be required to select:

- CPU or GPU;
- training/validation/test ratios;
- random seed;
- batch size;
- learning rate;
- epochs;
- augmentation;
- early stopping;
- optimizer.

Those technical fields MAY be available in **Advanced Parameters**, except the fixed split and fixed seed.

### 15.5 Model Type

**Faster** and **More Accurate** SHALL be one user-facing model choice.

Technical architecture names MAY be displayed as secondary information, such as:

```text
Faster — MobileNet-based
More Accurate — EfficientNet-based
```

### 15.6 Fixed automatic split

Training SHALL automatically divide eligible Labeled crops into:

```text
70% Training
15% Validation
15% Internal Test
```

The fixed random seed SHALL be:

```text
1729
```

The split and seed SHALL NOT be editable in the normal or Advanced interface.

The split assignment and ratios SHALL be stored in model training metadata.

### 15.7 Device selection

The system SHALL select the training device automatically:

```text
Compatible bundled GPU environment available
    → GPU training

Otherwise
    → CPU training
```

The selected device SHALL be visible as factual status.

The user SHALL NOT be required to choose CPU or GPU.

Live sorting inference remains CPU-based and is not changed by the training device.

### 15.8 Advanced Parameters

Advanced Parameters SHALL use a structured form with named fields and reset-to-default controls.

It SHALL NOT require users to edit raw JSON.

The Advanced section MAY include supported hyperparameters such as:

- batch size;
- epoch or stage settings;
- learning rates;
- optimizer;
- weight decay;
- augmentation;
- early stopping;
- scheduler settings.

The fixed 70/15/15 split and seed 1729 SHALL remain read-only or hidden.

### 15.9 Primary flow

1. The user selects a Dataset.
2. The system loads the Dataset through the authoritative dataset loader.
3. The interface displays:
   - Dataset Name;
   - two-class or three-class schema;
   - Class IDs and Class Names;
   - eligible image counts.
4. The user selects **Faster** or **More Accurate**.
5. The user optionally changes Advanced Parameters.
6. The user selects **Start Training**.
7. The system selects GPU or CPU automatically.
8. The system creates a temporary training run location.
9. Training begins.
10. Live metrics are displayed.
11. When training technically completes, the system opens **Name Model**.
12. The user enters a Model Name and selects a Save Location.
13. The system saves the model package.
14. The new model automatically becomes the Active Model.
15. The Model Library refreshes.

### 15.10 Live training display

The Train workspace SHALL display:

- elapsed time;
- epoch progress;
- overall progress;
- training loss;
- validation loss;
- validation accuracy;
- per-class validation accuracy;
- macro F1;
- estimated time remaining;
- selected device.

Technical console output SHALL NOT be part of the normal interface.

Technical details MAY be written to diagnostic files.

### 15.11 Stop Training

The user SHALL be able to stop Training.

A user-stopped or technically failed training operation SHALL NOT create a normal completed model package unless all required model artifacts were successfully produced and the user explicitly saves them.

### 15.12 Scientific performance

The system SHALL display the completed metrics.

It SHALL NOT:

- reject the model because it predicts one class;
- prevent saving because of low accuracy;
- retain the previous Active Model because the system considers the result poor;
- require Model Test;
- assign approved, candidate, rejected, promoted, or certified states.

If the training code completes and produces the required files, the user may save the model.

### 15.13 Model package

A completed model package SHALL contain at minimum:

```text
<model-folder>\
├── metadata.json
├── checkpoint.pth
└── model.onnx
```

The package SHALL preserve:

- Model ID and Model Name;
- source Dataset identification;
- source Dataset Class IDs and Class Name snapshot;
- source label counts;
- architecture and Model Type;
- fixed split and split assignment or reproducible split information;
- random seed;
- training parameters;
- training and internal-test metrics;
- Python version;
- package versions;
- CPU/GPU device information;
- OpenDSS version;
- ONNX input/output contract;
- model and artifact checksums.

### 15.14 Active Model behavior

After successful save:

- the model SHALL become the global Active Model;
- the Active Model selection SHALL persist across application restarts;
- Library, Model Test, Sequence Test, Sort Setup, and Live SHALL read Active Model state from the same authoritative model registry.

### 15.15 Acceptance criteria

- Training works without asking the user to install or locate Python.
- Device selection is automatic.
- The 70/15/15 split and seed 1729 are fixed.
- Live metrics update during Training.
- A technically completed low-performing model can still be named, saved, and activated.
- A saved model contains all required package files and provenance.
- The saved model immediately appears as Active Model.

---

## 16. Workflow MODEL-02: Model Test

### 16.1 Purpose

Apply a selected model to a selected labeled OpenDSS Dataset and report classification results.

### 16.2 Entry point

```text
Models > Model Test
```

### 16.3 Product rule

Model Test is optional and observational.

It SHALL NOT approve, reject, activate, deactivate, promote, certify, or otherwise change model status.

### 16.4 Preconditions

| ID | Requirement |
|---|---|
| TEST-001 | A readable model package SHALL be selected. |
| TEST-002 | A readable OpenDSS `dataset.json` SHALL be selected. |
| TEST-003 | The model and Dataset SHALL have the same class count. |
| TEST-004 | Eligible Labeled crops SHALL be readable. |
| TEST-005 | No mutually exclusive operation SHALL be active. |

### 16.5 Same-Dataset behavior

The system SHALL permit the user to select the same Dataset that was used for Training.

It SHALL NOT show a warning, ask for confirmation, or refuse the operation on that basis.

### 16.6 Class-count mismatch

The system SHALL block Model Test when:

```text
2-class model + 3-class Dataset
or
3-class model + 2-class Dataset
```

The message SHALL state the factual incompatibility, for example:

> The selected model has 2 output classes, but the selected Dataset defines 3 classes.

### 16.7 Eligible images

Model Test SHALL process crops whose state is `labeled`.

It SHALL exclude:

- Unlabeled;
- Skipped;
- Removed.

### 16.8 Primary flow

1. The user opens Model Test.
2. The user selects a model.
3. The user selects a Dataset.
4. The system loads both through their authoritative loaders.
5. The system verifies technical readability and matching class counts.
6. The user selects **Start Model Test**.
7. Inference is run over all eligible Labeled crops.
8. The interface displays progress.
9. On completion, the interface displays:
   - Overall Accuracy;
   - Per-Class Accuracy;
   - Confusion Matrix.
10. The user may export or open the result CSV.

### 16.9 Test CSV

The exported prediction CSV SHALL contain one row per tested crop:

```text
image_path
true_class_id
predicted_class_id
score_class_0
score_class_1
score_class_2
correct
```

For a two-class model, `score_class_2` SHALL be omitted.

The raw model outputs SHALL be named **Class Scores**, not Confidence or Probability, unless a future model contract explicitly changes the output semantics.

### 16.10 Misclassified images

The first release SHALL NOT require an integrated misclassified-image browser or copied misclassification folder.

Every misclassified result SHALL remain identifiable through its CSV row and original image path.

### 16.11 Suggested output structure

```text
ModelTests\
└── <timestamp>\
    ├── model_test_summary.json
    └── predictions.csv
```

The exact folder may be user-selected.

### 16.12 Acceptance criteria

- The same Dataset used for Training can be selected without warning.
- Different compatible Datasets can be tested.
- Class-count mismatch is blocked.
- The test does not change Active Model state.
- Overall accuracy, per-class accuracy, and confusion matrix are displayed.
- Per-image model output is exportable through CSV.

---

## 17. Workflow MODEL-03: Model Library

### 17.1 Purpose

Manage local model packages and the global Active Model.

### 17.2 Entry point

```text
Models > Library
```

### 17.3 Model list information

Each model entry SHOULD display:

- Model Name;
- Model Type or architecture;
- class count;
- Class IDs and Class Names;
- creation date;
- source Dataset;
- whether it is Active;
- last-known package location.

### 17.4 Required actions

The Library SHALL support:

```text
Set Active
Import Model
Export Model
Duplicate Model
Rename Model
Delete Model
```

The Library SHALL also allow the user to view:

- source Dataset information;
- training metadata;
- model metrics;
- model package location.

### 17.5 Import behavior

Import SHALL perform only technical loadability checks needed to determine whether the package can be used.

Minimum checks include:

- `metadata.json` exists and parses;
- required artifact paths exist;
- `model.onnx` can be opened;
- class count is two or three;
- metadata class count and ONNX output dimension agree;
- required preprocessing and input dimensions exist.

Import SHALL NOT assign a scientific compatibility, approval, recommendation, or quality status.

### 17.6 Class Names

Model metadata SHALL contain the Class Name snapshot copied from the source Dataset at Training time.

Imported packages SHALL display their stored Class Names.

### 17.7 Rename and duplicate

- Rename Model SHALL update the model’s user-facing name without changing Class IDs or model weights.
- Duplicate Model SHALL create an independent package copy with a new Model ID.
- Export Model SHALL export the complete package.
- Import Model SHALL copy or register the complete package according to the selected storage behavior.

### 17.8 Delete behavior

Delete Model SHALL require a direct confirmation because it removes a model package.

Deleting the Active Model SHALL clear Active Model state unless the user first selects another model.

### 17.9 Acceptance criteria

- One and only one Active Model is represented globally.
- Set Active persists across restarts.
- Imported packages can be used without the source Dataset being present.
- Exported packages contain the Class Name snapshot and provenance.
- No archive or recommendation state exists.

---

# PART IV — SEQUENCE REVIEW AND TESTING

## 18. Workflow DATA-05: Sequence Viewer

### 18.1 Purpose

Allow the user to visually inspect a recorded Image Sequence without requiring a connected camera.

### 18.2 Entry point

```text
Data > Sequence Viewer
```

### 18.3 Supported inputs

The Sequence Viewer SHALL open:

- a standalone `sequence.json`;
- the sequence referenced by an OpenDSS `dataset.json`;
- a saved sequence associated with a Run.

### 18.4 Required controls

The Sequence Viewer SHALL provide:

```text
Open Sequence
Previous Frame
Next Frame
Direct Frame Seek
Zoom In / Zoom Out
Pan
Fit
1:1
```

### 18.5 Display behavior

The viewer SHALL display:

- current frame;
- current frame number;
- total frame count.

Keyboard navigation SHALL use Left/Right for one frame, Shift+Left/Right for 10, Ctrl+Left/Right for 50, Home/End for first/last, plus/minus for zoom, F for Fit, and 1 for 1:1.

Missing frames SHALL be skipped silently.

### 18.6 Hardware behavior

The Sequence Viewer SHALL NOT require:

- camera;
- DAQ;
- Active Model;
- Python training environment.

It SHALL NOT emit DAQ output.

### 18.7 Acceptance criteria

- Numbered TIFF sequences navigate in correct order.
- The user can step frame by frame.
- Direct seek opens the selected readable frame.
- Zoom does not alter the source files.
- No automatic frame progression or frame-navigation lifecycle state is present.

---

## 19. Workflow MODEL-04: Sequence Test

### 19.1 Purpose

Process a recorded Image Sequence through droplet detection, optional classification, routing logic, visual trajectory tracking, and optional physical DAQ output.

Sequence Test also serves as the supported reclassification workflow for previously recorded sequences.

### 19.2 Entry point

```text
Models > Sequence Test
```

### 19.3 Preconditions

| ID | Requirement |
|---|---|
| SEQTEST-001 | A readable OpenDSS Image Sequence SHALL be selected. |
| SEQTEST-002 | Droplet-detection settings SHALL be available. |
| SEQTEST-003 | A model SHALL be selected for Class-Based Sorting. |
| SEQTEST-004 | A model MAY be absent for Trigger Every Droplet. |
| SEQTEST-005 | If Physical DAQ Output is enabled, the DAQ SHALL be Ready. |
| SEQTEST-006 | The output folder SHALL be writable. |
| SEQTEST-007 | No mutually exclusive operation SHALL be active. |

A connected camera SHALL NOT be required.

### 19.4 Required controls

```text
Sequence
Model
Trigger Every Droplet
Hit Class
Hit boundary calibration
Physical DAQ Output Enabled
Start Sequence Test
Stop Sequence Test
```

### 19.5 Physical DAQ default

The **Physical DAQ Output** checkbox SHALL start unchecked.

The state SHALL be visually explicit.

If the checkbox is checked and the DAQ is unavailable, Start Sequence Test SHALL be disabled.

If the checkbox is unchecked, Sequence Test MAY run without DAQ hardware.

### 19.6 Processing rate

Sequence Test SHALL process frames at the user-selected Processing FPS, defaulted from the recorded FPS, and SHALL record the requested and achieved rates in the Run Summary.

Sequence Test Processing FPS SHALL NOT control Sequence Test execution rate.

### 19.7 Trigger modes

#### Class-Based Sorting

- detection runs;
- classification runs;
- Predicted Class is the largest Class Score;
- Predicted Class equal to Hit Class produces Decision = Hit;
- all other predictions produce Decision = Waste;
- physical DAQ output occurs for Hit Decisions only when enabled.

#### Trigger Every Droplet

- detection runs;
- every detected droplet produces Decision = Hit;
- physical DAQ output occurs for every detected droplet when enabled;
- classification also runs and is logged when a model is selected;
- classification does not control the Decision;
- if no model is selected, Predicted Class and Class Score fields remain empty.

### 19.8 Observed Route

The sequence-processing pipeline SHALL derive Observed Route from y-axis trajectory using the selected Hit boundary calibration.

Each finalized event SHALL contain either:

```text
Observed Route = Hit
or
Observed Route = Waste
```

The first-release event schema SHALL NOT expose an Unknown route.

### 19.9 Primary flow

1. The user selects an Image Sequence.
2. The user selects or confirms processing settings.
3. The user confirms Trigger Every Droplet.
4. The user confirms Hit Class when Class-Based Sorting is selected.
5. The user confirms Hit boundary calibration.
6. The user checks Physical DAQ Output only when physical output is required; otherwise it remains unchecked.
7. The user selects **Start Sequence Test**.
8. The system creates a Run folder with operation type `sequence_test`.
9. The sequence is processed as quickly as possible.
10. Per-droplet events and Droplet Crops are written.
11. Optional physical DAQ output occurs according to Trigger Every Droplet.
12. The user may stop the test.
13. On completion or stop, the system finalizes the Droplet Log and Run Summary.
14. The Run appears in **Results > Runs**.

### 19.10 Acceptance criteria

- Sequence Test works without a camera.
- With DAQ output disabled, it works without DAQ hardware.
- With Trigger Every Droplet and no model, detection and event logging still work.
- With a model, Predicted Class and Class Scores are logged.
- A formal Run is created.
- The source Image Sequence is not modified.

---

# PART V — PHYSICAL SORTING

## 20. Workflow SORT-01: Configure Sorting

### 20.1 Purpose

Allow the user to define the settings for a Live Sorting Run.

### 20.2 Entry point

```text
Sort > Setup
```

### 20.3 Optional run information

The following fields SHALL be optional:

| Field | Default |
|---|---|
| Run Name | Timestamp |
| Experiment Type | Empty |
| Notes | Empty |
| Duration | Empty; continue until Stop |
| Save Location | `Documents\OpenDropletSortingSuite\Runs` |

A blank Run Name SHALL become a timestamp.

### 20.4 Required operational selections

The setup SHALL expose:

- Setup Profile;
- Active Model;
- Trigger Every Droplet;
- Hit Class;
- Hit boundary calibration;
- DAQ Output Channel;
- camera settings;
- droplet-detection settings;
- Droplet Crop settings;
- DAQ settings;
- timing settings;
- optional **Record Full Image Sequence**;
- **Send Test Pulse**.

### 20.5 Hit Class

For Class-Based Sorting, the user SHALL select exactly one Hit Class:

```text
Class 0
Class 1
or
Class 2
```

All other classes produce a Waste Decision.

The system SHALL NOT select a Hit Class based on class name or model metadata policy.

### 20.6 Hit boundary calibration

The user SHALL select the image y-axis direction corresponding to the physical Hit outlet:

```text
+Y — Downward in the displayed image
−Y — Upward in the displayed image
```

The opposite direction SHALL represent Waste.

The interface SHOULD show the resulting mapping graphically:

```text
Hit: +Y ↓
Waste: −Y ↑
```

or:

```text
Hit: −Y ↑
Waste: +Y ↓
```

### 20.7 Coordinate system

The Hit boundary calibration SHALL use the coordinate system of the displayed camera image:

```text
+Y = downward
−Y = upward
```

### 20.8 Separate channel terminology

The system SHALL separately label:

```text
Hit Class
Hit boundary calibration
DAQ Output Channel
```

It SHALL NOT use the ambiguous term **Hit Channel**.

### 20.9 Trigger modes

The user SHALL select one of:

```text
Class-Based Sorting
Trigger Every Droplet
```

#### Class-Based Sorting prerequisite

An Active Model SHALL be required.

#### Trigger Every Droplet prerequisite

An Active Model SHALL NOT be required.

If an Active Model exists, classification SHALL still run and be logged, but it SHALL NOT control triggering.

### 20.10 Send Test Pulse

**Send Test Pulse** SHALL be available to the normal user.

It SHALL issue one DAQ pulse using the current applied DAQ settings.

It SHALL require a Ready DAQ.

It SHALL not create a Run or Droplet Log entry unless a future hardware-test log is separately specified.

### 20.11 Record Full Image Sequence

The Live Sorting setup SHALL include:

```text
☐ Record Full Image Sequence
```

Droplet Crops and the Droplet Log are always saved.

The full sequence is saved only when this option is selected.

### 20.12 Setup Profiles

The user SHALL be able to:

```text
Select Setup Profile
Save Setup Profile
Import Setup Profile
Export Setup Profile
Delete Setup Profile
```

Selecting a Setup Profile SHALL load its values immediately.

A Setup Profile SHALL include at least:

- camera settings;
- droplet-detection settings;
- Droplet Crop settings;
- DAQ settings;
- timing settings;
- Active Model reference;
- Hit Class;
- Hit boundary calibration;
- Run Name.

A Setup Profile MAY also retain:

- Trigger Every Droplet;
- Record Full Image Sequence selection;
- default Save Location.

Notes and Experiment Type SHOULD remain run-specific unless explicitly saved by the user.

### 20.13 Offline profile viewing

A Setup Profile MAY be opened and inspected without connected hardware.

Hardware controls and Apply behavior SHALL remain unavailable when the corresponding device is unavailable.

### 20.14 Start enablement

**Start Sorting** SHALL be enabled only when the selected operation is technically ready.

#### Class-Based Sorting

Required:

- Camera Streaming;
- DAQ Ready;
- Active Model loadable;
- Hit Class selected;
- Hit boundary calibration selected;
- output folder writable.

#### Trigger Every Droplet

Required:

- Camera Streaming;
- DAQ Ready;
- Hit boundary calibration selected;
- output folder writable.

A model is optional.

### 20.15 Acceptance criteria

- The user explicitly chooses both Hit Class and Hit boundary calibration.
- Positive/negative y directions are shown relative to the displayed image.
- Test Pulse is available to a normal user.
- Duration can be blank.
- Setup Profiles can be imported and exported.
- Start is disabled only for factual technical blockers.

---

## 21. Workflow SORT-02: Live Sorting

### 21.1 Purpose

Detect, classify when applicable, trigger the physical sorter, track the visual route, and record each droplet.

### 21.2 Entry point

```text
Sort > Live
```

The user normally reaches Live after configuring Sort > Setup, but Live remains a persistent workspace.

### 21.3 Preconditions

The setup conditions in Section 20.14 SHALL be satisfied.

### 21.4 Run creation

When the user selects **Start Sorting**, the system SHALL:

1. create a Run ID;
2. use the entered Run Name or a timestamp;
3. create a Run folder;
4. write an initial `run_summary.json`;
5. open a recoverable partial Droplet Log;
6. start the selected processing pipeline.

### 21.5 Class-Based Sorting event flow

```text
Frame acquired
    ↓
Droplet detected
    ↓
First complete droplet crop selected
    ↓
CPU inference
    ↓
Class Scores produced
    ↓
Predicted Class = index of largest score
    ↓
Predicted Class == Hit Class?
    ├── Yes → Decision = Hit → issue DAQ output
    └── No  → Decision = Waste → no Hit output
    ↓
Track y-axis trajectory
    ↓
Map direction to Observed Route
    ↓
Save Droplet Crop and event row
    ↓
Update live counters
```

### 21.6 No threshold

The first release SHALL NOT use a confidence or score threshold to alter routing.

Decision SHALL be determined by:

```text
argmax(Class Scores) == Hit Class
```

Class Scores SHALL be logged as factual model output.

### 21.7 Trigger Every Droplet event flow

```text
Frame acquired
    ↓
Droplet detected
    ↓
Decision = Hit
    ↓
Issue DAQ output
    ↓
If a model is available:
    classify and log Predicted Class and Class Scores
Else:
    leave model fields empty
    ↓
Track y-axis trajectory
    ↓
Save event
```

### 21.8 Inference device

Live model inference SHALL use the qualified CPU path.

GPU availability SHALL NOT be required for Live Sorting.

### 21.9 Observed Route

Observed Route SHALL be based on visual y-axis trajectory.

It SHALL be mapped using the user-selected Hit boundary calibration.

The two allowed values are:

```text
Hit
Waste
```

No downstream sensor confirmation is assumed.

The interface and files SHALL use **Observed Route**, not **Actual Destination**.

### 21.10 Live display

The Live workspace SHALL display:

- live camera image;
- Total Droplets;
- Predicted Class counts;
- Decision Hit count;
- Decision Waste count;
- Observed Hit count;
- Observed Waste count;
- Inference Time;
- camera FPS;
- elapsed Run time;
- current Trigger Every Droplet;
- Active Model when present.

### 21.11 Optional overlays

The first release does not require saved overlay images.

The live preview MAY display non-persistent detection or trajectory indicators when useful, provided they do not alter saved source images.

### 21.12 Droplet Crop saving

Every completed sorting event SHALL save one Droplet Crop.

Droplet Crop saving SHALL not be optional.

### 21.13 Full-sequence saving

If **Record Full Image Sequence** is selected:

- all full frames SHALL be written as numbered TIFF files;
- sequence information SHALL be referenced by `run_summary.json`.

If it is not selected:

- no full-frame sequence is required;
- Droplet Crops and event data are still required.

### 21.14 Pause

When the user selects **Pause**:

- the live camera preview SHALL continue;
- inference SHALL stop;
- new DAQ triggers SHALL stop;
- event finalization and new event logging SHALL stop;
- the Run remains open;
- Current Activity SHALL display Paused.

Resume SHALL continue the same Run.

### 21.15 Stop and Duration

The Run SHALL stop when:

- the user selects Stop;
- the optional Duration expires;
- a technical fault requires interruption.

A user Stop before the Duration expires is permitted.

### 21.16 Minimum fault behavior

If the camera disconnects, the DAQ becomes unavailable, or inference repeatedly fails during Live Sorting, the system SHALL:

1. stop new inference when applicable;
2. stop new DAQ commands;
3. stop or interrupt the Run;
4. flush recoverable event and image data;
5. mark the Run Interrupted or Failed;
6. retain camera preview only if the camera remains functional;
7. show one plain-language error.

### 21.17 No software emergency-stop claim

The first release SHALL NOT label a GUI control as a safety-rated Emergency Stop.

Normal Stop and fault-stop behavior do not replace any required physical safety control.

### 21.18 Acceptance criteria

- Two-class and three-class models can be used.
- The user chooses the Hit Class.
- The user chooses the physical Hit direction.
- Model outputs and physical route observations remain separate.
- Live counters distinguish Decision and Observed Route.
- Every event has a saved crop and CSV row.
- Pause keeps the camera preview active and stops inference/output.
- A blank Duration allows indefinite operation until Stop.
- Faults stop new DAQ output and preserve recoverable data.

---

# PART VI — RESULTS

## 22. Workflow RESULT-01: Review Runs

### 22.1 Purpose

Review completed, stopped, interrupted, or failed Live Sorting and Sequence Test operations.

### 22.2 Entry point

```text
Results > Runs
```

### 22.3 Run scope

Only the following operations are Runs:

```text
Live Sorting
Sequence Test
```

The following are not Runs:

```text
Single Image capture
Image Sequence capture
Droplet Dataset Capture
Labeling
Training
Model Test
Sequence Viewer
```

### 22.4 Run list

The Runs workspace SHALL discover Run folders and display at least:

- Run Name;
- operation type;
- start timestamp;
- duration;
- status;
- model name when present;
- total droplet count.

### 22.5 Run status

Supported Run Status values SHALL include:

```text
Completed
Stopped
Interrupted
Failed
```

### 22.6 Run Summary

Selecting a Run SHALL display:

#### Run information

- Run Name;
- operation type;
- Experiment Type;
- Notes;
- start/end timestamps;
- requested Duration;
- elapsed duration;
- stop reason;
- status;
- save location.

#### Model and routing information

- Model Name and ID when present;
- model checksum;
- Class IDs and Class Name snapshot;
- Hit Class;
- Hit boundary calibration;
- Trigger Every Droplet;
- Physical DAQ Output state for Sequence Test.

#### Hardware and processing information

- camera settings;
- droplet-detection settings;
- Droplet Crop settings;
- DAQ settings;
- timing settings;
- OpenDSS version.

#### Counts

- Total Droplets;
- Predicted Class count for each class;
- Decision Hit;
- Decision Waste;
- Observed Hit;
- Observed Waste.

### 22.7 Decision vs. Observed Route

The Run Summary SHALL display:

> **Decision vs. Observed Route**

| Decision | Observed Hit | Observed Waste |
|---|---:|---:|
| Hit | count | count |
| Waste | count | count |

The system SHALL NOT label this table:

- Actual Destination;
- Ground Truth Route;
- Routing Accuracy;
- Predicted Hit vs. Actual Hit.

The first release MAY display calculated percentages, but SHALL NOT automatically interpret them as acceptable or unacceptable.

### 22.8 Notes

Notes SHALL be editable after the Run.

Notes SHALL be stored as structured JSON data in `run_summary.json`.

No audit trail or amendment history is required for first release.

### 22.9 Required actions

The selected Run SHALL provide:

```text
Open Droplet Log
Open Run Folder
Open Droplet Crop
Open Saved Sequence        when present
Edit Notes
Save Notes
```

An integrated event-by-event browser is useful later but is not required for first release.

### 22.10 Charts

Charts are deferred.

The first release SHALL prioritize:

- factual summary values;
- the Decision vs. Observed Route matrix;
- the Droplet Log;
- direct access to saved files.

### 22.11 Acceptance criteria

- Interrupted and failed Runs remain visible.
- Run Summary is rendered from structured files.
- Decision and Observed Route are not conflated.
- Notes can be edited without changing event data.
- The user can open the Droplet Log and Run folder.
- No separate Reports workspace is needed.

---

# PART VII — SETTINGS AND PROFILES

## 23. Workflow SET-01: Settings

### 23.1 Purpose

Provide direct access to user-controlled application and hardware configuration.

### 23.2 Entry point

```text
Settings
```

### 23.3 Recommended groups

```text
Camera
Droplet Detection
Droplet Crops
DAQ
Sorting
Training
Storage
Advanced
```

### 23.4 Camera settings

Camera settings SHALL reflect controls supported by the qualified camera integration.

The application SHALL NOT apply camera settings while a camera-owning operation is active.

### 23.5 Droplet Detection settings

The user SHALL control supported droplet-detection parameters.

The system SHALL record the applied values in Dataset and Run metadata.

### 23.6 Droplet Crop settings

The first-release training Dataset contract SHALL save Droplet Crops as 64 × 64 PNG.

Other detector or preview crop parameters MAY be user-controlled where supported, but the saved training-crop contract SHALL remain stable.

### 23.7 DAQ settings

DAQ settings SHALL include the qualified device, output channel, waveform/pulse settings, and timing settings needed by the existing hardware integration.

The DAQ Output Channel SHALL remain distinct from Hit boundary calibration.

### 23.8 Sorting settings

Sorting settings SHALL include:

- Trigger Every Droplet;
- Hit Class;
- Hit boundary calibration;
- timing values;
- optional full-sequence recording.

### 23.9 Storage settings

The user SHALL be able to:

- view the default data root;
- select another root or per-operation Save Location;
- open the data root in Windows Explorer.

### 23.10 Training settings

The system MAY expose information about the bundled training environment and selected device.

The user SHALL NOT be required to browse for a Python executable or manually register a virtual environment.

### 23.11 Offline hardware behavior

Without hardware:

- settings and Setup Profiles MAY be viewed;
- profiles MAY be imported or exported;
- hardware Apply/Test controls SHALL be disabled;
- no named diagnostic mode SHALL be introduced.

---

# PART VIII — DATA AND FILE CONTRACTS

## 24. Artifact naming

The first-release canonical filenames are:

| Object | Canonical file |
|---|---|
| Image Sequence | `sequence.json` |
| Dataset | `dataset.json` |
| Model package | `metadata.json` |
| Run Summary | `run_summary.json` |
| Droplet Log | `events.csv` |

Active or interrupted writes MAY use temporary names such as:

```text
events.partial.csv
dataset.json.tmp
run_summary.json.tmp
```

Temporary names SHALL not replace canonical finalized names.

---

## 25. Dataset file contract

### 25.1 Proposed top-level shape

```json
{
  "schema_version": 1,
  "dataset_id": "2026-07-21_16-03-41",
  "name": "2026-07-21_16-03-41",
  "experiment_type": "",
  "notes": "",
  "status": "completed",
  "created_at": "2026-07-21T16:03:41-05:00",
  "updated_at": "2026-07-21T16:20:01-05:00",
  "opendss_version": "1.0.0",

  "capture": {
    "started_at": "...",
    "ended_at": "...",
    "requested_duration_seconds": null,
    "stop_reason": "user",
    "sequence_path": "sequence",
    "frame_count": 12000,
    "camera_settings": {},
    "detector_settings": {},
    "crop_settings": {
      "width": 64,
      "height": 64,
      "channels": 1,
      "format": "png"
    }
  },

  "classes": [
    {"id": 0, "name": "Empty"},
    {"id": 1, "name": "Single cell"},
    {"id": 2, "name": "Multiple cells"}
  ],

  "images": [
    {
      "image_id": "droplet_000001",
      "path": "crops/droplet_000001.png",
      "source_frame_index": 184,
      "detection_timestamp": "...",
      "label_id": 1,
      "state": "labeled"
    }
  ],

  "counts": {
    "unlabeled": 0,
    "skipped": 0,
    "removed": 0,
    "class_0": 100,
    "class_1": 100,
    "class_2": 100
  }
}
```

### 25.2 Path rules

Paths inside `dataset.json` SHOULD be relative to the Dataset folder when possible.

This supports moving or copying the complete Dataset folder.

### 25.3 No external arbitrary image contract

A valid first-release Dataset SHALL originate from OpenDSS Droplet Dataset Capture and SHALL be opened through `dataset.json`.

---

## 26. Sequence file contract

### 26.1 Proposed top-level shape

```json
{
  "schema_version": 1,
  "sequence_id": "2026-07-21_15-45-03",
  "name": "2026-07-21_15-45-03",
  "experiment_type": "",
  "notes": "",
  "status": "completed",
  "created_at": "...",
  "started_at": "...",
  "ended_at": "...",
  "requested_duration_seconds": null,
  "stop_reason": "user",
  "opendss_version": "1.0.0",
  "frame_format": "tiff",
  "frame_count": 10000,
  "frame_filename_pattern": "frames/frame_%08d.tif",
  "camera_settings": {},
  "image": {
    "width": 0,
    "height": 0,
    "bit_depth": 0
  },
  "timing": {
    "timestamps_file": null,
    "nominal_fps": 0.0
  }
}
```

The actual dimensions and bit depth depend on the camera.

---

## 27. Model metadata contract

`metadata.json` SHALL describe the complete model package.

At minimum it SHALL contain:

- schema version;
- Model ID;
- Model Name;
- creation timestamp;
- Model Type and technical architecture;
- class count;
- Class IDs;
- Class Name snapshot;
- class-to-index mapping;
- source Dataset ID, name, and path or source reference;
- source label counts;
- 70/15/15 split;
- seed 1729;
- training parameters;
- training metrics;
- validation metrics;
- internal-test metrics;
- Python and package versions;
- CPU/GPU training device;
- ONNX input tensor contract;
- ONNX output tensor contract;
- Class Score type;
- artifact filenames;
- checksums;
- OpenDSS version.

The model package SHALL NOT hard-code a universal Hit Class.

Hit Class is a user selection made in Sort Setup or a Setup Profile.

---

## 28. Run Summary contract

### 28.1 Proposed top-level shape

```json
{
  "schema_version": 1,
  "run_id": "2026-07-21_17-10-22",
  "run_name": "2026-07-21_17-10-22",
  "operation": "live_sorting",
  "experiment_type": "",
  "notes": "",
  "status": "completed",
  "started_at": "...",
  "ended_at": "...",
  "requested_duration_seconds": null,
  "stop_reason": "user",
  "opendss_version": "1.0.0",

  "model": {
    "model_id": "model_04",
    "model_name": "Model 04",
    "model_sha256": "...",
    "class_count": 3,
    "classes": [
      {"id": 0, "name": "Empty"},
      {"id": 1, "name": "Single cell"},
      {"id": 2, "name": "Multiple cells"}
    ]
  },

  "routing": {
    "trigger_mode": "class_based",
    "hit_class_id": 1,
    "hit_outlet_direction": "negative_y",
    "physical_daq_output_enabled": true
  },

  "settings": {
    "camera": {},
    "detector": {},
    "crop": {},
    "daq": {},
    "timing": {}
  },

  "counts": {
    "total": 412,
    "class_0": 133,
    "class_1": 201,
    "class_2": 78,
    "decision_hit": 201,
    "decision_waste": 211,
    "observed_hit": 193,
    "observed_waste": 219
  },

  "decision_vs_observed": {
    "hit_decision_hit_observed": 188,
    "hit_decision_waste_observed": 13,
    "waste_decision_hit_observed": 5,
    "waste_decision_waste_observed": 206
  },

  "files": {
    "events_csv": "events.csv",
    "crops_path": "crops",
    "sequence_path": null
  }
}
```

### 28.2 Historical names

The Run Summary SHALL preserve the Class Names displayed at Run start.

Later changes to Dataset or model display names SHALL NOT alter historical Run Summaries.

---

## 29. Droplet Log contract

### 29.1 Required columns

`events.csv` SHALL contain one row per finalized droplet event.

Required columns are:

```text
event_id
detection_timestamp
source_frame_index
crop_path
predicted_class_id
score_class_0
score_class_1
score_class_2
decision
observed_route
inference_time_ms
```

For two-class models, `score_class_2` SHALL be omitted or left empty according to one consistent schema convention.

For events without a model:

- `predicted_class_id` SHALL be empty;
- all Class Score fields SHALL be empty;
- `inference_time_ms` SHALL be empty.

### 29.2 Decision values

```text
Hit
Waste
```

### 29.3 Observed Route values

```text
Hit
Waste
```

### 29.4 Trajectory detail

The first release SHALL record only the derived Observed Route in the Droplet Log.

Full per-frame trajectory coordinates are not required.

---

## 30. Setup Profile contract

A Setup Profile SHALL be an importable and exportable JSON file.

It SHALL record the selected configuration without judging it.

Minimum content:

```json
{
  "schema_version": 1,
  "profile_name": "Standard Setup",
  "camera_settings": {},
  "detector_settings": {},
  "crop_settings": {},
  "daq_settings": {},
  "timing_settings": {},
  "active_model_id": "model_04",
  "hit_class_id": 1,
  "hit_outlet_direction": "positive_y",
  "run_name": ""
}
```

The profile MAY additionally include Trigger Every Droplet, full-sequence recording preference, and a default Save Location.

---

# PART IX — PERSISTENCE, RECOVERY, AND PERFORMANCE

## 31. Event persistence

### 31.1 Non-blocking writes

Inference and camera-processing threads SHALL NOT synchronously perform all crop and CSV disk writes.

Completed events SHALL be handed to a bounded persistence queue.

A background writer SHALL:

- save the Droplet Crop;
- append the Droplet Log row;
- update recoverable Run state.

### 31.2 Batched CSV writes

Droplet Log rows SHOULD be written in short batches.

A practical default is to flush after either:

- 50–100 completed events; or
- approximately 500 milliseconds;

whichever occurs first.

These exact batch values are implementation defaults, not user-facing controls.

### 31.3 Mandatory flush points

The persistence queue SHALL be flushed when:

- the user pauses;
- the user stops;
- Duration expires;
- a Run completes;
- a fault interrupts the operation;
- the application closes normally.

### 31.4 Partial files

During an active Run:

```text
events.partial.csv
```

MAY be used.

On clean finalization it SHALL become:

```text
events.csv
```

An interrupted partial file SHALL be preserved for recovery rather than discarded.

### 31.5 Live counters

Live counters SHALL be maintained in memory from finalized events.

The GUI SHALL NOT reread the CSV for every counter update.

### 31.6 Sequence writing

Full-frame TIFF writing SHALL use a separate bounded writer queue so that frames do not accumulate for the entire operation in memory.

---

## 32. JSON integrity

Structured JSON files SHOULD be written through:

1. a temporary file;
2. flush and close;
3. atomic replacement of the canonical file.

This applies to:

- `dataset.json`;
- `sequence.json`;
- `run_summary.json`;
- Setup Profiles;
- mutable model-library registry data.

---

## 33. Crash recovery

On next launch, the application SHOULD discover incomplete operation folders.

It SHOULD distinguish:

```text
Completed
Stopped
Interrupted
Failed
```

It SHOULD preserve and index recoverable data.

It SHALL NOT claim that an interrupted operation completed normally.

---

# PART X — ERROR HANDLING

## 34. Error policy

### ERR-001 — Plain-language primary error

The user-facing message SHALL describe:

- what operation could not continue;
- the direct technical reason when known;
- the affected file or device when useful.

### ERR-002 — No policy language

Errors SHALL NOT use scientific-policy language such as:

```text
Dataset rejected
Model not approved
Model failed validation
Model unsuitable for sorting
Class distribution unacceptable
```

unless the issue is a literal file-schema or technical validation failure, in which case the message must identify the technical mismatch.

### ERR-003 — Detailed diagnostics

Technical details MAY be written to a diagnostic log for support and publication reproducibility.

The normal user interface SHALL not require users to interpret a console.

### ERR-004 — Hardware failure

Hardware failure SHALL stop new unsafe or impossible output and preserve recoverable data.

### ERR-005 — File error

A failed save SHALL never be reported as successful.

---

# PART XI — INSTALLATION AND FIRST LAUNCH

## 35. Installer workflow

### 35.1 Primary flow

1. User launches the signed offline installer with administrator rights.
2. Installer installs OpenDSS and runtime dependencies.
3. Installer installs the application-owned Python environment.
4. Installer installs CPU and GPU-capable training packages included in the installer.
5. Installer creates the default Documents data root.
6. Installer verifies that the bundled Python environment can start.
7. Installer creates Start Menu entries.
8. Installer completes without requiring an internet connection.

### 35.2 GPU behavior

The installed training runtime SHALL detect whether a compatible NVIDIA environment is usable.

If usable, Training SHALL select GPU.

Otherwise, Training SHALL select CPU.

The installer SHALL not require the user to construct or register a virtual environment manually.

### 35.3 External prerequisite reporting

OpenDSS MAY report that DCAM, NI-DAQmx, or an NVIDIA driver is unavailable.

It SHALL not attempt to download those components.

### 35.4 First launch

On first launch:

- no account creation is required;
- no network consent is required;
- no telemetry prompt is required because telemetry does not exist;
- default storage is available;
- camera-free workspaces are usable immediately;
- hardware workspaces show factual unavailable states until dependencies are present.

---

# PART XII — CAMERA-FREE USE

## 36. Supported camera-free activities

Without a connected camera, the user SHALL be able to:

- Label existing Datasets;
- Train Models;
- run Model Test;
- manage the Model Library;
- use Sequence Viewer;
- use Sequence Test when physical DAQ output requirements are satisfied or disabled;
- review Runs;
- inspect saved Droplet Crops;
- edit Run Notes;
- open and inspect Setup Profiles;
- open Live and see **Camera unavailable**.

The system SHALL not introduce a separate mode name.

### 36.1 Hardware controls

Controls that require unavailable hardware SHALL simply be disabled with a direct reason.

---

# PART XIII — REPRODUCIBILITY

## 37. Dataset provenance

`dataset.json` SHALL preserve:

- OpenDSS version;
- camera settings;
- droplet-detection settings;
- Droplet Crop settings;
- timestamps;
- label counts;
- Class IDs and Class Names;
- source-frame references.

### 37.1 Model provenance

Model metadata SHALL preserve:

- source Dataset identification;
- Class IDs and Class Names;
- label counts;
- split ratios;
- split seed;
- model architecture;
- Training parameters;
- Python and package versions;
- CPU/GPU device;
- model checksum;
- training metrics;
- OpenDSS version.

### 37.2 Run provenance

Run Summary SHALL preserve:

- OpenDSS version;
- camera settings;
- detector and crop settings;
- DAQ settings;
- model identity and checksum;
- Class Name snapshot;
- Hit Class;
- Hit boundary calibration;
- Trigger Every Droplet;
- user-entered experiment metadata;
- timing and status.

### 37.3 No dataset-version workflow

The system SHALL NOT require user-facing Dataset versions, branches, revisions, or locking beyond temporary operation locks.

---

# PART XIV — NONFUNCTIONAL REQUIREMENTS

## 38. Usability

- Normal workflows SHALL avoid requiring terminal use.
- Normal workflows SHALL avoid raw JSON editing.
- Controls SHALL use the approved terminology.
- Destructive actions SHALL be explicit.
- Blank optional metadata SHALL be accepted.
- Timestamp defaults SHALL allow immediate operation.

## 39. Responsiveness

- Camera preview SHALL remain responsive during capture and sorting.
- Disk writes SHALL not block inference or preview rendering.
- Training progress SHALL update without freezing navigation.
- Long operations SHALL expose Stop.

## 40. Data integrity

- Saved files SHALL use stable schemas with schema versions.
- Relative paths SHOULD be used within portable folders.
- Partial results SHALL be preserved after interruption.
- Run counts SHALL be derived from finalized event records.

## 41. Scientific transparency

- Raw Class Scores MAY be logged.
- Routing rules SHALL be explicit.
- Hit Class and Hit boundary calibration SHALL be visible before sorting.
- The system SHALL not hide the selected model or setup in a Run Summary.

## 42. Privacy and network behavior

- No telemetry.
- No update checks.
- No network dependency.
- No cloud storage.
- No account.
- No automatic data upload.

---

# PART XV — EXPLICIT FIRST-RELEASE EXCLUSIONS

## 43. Out of scope

Unless separately approved, the first public release SHALL NOT include:

- more than three classes;
- arbitrary user-supplied model architectures;
- multiple simultaneous cameras;
- multiple simultaneous sorting output paths;
- cloud training;
- remote control;
- user accounts;
- permissions;
- project-management hierarchy;
- collaborative labeling;
- automated hyperparameter search;
- mandatory Model Test;
- model approval or promotion gates;
- dataset quality scores;
- class-balance warnings;
- confidence-threshold routing;
- external arbitrary image import;
- merging multiple Droplet Dataset Captures;
- integrated per-event Run browser;
- first-class Run charts;
- telemetry;
- automatic updates;
- plugin architecture;
- a named no-hardware mode;
- software emergency-stop claims.

No placeholder buttons SHALL be added for excluded features.

---

# PART XVI — END-TO-END ACCEPTANCE SCENARIOS

## 44. AC-01 — Offline installation

**Given** a Windows 11 computer with no internet connection  
**When** the user runs the full installer  
**Then** OpenDSS, application dependencies, Python, CPU training, GPU training packages, CUDA runtime components, scripts, and base weights are installed  
**And** no online download is required  
**And** the default Documents data root is created.

## 45. AC-02 — Launch without hardware

**Given** no camera or DAQ is connected  
**When** OpenDSS launches  
**Then** the application opens normally  
**And** camera-dependent controls are disabled  
**And** the preview says **Camera unavailable**  
**And** Label, Train, Model Test, Library, Sequence Viewer, and Results remain available as their file prerequisites permit.

## 46. AC-03 — Single Image

**Given** the camera is streaming  
**When** the user selects Capture Image with no filename  
**Then** one timestamp-named TIFF is saved  
**And** no Dataset or Run is created.

## 47. AC-04 — Untimed Image Sequence

**Given** the camera is streaming  
**And** Duration is blank  
**When** the user starts recording and later selects Stop  
**Then** numbered TIFF frames and `sequence.json` are finalized.

## 48. AC-05 — Timed Image Sequence

**Given** a Duration is entered  
**When** recording reaches that Duration  
**Then** recording stops automatically  
**And** the user could have stopped earlier.

## 49. AC-06 — Droplet Dataset Capture without model

**Given** no Active Model exists  
**And** the camera is streaming  
**When** Droplet Dataset Capture runs  
**Then** full TIFF frames are recorded  
**And** one 64 × 64 PNG is saved for each detected droplet  
**And** no labels, Hit/Waste values, or model outputs are assigned  
**And** `dataset.json` is finalized.

## 50. AC-07 — Two-class labeling

**Given** an unlabeled Dataset  
**When** the user chooses two classes and labels images  
**Then** only Class IDs 0 and 1 are used  
**And** the user may edit their Class Names.

## 51. AC-08 — Three-class labeling

**Given** an unlabeled Dataset  
**When** the user chooses three classes  
**Then** Class IDs 0, 1, and 2 are available.

## 52. AC-09 — Skip and Remove

**Given** a displayed Droplet Crop  
**When** the user selects Skip  
**Then** it remains available for later review and is excluded from Training.

**When** the user selects Remove from Dataset  
**Then** its PNG remains on disk  
**And** its state becomes Removed  
**And** it is excluded from Training and Model Test.

## 53. AC-10 — Automatic Training split

**Given** an eligible Dataset  
**When** Training starts  
**Then** the system uses 70/15/15 and seed 1729  
**And** the user is not asked to configure them.

## 54. AC-11 — Automatic device selection

**Given** a usable bundled CUDA environment  
**When** Training starts  
**Then** GPU is selected.

**Given** no usable CUDA environment  
**When** Training starts  
**Then** CPU is selected.

## 55. AC-12 — User retains scientific authority

**Given** Training completes with poor metrics or one dominant predicted class  
**When** the user chooses to save it  
**Then** the system permits naming and saving  
**And** the model becomes Active  
**Provided** the required artifacts were technically produced.

## 56. AC-13 — Model Test on source Dataset

**Given** the user selects the same Dataset used for Training  
**When** Model Test starts  
**Then** the test runs without warning about dataset reuse.

## 57. AC-14 — Model Test class mismatch

**Given** a two-class model and three-class Dataset  
**When** the user attempts Model Test  
**Then** Start is blocked with a direct class-count mismatch message.

## 58. AC-15 — Model package transfer

**Given** a complete exported model package  
**When** another installation imports it  
**Then** technical loadability is checked  
**And** its Class Names and training metadata are visible  
**And** no approval state is assigned.

## 59. AC-16 — Class-Based Sorting

**Given** Camera Streaming, DAQ Ready, an Active Model, a Hit Class, and Hit boundary calibration  
**When** Live Sorting starts  
**Then** each detected droplet is classified on CPU  
**And** a matching Predicted Class produces Decision Hit and DAQ output  
**And** nonmatching classes produce Decision Waste  
**And** Observed Route is tracked independently.

## 60. AC-17 — Three-class routing

**Given** a three-class model  
**And** Class 2 is selected as Hit Class  
**When** Predicted Class is 2  
**Then** Decision is Hit.

**When** Predicted Class is 0 or 1  
**Then** Decision is Waste.

## 61. AC-18 — Hit direction reversal

**Given** Hit boundary calibration is +Y  
**Then** positive-y movement is Observed Hit.

**When** the user changes Hit boundary calibration to −Y  
**Then** negative-y movement is Observed Hit  
**And** positive-y movement is Observed Waste.

## 62. AC-19 — Trigger Every Droplet without model

**Given** no Active Model  
**And** Trigger Every Droplet is selected  
**When** Live Sorting starts  
**Then** every detected droplet produces Decision Hit and DAQ output  
**And** model fields remain empty  
**And** Observed Route and event data are still recorded.

## 63. AC-20 — Trigger Every Droplet with model

**Given** an Active Model exists  
**And** Trigger Every Droplet is selected  
**When** a droplet is detected  
**Then** DAQ output occurs regardless of Predicted Class  
**And** classification is still logged.

## 64. AC-21 — Pause and Resume

**Given** Live Sorting is running  
**When** the user selects Pause  
**Then** the camera preview remains active  
**And** inference and DAQ output stop  
**And** no new events are finalized.

**When** the user selects Resume  
**Then** the same Run continues.

## 65. AC-22 — Optional full sequence

**Given** Record Full Image Sequence is unchecked  
**When** a Run completes  
**Then** Droplet Crops and events exist  
**And** a full sequence is not required.

**Given** it is checked  
**Then** numbered TIFF frames are also retained.

## 66. AC-23 — Run Summary

**Given** a completed Run  
**When** the user opens it in Results > Runs  
**Then** the user sees class counts, Decision counts, Observed Route counts, and the Decision vs. Observed Route matrix  
**And** can open the Droplet Log and Run folder.

## 67. AC-24 — Sequence Viewer

**Given** a valid sequence  
**When** the user opens Sequence Viewer  
**Then** the user can step, seek directly, zoom, pan, Fit, and select 1:1 without hardware.

## 68. AC-25 — Sequence Test without physical DAQ

**Given** a valid sequence  
**When** the user unchecks Physical DAQ Output Enabled  
**Then** Sequence Test can run without a Ready DAQ  
**And** creates a Run.

## 69. AC-26 — Sequence Test with physical DAQ

**Given** Physical DAQ Output Enabled is checked  
**And** the DAQ is Ready  
**When** Sequence Test runs  
**Then** DAQ output follows the selected Trigger Every Droplet.

## 70. AC-27 — Fault persistence

**Given** a sorting fault after events have been recorded  
**When** the Run is interrupted  
**Then** new DAQ output stops  
**And** recoverable events are flushed  
**And** the Run remains visible as Interrupted or Failed.

## 71. AC-28 — Mutual exclusion

**Given** Training is active  
**When** the user attempts to start Live Sorting  
**Then** Start Sorting is disabled because another operation is active.

## 72. AC-29 — No network communication

**Given** OpenDSS is operating normally  
**Then** it performs no telemetry, update checks, cloud requests, or other required network communication.

---

# PART XVII — APPLICATION-LAYER WORKFLOW OWNERSHIP

## 73. Authoritative workflow services

The reconstructed application layer should enforce the workflow specification through five authoritative services:

```text
AppStateStore
DatasetService
ModelRegistry
RunRepository
HardwareCoordinator
```

### 73.1 AppStateStore

Authoritative for:

- Camera status;
- DAQ status;
- Active Model;
- current Dataset;
- current Sequence;
- current Run;
- Current Activity;
- Pause state;
- current technical error.

All global status displays and enabled states SHALL derive from the same state snapshot.

### 73.2 DatasetService

Authoritative for:

- loading `dataset.json`;
- validating technical readability;
- resolving relative crop paths;
- returning class definitions;
- returning labeled, skipped, removed, and unlabeled counts;
- saving label changes.

Capture, Label, Train, and Model Test SHALL use the same contract.

### 73.3 ModelRegistry

Authoritative for:

- discovering models;
- importing and exporting packages;
- technical package checks;
- Active Model;
- Active Model persistence;
- package metadata;
- duplicate, rename, and delete.

### 73.4 RunRepository

Authoritative for:

- discovering Runs;
- reading `run_summary.json`;
- indexing `events.csv`;
- locating Droplet Crops;
- locating optional sequences;
- editing notes;
- exposing Run status.

### 73.5 HardwareCoordinator

Authoritative for:

- camera connection and streaming state;
- DAQ readiness and active output state;
- applying camera/DAQ settings;
- Send Test Pulse;
- preventing invalid hardware operation;
- fault propagation.

---

## 74. Minimum workflow contract tests

The reconstruction SHALL include at least these fast tests:

1. The same valid `dataset.json` is accepted by Label, Train, and Model Test.
2. The same invalid `dataset.json` produces the same technical error in all three.
3. One Droplet Dataset Capture detection produces one Droplet Crop entry.
4. Skipped and Removed crops are excluded from Training and Model Test.
5. Training uses fixed 70/15/15 and seed 1729.
6. Saving a model makes it Active and persists the selection.
7. Class-Based Sorting maps only the selected Hit Class to Hit.
8. Reversing Hit boundary calibration reverses Observed Route mapping.
9. Trigger Every Droplet runs without a model.
10. RunRepository recognizes `run_summary.json`, `events.csv`, crops, and optional sequence.
11. All status displays and enabled states reflect one AppStateStore snapshot.
12. HardwareCoordinator rejects invalid technical state transitions.
13. A partial Droplet Log survives interruption.
14. Model Test blocks class-count mismatch but allows the source Dataset.
15. No application workflow requires network access.

---

# PART XVIII — REPOSITORY ALIGNMENT

## 75. Reusable repository boundary

The existing repository already separates many low-level functions that correspond to this specification, including application state, camera workers/controllers, dataset labeling and workspace code, model-registry code, pipeline execution, sequence writers, run/log writers, validation, reports, and settings components. The runtime also separates DCAM camera access, DAQ triggering, event detection, metadata loading, and ONNX inference. These boundaries support reconstructing the application/orchestration layer without replacing the proven hardware and inference implementations.

## 76. Existing behaviors superseded by this specification

The current repository includes behaviors that SHALL be treated as historical implementation rather than first-release product requirements, including:

- capture modes tied to Hit/Waste/Mixed collection;
- model-linked automatic labels and confidence during Droplet Dataset Capture;
- a fixed binary Hit/Waste/Exclude dataset schema;
- a model metadata sorting policy that defaults Class 1 as the target;
- user-managed external Python environments;
- user-selected CPU/GPU training;
- training-collapse rejection;
- separate Reports behavior.

The new application layer SHALL follow this specification instead.

## 77. Existing training and inference behavior retained

The existing trainer already defines an automatic 70/15/15 split and seed 1729. Its ONNX export uses a `logits` output, and the current C++ inference code copies the raw output scores and selects the maximum value. This supports the user-facing **Class Score** and argmax-based Predicted Class contract.

The existing pipeline also converts input to grayscale when needed, creates a square crop, resizes it, and writes PNG output. This provides an implementation basis for the fixed 64 × 64 Droplet Crop contract, subject to configuration in the reconstructed workflow.

---

# PART XIX — IMPLEMENTATION DETAILS THAT DO NOT REQUIRE PRODUCT APPROVAL

## 78. Engineering discretion

The following may be selected during implementation without reopening the user-facing workflow, provided this specification remains satisfied:

- exact Qt widget classes;
- exact layout spacing and typography;
- thread implementation;
- queue container type;
- batch size within the persistence requirement;
- JSON serialization library;
- internal identifiers;
- error-code values;
- model-registry index implementation;
- exact temporary filename suffixes;
- exact CMake target names;
- installer technology;
- internal logging format;
- exact progress-bar update rate;
- internal controller and service class names.

---

# Sources

1. OpenDSS_clean repository overview and README:  
   https://github.com/haeminjung12/OpenDSS_clean

2. Runtime source tree, including camera, DAQ, detector, metadata, and ONNX components:  
   https://github.com/haeminjung12/OpenDSS_clean/tree/main/app/runtime

3. Existing desktop application component tree:  
   https://github.com/haeminjung12/OpenDSS_clean/tree/main/app/runtime/desktop_app

4. Existing trainer defaults, including 70/15/15 split, seed 1729, training configuration, metrics, and ONNX output:  
   https://github.com/haeminjung12/OpenDSS_clean/blob/main/training/python/droplet_trainer/train.py

5. Existing C++ ONNX inference implementation and maximum-score class selection:  
   https://github.com/haeminjung12/OpenDSS_clean/blob/main/app/runtime/onnx_classifier.cpp

6. Existing capture-session contract showing Hit/Waste/Mixed modes and model-linked automatic labeling:  
   https://github.com/haeminjung12/OpenDSS_clean/blob/main/app/runtime/dataset_capture_session.h

7. Existing dataset-capture artifacts and fixed Hit/Waste/Exclude schema:  
   https://github.com/haeminjung12/OpenDSS_clean/blob/main/app/runtime/dataset_capture_session.cpp

8. Existing model metadata and Class 1 target policy:  
   https://github.com/haeminjung12/OpenDSS_clean/blob/main/training/python/droplet_trainer/metadata.py

9. Existing crop creation, resizing, classification, triggering, and PNG writing:  
   https://github.com/haeminjung12/OpenDSS_clean/blob/main/app/runtime/desktop_app/pipeline_runner.cpp
