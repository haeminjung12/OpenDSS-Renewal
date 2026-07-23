# OpenDSS v2 Page Composition Interpretation for User Review

**Status:** Source-grounded interpretation for user review — not an approved visual baseline and not implementation authorization  
**Purpose:** Describe exactly how every approved OpenDSS v2 page, workspace, and shared shell surface should look and behave before further Qt Design Studio or production work.  
**Scope:** Documentation interpretation only. Existing application screens and the superseded Single Image-only implementation are not design authority.

**Amendment status:** Reconciled July 23, 2026 to `OpenDSS_v2_UIUX_Design_Amendment.md`. The amendment controls every explicitly changed UI, layout, naming, interaction, and workflow matter. Any retained detail below applies only where it agrees with this summary and the revised canonical sources.

## Amendment-controlled page composition

| Page or surface | Review composition |
|---|---|
| Shell | Maximized startup; 1600 × 900 restored minimum; 16:9 resizing; compact status line; bottom-left overlay Hardware panel. |
| Capture | Shared Camera preview; fixed Single Image, Image Sequence, and Droplet Dataset Capture headings; collapsible bodies only. |
| Label | Dominant Droplet Crop grid; Selected Crop and Classes & Filter right-side disclosures. |
| Sequence Viewer | Frame navigation and image inspection only; no automatic playback surface. |
| Train | All save identity/location before Start; two live plots; automatic save and activation; Retry Save on failure. |
| Model Test | Read-only Active Model context; no local Model selector. |
| Library | Minimal rows, explicit Set Active, collapsible selected-Model detail. |
| Live | Camera preview plus action bar; five disclosure sections; Running expands when sorting starts. |
| Sequence Test | Sequence picker/load/buffer section; editable Processing FPS; optional physical DAQ off by default. |
| Results | Loaded Run center; selectable Runs panel with explicit Load. |
| Settings | Centered Storage, Application Information, and Diagnostics only. |

Design QA must review the minimum restored window, maximized and larger 16:9 layouts, supported scaling, keyboard focus, non-color readiness meaning, all disclosure combinations, startup Camera prompt, minimal Error state, Active Model locks, Live setup/running transition, hit-boundary overlay, Sequence Test memory failure, and Results selected-versus-loaded distinction.

## 1. How to read this document

This document separates three kinds of statements:

- **Approved requirement** — controlled by the Approved Product Model (PM), Information Architecture (IA), or Low-Fidelity Interaction and Application-State Specification (LF).
- **Retained workflow detail** — supplied by the Detailed Workflow Specification (WF) only where it does not conflict with higher authority.
- **Draft visual guidance** — supplied by the Consolidated Product Design Specification (CDS), which remains **Consolidated Draft for Review**. Recommended colors, dimensions, typography, shortcuts, optional inspectors, and mock states are review guidance, not approved product decisions.

The controlling order is:

1. [Approved Product Model](../canonical/product-model.md), especially §§4–20 and D-001 through D-019.
2. [Information Architecture and Screen Inventory](../canonical/information-architecture.md) §§1–6 and [Low-Fidelity Interaction and Application-State Specification](../canonical/interaction-and-state.md) §§1–20.
3. Nonconflicting page-specific requirements in the [Detailed Workflow Specification](../canonical/detailed-workflows.md).
4. [Consolidated Product Design Specification](../design/consolidated-design-draft.md), still a draft for review.
5. [Reuse-First Implementation Plan](../implementation/reuse-first-plan.md) P1-7 and sequencing notes as subordinate engineering guidance only.

The simplest complete interpretation is one persistent desktop shell, ten directly accessible workspaces, one shell-owned Hardware panel, and state changes inside workspaces rather than extra pages, wizards, dashboards, or modes.

## 2. Approved product map

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

There is no Home page, separate Reports page, separate Sort Setup page, managed Setup Profile library, legacy-conversion page, or Training/Model Test history page. Capture's three operations are fixed sections inside one workspace, not navigation children. Live pre-run, running, paused, and result views are states of one workspace. [PM §§4, 7, 17, 19–20](../canonical/product-model.md); [IA §§2–3](../canonical/information-architecture.md); [LF §§1.2, 2.2](../canonical/interaction-and-state.md).

## 3. Application shell

### 3.1 Stable composition

Every page sits in the same desktop shell:

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Camera: <value> | DAQ: <value> | Active Model: <value> | Activity: <value> │
│                                                                  [ Hardware ]│
├────────────────┬──────────────────────────────────────┬──────────────────────┤
│ PRIMARY NAV    │ WORKSPACE / SCIENTIFIC CONTENT       │ OPERATION OR DETAIL  │
│                │                                      │ PANEL                │
│ Data           │ Viewer, crop collection, metrics,    │ Inputs, selection,   │
│ Models         │ model list, Run summary, or Settings │ status, counters,    │
│ Sort           │                                      │ and actions          │
│ Results        │                                      │                      │
│ Settings       │                                      │                      │
└────────────────┴──────────────────────────────────────┴──────────────────────┘
```

The workspace receives the largest flexible area. The right panel is owned by the current workspace and never duplicates Camera or DAQ technical controls. A contextual fault banner appears inside the affected workspace, below its title or Capture headings and above its main regions. The shell has no notification center. [PM §5 and §14](../canonical/product-model.md); [IA §1](../canonical/information-architecture.md); [LF §§2, 4.9](../canonical/interaction-and-state.md); [CDS §§4, 7](../design/consolidated-design-draft.md).

### 3.2 Global header

The header is always visible and contains exactly four read-only state projections plus the Hardware action:

| Area | Approved values |
|---|---|
| Camera | `Unavailable`, `Connected`, `Streaming` |
| DAQ | `Unavailable`, `Ready`, `Active` |
| Active Model | `No Active Model` or the Model Name |
| Current Activity | `Idle`, `Capturing Image`, `Recording Sequence`, `Droplet Dataset Capture`, `Labeling`, `Training`, `Testing Model`, `Testing Sequence`, `Sorting`, `Paused` |

`Starting` and `Stopping` do not add header vocabulary; the operation's activity remains until resources release. Labels and values stay visible; icons and colors are supplemental. Long values may elide only when their full value is otherwise available. [PM §5.1](../canonical/product-model.md); [LF §§2.5–2.7](../canonical/interaction-and-state.md); [WF §8](../canonical/detailed-workflows.md); [CDS §4.4](../design/consolidated-design-draft.md).

### 3.3 Navigation and startup

- Every launch selects `Data > Capture`; all three Capture sections are collapsed.
- The previous workspace is not restored across launches.
- Within the current session, navigation retains workspace selections, drafts, expansion state, and any active/result presentation.
- Leaving a workspace does not stop its operation. Returning restores its current operation controls.
- Other long-running Start actions show **Another operation is active** while the global slot is occupied.
- Read-only or nonconflicting work remains accessible according to resource locks.

[PM §§4, 13 and D-018](../canonical/product-model.md); [LF §§2.2–2.4, 17](../canonical/interaction-and-state.md).

### 3.4 Shared visual language — draft guidance

The CDS proposes a restrained scientific workbench:

- light application shell and panels; dark Camera/image viewers;
- deep navy structure, royal blue actions/focus, limited teal accent, neutral grays;
- semantic colors only for technical state/outcome and always paired with text or a symbol;
- separate class tokens for Class 0, 1, and 2, never reused for focus, selection, or operation state;
- legible sans-serif body text, tabular figures for counters/times/matrices, monospace only for paths and technical identifiers;
- a 4/8/12/16/20/24/32/40 logical-pixel spacing scale, mostly flat panels, restrained rounding, and functional/reduced motion.

The recommended review geometry is a 52–56 px header, 56–64 px compact or 200–224 px expanded navigation, 360–440 px operation panel, 420–520 px Hardware panel, 16–24 px workspace padding, and an 8 px splitter hit target. These are **draft values**, not approved measurements. [CDS §§4.3, 5–6](../design/consolidated-design-draft.md).

## 4. Behavior shared by all workspaces

### 4.1 State language

| Presentation | What it means | What must be visible |
|---|---|---|
| Empty | No required artifact is selected | Factual empty state and one direct Open/Select action |
| Unavailable | A technical prerequisite is unmet | Existing context remains visible; disabled action plus one direct reason |
| Ready | Technical prerequisites are met | Editable setup and enabled primary action |
| Starting | Start was accepted and initialization is underway | Locked inputs, named/busy stage, Stop when safely available |
| Running | Operation owns its resources | Status, progress/counters, locked setup, applicable Pause and Stop |
| Paused | Same operation and identity remain; defined work/output is stopped | `Paused`, stable counters, Resume and Stop, explanation of what continues |
| Stopping | New work/output has stopped and finalization is underway | Noninteractive finalization stage; no second Start |
| Completed | Canonical output finalized | Factual outcome, location/counts, direct next/open actions |
| Interrupted | Operation ended early and recoverable output may exist | One banner with reason, preservation result, and direct actions |
| Failed | Runtime, processing, hardware, or persistence contract failed | One banner with direct reason, preservation result, and retry/inspection actions |

Empty and Unavailable are presentations, not long-running lifecycle states. A clean manual Stop may use a Completed post-operation presentation while the persisted Run status is `Stopped`. [PM §§13–14](../canonical/product-model.md); [LF §§3.4–3.7](../canonical/interaction-and-state.md); [CDS §7](../design/consolidated-design-draft.md).

### 4.2 Actions, validation, and files

- One action is visually primary in each local state and sits at the bottom of the operation/detail panel or equivalent final-action area.
- A disabled primary action has one adjacent reason, not a readiness checklist. General priority is: same operation state; conflicting operation/lock; hardware; artifact; required selection; output writability. Page-specific ordering controls where stated.
- Standard Windows pickers handle Datasets, Sequences, Model Packages, Profiles, output files, and folders.
- A failed open leaves the last valid selection unchanged where possible.
- Unsupported artifacts show **Unsupported OpenDSS v2 schema** and are not partially interpreted.
- Blank optional names become timestamps. Blank Duration means continue until Stop and persists as `null`, not zero.
- Start acceptance immediately locks inputs and prevents duplicate operations/folders.
- Every long-running operation provides Stop; Stop is idempotent and flushes/finalizes what can be represented truthfully.
- A failed write never appears as success.

[LF §4 and §16](../canonical/interaction-and-state.md); [WF §§9–10, 31–34](../canonical/detailed-workflows.md).

### 4.3 Fault banner

The affected workspace shows `Error`. Technical details and hardware/driver codes go to the program log; preserved-output facts and useful Retry/Open Folder actions may remain. The message does not replace a required viewer, Capture heading, or workspace identity. Hardware faults stop new impossible output before the message appears. [PM §14](../canonical/product-model.md); [LF §§4.9, 19](../canonical/interaction-and-state.md); [WF §34](../canonical/detailed-workflows.md).

### 4.4 Resource ownership

Only Image Sequence, Droplet Dataset Capture, Training, Model Test, Sequence Test, and Live occupy the global long-running-operation slot. Single Image reserves Camera/storage momentarily. Label may write an unlocked Dataset. Training and Model Test read-lock their Dataset; Model Test, Live, and Sequence Test read-lock their selected Model Package; Live owns Camera and DAQ; Sequence Test owns DAQ only when Physical DAQ Output is on. Results can edit Notes only for an inactive Run. [PM §13](../canonical/product-model.md); [IA §6](../canonical/information-architecture.md); [LF §17](../canonical/interaction-and-state.md); [WF §9](../canonical/detailed-workflows.md).

## 5. Data > Capture

### 5.1 Exact workspace composition

Capture is one workspace, not three pages:

```text
┌──────────────────────────────────────────────┬────────────────────────────┐
│ SHARED LIVE CAMERA PREVIEW                   │ ▸ Single Image             │
│                                              │ ▸ Image Sequence           │
│                                              │ ▸ Droplet Dataset Capture          │
└──────────────────────────────────────────────┴────────────────────────────┘
```

Approved behavior:

1. The left side is one shared live Camera preview for all three operations.
2. The right side is one operation panel with headings named exactly **Single Image**, **Image Sequence**, and **Droplet Dataset Capture**.
3. No section is expanded the first time Capture opens on launch. Returning during the same session retains its in-session presentation.
4. While idle, headings expand/collapse independently and multiple sections may be open.
5. All three headings remain fixed and visible at all times, including at 1600 × 900 and supported scaling.
6. One expanded body receives all remaining body height below the three headings. Two expanded bodies divide that remaining height equally. Three divide it into equal thirds.
7. Every expanded body scrolls independently; scrolling never moves any heading.
8. Collapsing a section reallocates its released body height equally among the bodies still expanded.
9. During Single Image capture/write, Single Image is forced open and the other two headings are visible but disabled.
10. From accepted Start through finalization for Image Sequence or Droplet Dataset Capture, the active section is forced open and cannot collapse; the other headings remain visible but disabled.
11. After Completed, Interrupted, or Failed, the other headings re-enable and the result section remains expanded until the user manually collapses it.
12. Collapse does not discard that section's in-session draft fields. If focus is inside a body when it collapses, focus returns to its heading.

Draft visual guidance only: CDS §5.8 recommends a dark viewer surface and
aspect-preserving image treatment. Those traits are not approved Capture
behavior. [CDS §5.8](../design/consolidated-design-draft.md)

The preview remains visible when Camera is unavailable and centers the text **Camera unavailable**. Expanded file/name fields stay visible, while Camera-dependent actions show that direct reason. The fault banner sits above the preview/panel split and never replaces the headings. Camera settings exist only in the Hardware panel. [PM §7.1 and D-014](../canonical/product-model.md); [IA §§1.5, 2.1, 3](../canonical/information-architecture.md); [LF §6](../canonical/interaction-and-state.md); [CDS §9.1](../design/consolidated-design-draft.md).

At minimum width, the panel remains at least approximately 320 logical px under the draft guidance, fields become a dense single column, headings remain fixed, and only bodies scroll. The preview keeps the remaining width and a useful image area. [CDS §9.1](../design/consolidated-design-draft.md).

### 5.2 Single Image section

```text
SINGLE IMAGE

File Name       [________________________]  optional; blank uses timestamp
Save Location   [ path________________ ] [ Browse ]

[ Capture Image ]

After success: Saved: <TIFF path>  [ Reveal ]
```

- Requires Camera Streaming, writable image location, and no conflicting Camera/storage owner.
- **Capture Image** creates exactly one full-frame TIFF and no Dataset, Sequence metadata, Model Test output, Run, classification, or DAQ output.
- On activation the button becomes busy, Current Activity becomes `Capturing Image`, the section stays open, and the other headings disable until the frame is written or fails.
- Success keeps the fields and Capture Image available for repetition, shows the saved path inline, and returns focus to Capture Image.
- Failure shows one banner and no saved-path claim.
- Disabled-reason order: **Another operation is active** → **Camera unavailable** → **Output folder is not writable**.
- Presentations: Unavailable, Ready, transient busy, Completed, Failed.

[PM §§7.1, 12](../canonical/product-model.md); [LF §6.4 and §16](../canonical/interaction-and-state.md); [WF §11](../canonical/detailed-workflows.md); [CDS §9.2](../design/consolidated-design-draft.md).

### 5.3 Image Sequence section

Ready body:

```text
IMAGE SEQUENCE
Name              [________________]
Experiment Type   [________________]
Notes             [________________]
Duration          [________________]  optional; continue until Stop
Save Location     [ path_________ ] [ Browse ]

[ Start Recording ]
```

Active/result bodies replace the editable setup inside the same section:

| State | Visible content and actions |
|---|---|
| Starting | Initialization/folder status, locked inputs, Stop when safe |
| Running | `Recording Sequence`, active elapsed time, Frames, **Pause**, **Stop** |
| Paused | `Paused`, frozen active time/count, preview-continues explanation, **Resume**, **Stop** |
| Stopping | New frame writes stopped; queued frames and `sequence.json` finalizing |
| Completed | Final frame count, stop reason, location, **Open in Sequence Viewer**, **Open in Sequence Test**, **Start New Recording** |
| Interrupted/Failed | Banner stating whether `sequence.json`, frames, or only a folder are recoverable, plus valid open/folder actions |

Start creates one folder, numbered TIFF frames in acquisition order, and recoverable/final `sequence.json`. Pause stops TIFF writing and active-recording time while the live preview continues; Resume continues the same folder and numbering. Stop or Duration expiry finalizes the same sequence. It is not a Run and never appears in Results. Disabled-reason order is **Another operation is active** → **Camera unavailable** → **Output folder is not writable**. [PM §§7.1, 8, 12–15](../canonical/product-model.md); [LF §6.5](../canonical/interaction-and-state.md); [WF §12 and §§31–34](../canonical/detailed-workflows.md); [CDS §9.3](../design/consolidated-design-draft.md).

### 5.4 Droplet Dataset Capture section

Ready body:

```text
DROPLET DATASET CAPTURE
Dataset Name      [________________]
Experiment Type   [________________]
Notes             [________________]
Duration          [________________]  optional; continue until Stop
Save Location     [ path_________ ] [ Browse ]

Processing
Fixed qualified detection and Droplet Crop processing.
The effective configuration is recorded with the Dataset.

[ Start Droplet Dataset Capture ]
```

The Processing text is factual, not a settings disclosure. No Active Model, Predicted Class, Class Score, Label, Decision, Hit/Waste, Observed Route, DAQ, detector, or crop control appears.

| State | Visible content and actions |
|---|---|
| Starting | Dataset/folder/writer initialization, locked inputs, Stop when safe |
| Running | `Droplet Dataset Capture`, active elapsed time, Full Frames, Detected Droplets, Droplet Crops, **Pause**, **Stop** |
| Paused | Frozen counts/time, preview-continues explanation, **Resume**, **Stop** |
| Stopping | New writes/detection/crop creation stopped; queues and `dataset.json` finalizing |
| Completed | Final counts, location, **Open in Label**, **Open Folder**, **Start New Droplet Dataset Capture** |
| Interrupted/Failed | Banner distinguishing recoverable Dataset, preserved files only, or no usable output |

Droplet Dataset Capture requires Camera Streaming, loadable fixed qualified processing, writable output, and the global slot; it never requires a model or DAQ. It creates one Dataset containing `dataset.json`, its full-frame sequence, and exactly one 64 × 64 grayscale PNG Droplet Crop per completed detection. Every crop starts Unlabeled. Pause stops full-frame writes, detection, and crop creation while preview continues. Disabled-reason order is **Another operation is active** → **Camera unavailable** → **Processing configuration unavailable** → **Output folder is not writable**. [PM §§7.1, 8, 11–15](../canonical/product-model.md); [LF §6.6](../canonical/interaction-and-state.md); nonconflicting [WF §13 and §§31–34](../canonical/detailed-workflows.md); [CDS §9.4](../design/consolidated-design-draft.md).

## 6. Data > Label

### 6.1 Composition

```text
Dataset: <name/path>                                      [ Open Dataset ]
Filters: [ Class ▼ ] [ State ▼ ]
OPTIONAL FAULT BANNER

┌───────────────────────────────────────────┬──────────────────────────────┐
│ VIRTUALIZED DROPLET CROP COLLECTION       │ SELECTED CROPS INSPECTOR     │
│ [crop] [crop] [crop] [crop]               │ Large preview / Selected: n  │
│ [crop] [crop] [crop] [crop]               │ Class 0 / 1 / 2 actions      │
│                                           │ Skip / Remove / Restore      │
├───────────────────────────────────────────┼──────────────────────────────┤
│ IMAGE COUNTS                              │ CLASSES                      │
│ Per-class, Unlabeled, Skipped, Removed     │ 0 [name] 1 [name] 2 [name] │
│                                           │ [ Use in Train ]             │
└───────────────────────────────────────────┴──────────────────────────────┘
```

The crop collection is visually dominant. Each thumbnail independently communicates crop image, Class ID/Name, crop state, selection, and keyboard focus. Removed uses a subdued overlay plus Removed/X; Skipped has its own factual badge; missing files are not styled as Removed. Large collections are virtualized. [PM §7.2](../canonical/product-model.md); [LF §7](../canonical/interaction-and-state.md); [WF §14](../canonical/detailed-workflows.md); [CDS §10](../design/consolidated-design-draft.md).

### 6.2 Behavior and states

- Empty shows **No Dataset selected** and **Open Dataset**.
- A Dataset without classes shows peer choices `2 Classes` and `3 Classes`; no option is recommended or preselected as scientific advice.
- Class IDs become 0/1 or 0/1/2 and remain immutable once labeling begins; Class Names remain editable.
- Click selects one crop; Ctrl-click toggles; Shift-click extends a range in filtered order.
- Class actions label or relabel; Skip defers; Remove retains the PNG and entry; Restore returns Removed to Unlabeled; Undo reverts the most recent in-session Label edit.
- Only Labeled crops are eligible for Training and Model Test.
- There is no Dataset Save button. Each label, class-name, Skip, Remove, Restore, or Undo command persists atomically. A small `Saving…`, `Saved`, or `Change not saved` line may show factual persistence state.
- When Training or Model Test read-locks this Dataset, content and filters remain inspectable but mutations disable with **Dataset is in use by Training/Model Test**. Another unlocked Dataset may be opened.
- **Use in Train** navigates and preselects the Dataset; it does not start Training.

The inspector may collapse at minimum width behind a labeled restore action, but the crop collection and class actions remain usable. Recommended workspace-scoped shortcuts are `1/2/3`, `S`, `X`, `R`, `Ctrl+Z`, arrows, Shift+arrows, and Space; all are suppressed during text entry and have visible nonshortcut controls. [LF §7](../canonical/interaction-and-state.md); [CDS §§10, 22.6](../design/consolidated-design-draft.md).

## 7. Data > Sequence Viewer

### 7.1 Composition

```text
Data > Sequence Viewer                                  [ Open Sequence ]
OPTIONAL FAULT BANNER

┌──────────────────────────────────────────────────────────────────────┐
│ DARK FULL-FRAME VIEWER                                               │
│                         <current frame>                              │
├──────────────────────────────────────────────────────────────────────┤
│ Frame <current> of <total>                                           │
│ [ Previous ] [ Next ]   Frame [________]                            │
│ [ Zoom - ] [ Zoom + ] [ Fit ] [ 1:1 ]                              │
└──────────────────────────────────────────────────────────────────────┘
```

No permanent right operation panel is required. The image is dominant; optional sequence metadata is secondary and collapsible. [PM §7.3](../canonical/product-model.md); [LF §8](../canonical/interaction-and-state.md); [WF §18](../canonical/detailed-workflows.md); [CDS §11](../design/consolidated-design-draft.md).

### 7.2 Behavior and states

- Empty retains the viewer with **No Image Sequence selected** and **Open Sequence**.
- It opens standalone `sequence.json`, a Dataset sequence, or a Run sequence.
- Previous/Next move one readable frame; direct seek selects a readable frame; zoom/pan never modify files.
- Left/Right move one frame, Shift+Left/Right move 10, Ctrl+Left/Right move 50, Home/End select first/last, plus/minus zoom, F selects Fit, and 1 selects 1:1.
- Missing frames are skipped silently.
- It requires no Camera, DAQ, model, or trainer; owns no global operation slot; emits no DAQ output.
- At minimum width, optional details collapse before frame-navigation controls.

[PM §7.3](../canonical/product-model.md); [LF §8](../canonical/interaction-and-state.md); nonconflicting [WF §18](../canonical/detailed-workflows.md); draft [CDS §§11, 22.7, 23](../design/consolidated-design-draft.md).

## 8. Models > Train

### 8.1 Composition by phase

Ready:

```text
┌───────────────────────────────────────────┬─────────────────────────────┐
│ DATASET SUMMARY                           │ TRAINING SETUP              │
│ Dataset / classes / eligible counts       │ Model Type                 │
│ [ Select Dataset ]                        │ ( ) Faster                 │
│                                           │ ( ) More Accurate          │
│                                           │ Device: <automatic GPU/CPU>│
│                                           │ Split 70 / 15 / 15         │
│                                           │ Seed 1729                  │
│                                           │ [ Start Training ]         │
└───────────────────────────────────────────┴─────────────────────────────┘
```

Ready includes Model Name and Save Location before Start. Running shows progress, timing, automatic device, Training/Validation Loss, and Validation Accuracy. Completion automatically saves the Model Package, makes it Active, then shows overall and per-class results, optional confusion matrix, saved path, and **Open in Model Test**. Save failure shows `Error`, retains temporary artifacts, and offers **Retry Save** without Active confirmation. [PM §7.4 and D-003/D-015](../canonical/product-model.md); [LF §9](../canonical/interaction-and-state.md); nonconflicting [WF §15](../canonical/detailed-workflows.md); [CDS §12](../design/consolidated-design-draft.md).

### 8.2 Behavior and states

- Only Labeled crops are eligible; class/image counts are factual and never create class-balance warnings.
- The only editable model choices are **Faster** and **More Accurate**. There is no Advanced Training Parameters heading, disclosure, dialog, or placeholder.
- Split 70/15/15, seed 1729, and planned/actual compute device are read-only facts. GPU is automatic when compatible; otherwise CPU is a normal fallback, not a warning.
- Running shows Training Loss, Validation Loss, Validation Accuracy, Per-Class Validation Accuracy, Macro F1, elapsed, epoch and overall progress, estimated time, and device as available. Metrics use no pass/fail thresholds.
- Start locks the Dataset and Model Type and occupies the global slot. There is Stop but no Pause.
- Stopping states that processing and temporary output are being finalized.
- Low accuracy, collapse, or one dominant Predicted Class cannot block saving when the technical package is complete.
- Automatic Save requires completed artifacts, a Model Name, and writable destination; it creates `metadata.json`, `checkpoint.pth`, and `model.onnx`, registers the package, and makes it Active.
- Interrupted/Failed training does not claim a normal Model Package unless all required artifacts technically exist and the user explicitly saves them.
- Disabled Start order: operation conflict → Dataset → Labeled crops/readability → Model Type → runtime → output. Save order: completion → Model Name → destination.

At minimum width, metrics may stack and scroll, but status/Stop or the Automatic Save region remains directly reachable. Training is never listed in Results. [PM §§7.4, 11–13](../canonical/product-model.md); [LF §§9, 16–17](../canonical/interaction-and-state.md); [WF §§15, 31–34](../canonical/detailed-workflows.md).

## 9. Models > Model Test

### 9.1 Composition

```text
Ready
┌───────────────────────────────────────────┬─────────────────────────────┐
│ SELECTED ARTIFACTS                        │ TEST SETUP                  │
│ Active Model / classes                    │ Compatibility: <fact>      │
│ Dataset / eligible crops [Select Dataset] │ Device: <automatic GPU/CPU>│
│                                           │ Output Location [____][…]  │
│                                           │ [ Start Model Test ]       │
└───────────────────────────────────────────┴─────────────────────────────┘

Completed
Overall Accuracy; Per-Class Accuracy; 2×2 or 3×3 Confusion Matrix
[ Open Model Test Summary ] [ Open Predictions CSV ] [ Open Output Folder ]
[ Start Another Model Test ]
```

During Running, show actual device, `Processed n of total`, determinate progress, and **Stop Model Test**. [PM §7.5](../canonical/product-model.md); [LF §10](../canonical/interaction-and-state.md); [WF §16](../canonical/detailed-workflows.md); [CDS §13](../design/consolidated-design-draft.md).

### 9.2 Behavior and states

- Model and Dataset selections are local and never alter Active Model.
- The source Dataset used for Training is allowed with no warning.
- Two-class/three-class mismatch blocks Start with the exact factual mismatch; only Labeled crops are processed.
- GPU acceleration and CPU fallback are automatic; GPU absence is never a blocker or warning.
- Start occupies the global slot and read-locks both artifacts. Stop exists; Pause does not.
- Completion writes `model_test_summary.json` and `predictions.csv`, shows neutral Overall/Per-Class Accuracy and a class-labeled confusion matrix, and uses `Class Scores`, never Confidence.
- It creates no Run, appears nowhere in Results, assigns no approval, and has no integrated misclassified-image browser.
- Interrupted/Failed states say whether partial summary/CSV output exists and do not present incomplete metrics as canonical.
- At minimum width, artifact summaries stack above setup; metrics may scroll, but output actions remain reachable.

**Unresolved product-choice detail:** CDS §13.3 says either **Open Predictions CSV** or **Open Model Test Summary** should be the local primary action “according to the approved implementation,” but PM/IA/LF do not select one. Both actions belong in Completed; which one receives primary styling requires user approval. This document does not choose.

[PM §7.5](../canonical/product-model.md); [IA §4.8](../canonical/information-architecture.md); [LF §10](../canonical/interaction-and-state.md); nonconflicting [WF §16](../canonical/detailed-workflows.md); draft [CDS §§13, 23](../design/consolidated-design-draft.md).

## 10. Models > Library

### 10.1 Composition

```text
┌───────────────────────────────────────────┬─────────────────────────────┐
│ MODEL PACKAGES                            │ SELECTED MODEL              │
│ ● <Active model>                          │ Name / Active Yes-No       │
│   <selected model>                        │ Type / classes / names     │
│   <other model>                           │ source / date / location  │
│                                           │ metadata / factual metrics │
│ [ Import Model ]                          │ [ Set Active ]             │
│                                           │ [ Open in Model Test ]     │
│                                           │ [ Export ] [ Duplicate ]   │
│                                           │ [ Rename ] [ Delete ]      │
└───────────────────────────────────────────┴─────────────────────────────┘
```

Selected Model and global Active Model are separate visual states. Selection uses the selection treatment; Active uses a persistent text/marker and the header. Selecting never activates. [PM §7.6](../canonical/product-model.md); [LF §11](../canonical/interaction-and-state.md); [WF §17](../canonical/detailed-workflows.md); [CDS §14](../design/consolidated-design-draft.md).

### 10.2 Behavior and states

- Empty shows **No OpenDSS v2 Model Packages found** and **Import Model**.
- A row/detail shows Model Name/ID, Active state, Model Type, class count/Names, source Dataset, creation date, package location, provenance, and neutral metrics.
- Required actions: Set Active, Import, Export, Duplicate, Rename, Delete, and Open in Model Test.
- Import accepts valid v2 packages only and performs technical checks; there is no legacy conversion or scientific status.
- Duplicate creates a new Model ID; Rename changes only the user-facing name; Delete requires one named confirmation and clears Active Model if deleting it.
- A package used by Model Test, Live, or Sequence Test cannot be mutated. If an operation uses the Active Model, Set Active cannot replace it. Unrelated packages remain manageable.
- At minimum width, the model list stays visible and details may become a full-height inspector. Delete never receives default focus and never occurs from the Delete key without confirmation.

[PM §7.6](../canonical/product-model.md); [IA §4.9](../canonical/information-architecture.md); [LF §11](../canonical/interaction-and-state.md); nonconflicting [WF §17](../canonical/detailed-workflows.md); draft [CDS §§14, 23](../design/consolidated-design-draft.md).

## 11. Sort > Live

### 11.1 One stateful workspace

The left live Camera preview remains continuous. The right panel changes inside the same `Sort > Live` workspace from pre-run configuration to Starting/Running/Paused monitor and then outcome. No separate Setup page exists. [PM §7.7 and D-007](../canonical/product-model.md); [LF §12](../canonical/interaction-and-state.md); nonconflicting [WF §§20–21](../canonical/detailed-workflows.md); [CDS §15](../design/consolidated-design-draft.md).

### 11.2 Pre-run composition

```text
┌───────────────────────────────────────────┬──────────────────────────────┐
│ LIVE CAMERA PREVIEW                       │ LIVE RUN CONFIGURATION       │
│                                           │ Setup Profile               │
│                                           │ [ Open ] [ Save ] [ Save As]│
│                                           │ Run Name / Experiment Type │
│                                           │ Notes / Duration / Location│
│                                           │ Trigger Every Droplet               │
│                                           │ ( ) Class-Based Sorting   │
│                                           │ ( ) Trigger Every Droplet │
│                                           │ Active Model / Hit Class  │
│                                           │ Hit boundary calibration      │
│                                           │ Hit/Waste direction map   │
│                                           │ [ ] Record Full Sequence  │
│                                           │ [ Send Test Pulse ]       │
│                                           │ [ Start Sorting ]         │
└───────────────────────────────────────────┴──────────────────────────────┘
```

The field order is Profile; run information; trigger/routing; output retention; Send Test Pulse; Start. Camera/DAQ settings are never duplicated here.

- Class-Based Sorting requires global Active Model and explicit Hit Class; the largest Class Score determines Predicted Class, and equality with Hit Class determines Hit.
- Trigger Every Droplet is an equal first-class choice, requires no model, hides/disables Hit Class as not used, and makes every detected droplet Decision Hit. If a model exists, classification is still logged without controlling Decision.
- Hit boundary calibration is explicitly `+Y — Downward` or `−Y — Upward`, followed immediately by the text/diagram mapping Hit and Waste. The term Hit Channel never appears.
- Record Full Image Sequence controls only full-frame retention; event crops and event rows are always saved.
- Send Test Pulse is a secondary physical-output action requiring DAQ Ready. It creates no Run/event and is not arming or Emergency Stop.
- Start order: operation conflict → Camera → DAQ → Trigger Every Droplet → Active Model/Hit Class if Class-Based → Hit boundary calibration → output.

[PM §§7.7, 11–13](../canonical/product-model.md); [IA §§4.10, 6](../canonical/information-architecture.md); [LF §§12.2–12.5, 16–17](../canonical/interaction-and-state.md); nonconflicting [WF §§20–21](../canonical/detailed-workflows.md); draft [CDS §§15.5–15.10, 20](../design/consolidated-design-draft.md).

### 11.3 Start and active monitor

Accepted Start immediately locks inputs, creates the Run and recoverable files, snapshots configuration, locks Camera/DAQ/model/output/global slot, closes the Hardware panel, and replaces configuration with the monitor. Starting may name stages such as Run-folder creation or model loading and exposes Stop when safe.

```text
┌───────────────────────────────────────────┬──────────────────────────────┐
│ LIVE CAMERA PREVIEW                       │ LIVE SORTING                 │
│                                           │ Status / elapsed            │
│                                           │ Trigger / model / routing  │
│                                           │ Total Droplets             │
│                                           │ Decision Hit / Waste       │
│                                           │ Observed Hit / Waste /     │
│                                           │ Unresolved                 │
│                                           │ Predicted Classes (model) │
│                                           │ Inference Time / FPS      │
│                                           │ [ Pause ] [ Stop ]        │
└───────────────────────────────────────────┴──────────────────────────────┘
```

Metric hierarchy is status/time, Total Droplets, Decision counts, Observed Route counts including Unresolved, Predicted Class counts if a model exists, performance facts, then immutable configuration. These three scientific concepts stay in separate titled groups. Without a model, model-dependent groups are absent or explicitly not recorded.

[PM §§7.7, 10](../canonical/product-model.md); [IA §§4.11–4.12](../canonical/information-architecture.md); [LF §§12.6–12.9](../canonical/interaction-and-state.md); nonconflicting [WF §21](../canonical/detailed-workflows.md); draft [CDS §§15.11–15.14](../design/consolidated-design-draft.md).

### 11.4 Pause, stop, and results

- Pause keeps the live preview and all locks, freezes active time/counters, and stops inference, new DAQ output, and event finalization. Resume continues the same Run.
- Stopping states that new inference/output/events stopped and persistence queues are flushing.
- Completed/clean Stop shows persisted status/reason, Total Droplets, location, **Open Run Summary**, **Open Run Folder**, and **Start New Run**.
- Interrupted/Failed shows the one banner and only valid preserved-output actions.
- Start New Run returns to pre-run with prior values available for review; it neither duplicates nor automatically starts.
- At minimum width, the preview remains useful and the monitor retains status, elapsed, Total, Decision, Observed Route, Pause/Resume, and Stop; less-important provenance may collapse.

[PM §§7.7, 13–14](../canonical/product-model.md); [IA §§4.12–4.13, 6](../canonical/information-architecture.md); [LF §§12.9–12.13, 17](../canonical/interaction-and-state.md); nonconflicting [WF §21](../canonical/detailed-workflows.md); draft [CDS §§15.14–15.18, 23](../design/consolidated-design-draft.md).

## 12. Sort > Sequence Test

### 12.1 Composition

```text
┌───────────────────────────────────────────┬──────────────────────────────┐
│ SOURCE AND PROCESSING SUMMARY             │ SEQUENCE TEST SETUP          │
│ [Load Sequence] name / first frame        │ Trigger Every Droplet [OFF] │
│ Frames / recorded FPS / Processing FPS    │ Hit Class when applicable  │
│ Available memory / buffer / load status   │ hit boundary calibration   │
│ [Load to Memory]                          │ [ ] Physical DAQ Output    │
│                                           │ DAQ Ready/unavailable/Off │
│                                           │ Save Location             │
│                                           │ [ Start Sequence Test ]   │
└───────────────────────────────────────────┴──────────────────────────────┘
```

There is no live Camera preview and Camera is never required. A static source-frame preview may appear only as a read-only sequence fact. Fixed detector, crop, routing, and timing configuration is factual, not editable. [PM §7.8 and D-006](../canonical/product-model.md); [LF §13](../canonical/interaction-and-state.md); nonconflicting [WF §19](../canonical/detailed-workflows.md); [CDS §16](../design/consolidated-design-draft.md).

### 12.2 Behavior and states

- Physical DAQ Output begins unchecked in a new setup and remains visibly explicit. Checked requires DAQ Ready and locks DAQ during execution; unchecked permits no-DAQ processing and owns no hardware.
- Class-Based requires the compatible Active Model and Hit Class. Trigger Every Droplet may use no Active Model. Another Model must be Set Active in Library first.
- Hit boundary calibration is always required; the source Sequence is read-only; viewer frame navigation is absent; processing follows the configured Processing FPS.
- Start creates a `sequence_test` Run and occupies the global slot.
- Running shows frames processed/total, determinate progress, Events finalized, separate Predicted Class (if model), Decision, and Observed Route Hit/Waste/Unresolved counts, Trigger Every Droplet, physical-output state, model, routing, and **Stop Sequence Test**.
- There is Stop and Stopping, but no Pause.
- Completed or clean Stop provides **Open Run Summary**, **Open Run Folder**, and **Start Another Sequence Test**. Every completed/stopped test appears in Results.
- Interrupted/Failed states report preserved Run data and whether physical output had been active.
- Disabled order: operation conflict → Sequence → schema/readability → Trigger Every Droplet → Model/Hit Class if Class-Based → Hit boundary calibration → DAQ if output on → output location.
- At minimum width, source summary stacks above setup and Stop remains pinned/reachable.

[PM §§7.8, 11–14](../canonical/product-model.md); [IA §§4.14, 6](../canonical/information-architecture.md); [LF §§13.2–13.6, 16–17](../canonical/interaction-and-state.md); nonconflicting [WF §19](../canonical/detailed-workflows.md); draft [CDS §§16.4–16.9, 23](../design/consolidated-design-draft.md).

## 13. Results > Runs

Results contains only Live Sorting and Sequence Test Runs. It has two presentations inside the same workspace. [PM §7.9 and D-013](../canonical/product-model.md); [LF §14](../canonical/interaction-and-state.md); nonconflicting [WF §22](../canonical/detailed-workflows.md); [CDS §17](../design/consolidated-design-draft.md).

### 13.1 Runs list

```text
Run Name | Operation | Started | Duration | Status | Model | Total
------------------------------------------------------------------
<row>
<row>

[ Load selected Run ]
```

- Empty states **No Runs found**; before selection the reason is **No Run selected**.
- Status values are factual `Completed`, `Stopped`, `Interrupted`, or `Failed`; interrupted/failed Runs remain visible.
- An unreadable row remains identifiable from available path/name facts and displays the direct reason.
- No capture, Training, or Model Test entries appear.
- Up/Down moves row focus. Selection does not load; the bottom **Load** button explicitly replaces the center Run while the right-side Runs panel remains available.

### 13.2 Selected Run

```text
┌───────────────────────────────────────────┬──────────────────────────────┐
│ RUN SUMMARY                               │ FILES AND NOTES              │
│ Identity / operation / status / timing    │ [ Open Droplet Log ]        │
│ Model and routing snapshot               │ [ Open Run Folder ]         │
│ Hardware/fixed-processing provenance     │ [ Open Droplet Crop ]       │
│ Total Droplets                           │ [ Open Saved Sequence ]     │
│ Predicted Class counts                   │ Notes [Edit/Save/Cancel]    │
│ Decision Hit / Waste                     │                              │
│ Observed Hit / Waste / Unresolved        │                              │
│ Decision vs. Observed Route matrix       │                              │
└───────────────────────────────────────────┴──────────────────────────────┘
```

The summary shows status/timestamps/duration/reason/location; Experiment Type/Notes; model identity/checksum/classes when present; Trigger Every Droplet, Hit Class if applicable, Hit boundary calibration; Physical DAQ Output for Sequence Test; Camera/DAQ/fixed-processing configuration/version; OpenDSS/schema versions where recorded; and the separate count groups.

The matrix is exactly Decision versus Observed Route and includes an Unresolved column:

| Decision | Observed Hit | Observed Waste | Unresolved |
|---|---:|---:|---:|
| Hit | count | count | count |
| Waste | count | count | count |

It is never called ground truth, actual destination, or routing accuracy. Open Droplet Crop uses a standard picker rooted in the crop folder; there is no integrated event browser. Open Saved Sequence appears only when present and hands off to Sequence Viewer. Notes enter explicit edit mode; Save atomically changes only Notes; all historical events, counts, configuration, and images remain immutable.

## 14. Settings

### 14.1 Exact reduced composition

```text
Settings

STORAGE
Default Data Root: <path>
[ Choose Default Data Root ] [ Open Data Root ]

APPLICATION INFORMATION
OpenDSS Version / Schema Versions / Runtime Availability
Camera Driver / DAQ Driver / GPU Environment Availability

DIAGNOSTICS
Diagnostic Folder: <path>
[ Open Diagnostic Folder ]
```

There is no right operation panel requirement; a centered column or grouped panels is appropriate. Settings never contains Camera/DAQ fields, detector/crop/routing/timing/training controls, profiles, cloud/account/telemetry/update, or legacy migration. [PM §7.10 and D-017](../canonical/product-model.md); [LF §15](../canonical/interaction-and-state.md); nonconflicting [WF §§10, 23](../canonical/detailed-workflows.md); [CDS §18](../design/consolidated-design-draft.md).

### 14.2 Behavior

- The default root is `%USERPROFILE%\Documents\OpenDropletSortingSuite` unless changed.
- Choose opens a Windows folder picker; the new root becomes authoritative only after writability and preference persistence succeed. Failure keeps the prior root and says so.
- Root changes affect future operations only and never relocate artifacts or alter an active operation snapshot.
- Open Data Root and Open Diagnostic Folder use Windows Explorer.
- Application/runtime/driver facts are read-only, selectable/copyable where practical, and are not styled as disabled.
- Groups stack at minimum width. Ready and Failed are the applicable presentations.

## 15. Shared Hardware panel

The header's **Hardware** action opens a shell-owned bottom-left overlay while the header remains visible:

```text
HARDWARE
CAMERA
Status: <value>
<only qualified supported controls>

DAQ
Status: <value>
Output Channel [▼]
<only qualified supported controls>

[ Close ]
```

There is no global Apply. Selectors/toggles apply on change; validated text/numbers apply on Enter or focus commit. Success becomes authoritative everywhere. Rejection restores the last applied value and shows an inline reason. Unsupported properties are absent, not disabled placeholders. [PM §5.2 and D-016](../canonical/product-model.md); [IA §§1.6–1.7](../canonical/information-architecture.md); [LF §5](../canonical/interaction-and-state.md); [CDS §19](../design/consolidated-design-draft.md).

| Condition | Appearance and behavior |
|---|---|
| Idle/available | Device section visible and editable; valid edits apply immediately |
| Unavailable | Section visible, controls disabled, factual **Camera/DAQ unavailable**; other device remains independently usable |
| Owned by non-Live operation | Panel may open; owned section is read-only and names the owner; other section remains usable |
| Live Starting/Running/Paused/Stopping | Entire panel closes; Hardware action disables; both devices remain locked |
| Sequence Test, DAQ on | DAQ section locked; Camera independently usable |
| Sequence Test, DAQ off | Both sections independently usable, though another long operation cannot start |

The panel never contains detector, crop, routing, internal timing, training, software arming, Hit Class, Hit boundary calibration, or Profile-library controls. DAQ Output Channel remains distinct from Hit boundary calibration. Focus is contained while open; Escape or Close returns focus to Hardware. Forced Live closure moves focus to Live status. At minimum size the panel scrolls internally, may occupy up to 44% of content width under draft guidance, and never hides the header.

**Unresolved content boundary:** the canonical documents intentionally do not enumerate the exact Camera and DAQ property fields. The panel must show only properties supported by the qualified integrated adapters. Field inventory requires separate hardware/source evidence and product review; placeholder fields must not be invented.

## 16. Setup Profile surface inside Live

Setup Profile is not a page. It is a compact file-control group at the top of Live pre-run:

```text
SETUP PROFILE
<No profile loaded> or <filename.json>
<Loaded / Modified / unapplied-values summary>
[ Open ] [ Save ] [ Save As ]
```

It is one ordinary v2 JSON file. There is no managed list, Import, Export, Delete, archive, or migration workflow. A Profile may contain current applied Camera/DAQ settings, Active Model reference, Trigger Every Droplet, Hit Class, Hit boundary calibration, Record Full Image Sequence, Run Name, and default Save Location. It never contains editable fixed-processing or training values. Notes and Experiment Type remain run-specific unless a future approved contract changes that boundary. [PM §9 and D-009](../canonical/product-model.md); [LF §12.3](../canonical/interaction-and-state.md); nonconflicting [WF §§20, 30](../canonical/detailed-workflows.md); [CDS §20](../design/consolidated-design-draft.md).

- Open is available only in Live pre-run and never starts sorting.
- Valid idle hardware values apply immediately. Unavailable/locked hardware remains unchanged and is named as unapplied in a read-only summary; no second hardware editing surface appears.
- A valid unlocked referenced model may become Active. A missing model is shown factually; no substitute is selected; other readable values load; Class-Based stays unavailable while Trigger Every Droplet may proceed.
- Unsupported schema loads nothing and preserves the current valid configuration. Supported partial-load cases distinguish `Loaded`, `Partially loaded`, and `Not loaded` in text.
- Save uses the current path or behaves as Save As. Save/Save As snapshot authoritative applied hardware and approved run selections, never invalid drafts.
- `Modified` means file-covered values differ from the last loaded/saved snapshot; it is not a scientific or managed-library state.

## 17. Contextual handoffs

Each link only navigates and preselects; it never starts an operation, creates a project, or forms a wizard.

| Source | Link | Destination and effect |
|---|---|---|
| Droplet Dataset Capture result | **Open in Label** | Opens Label with recoverable/completed Dataset selected |
| Label | **Use in Train** | Opens Train with Dataset selected; Model Type remains explicit |
| Train after model save | **Open in Model Test** | Selects new Model Package; Model Test remains optional |
| Library | **Open in Model Test** | Selects model without changing Active Model |
| Image Sequence result | **Open in Sequence Viewer** | Selects `sequence.json` and first readable frame |
| Image Sequence result | **Open in Sequence Test** | Selects sequence; all routing/output choices remain explicit |
| Live/Sequence Test result | **Open Run Summary** | Opens Results with recoverable/finalized Run selected |
| Selected Run with sequence | **Open Saved Sequence** | Opens Sequence Viewer with that sequence selected |

Focus moves to the destination title or artifact summary, never directly to Start, Save, Delete, or another consequential action. [IA §5](../canonical/information-architecture.md); [LF §18](../canonical/interaction-and-state.md); [CDS §21](../design/consolidated-design-draft.md).

## 18. Keyboard, focus, minimum window, scaling, and accessibility

### 18.1 Keyboard and focus

- Every core action has a keyboard path; hover, drag, and context menus are never the sole path.
- Default region order is header/Hardware → selected navigation → workspace title/utilities → primary content → operation/detail panel → banner actions.
- `F6`/`Shift+F6` should cycle major regions without changing workspace or selection.
- Focus and selection use different visual channels. Routine counters, playback, and polling never steal focus.
- Start acceptance moves focus to Starting status; Running to operation status; Pause to Paused status with Resume in the same position; Stop to Stopping; completion to outcome then contextual action; fault to the banner once.
- Escape closes overlays/edits where safe; it never Stops an operation. No unmodified global key starts or stops physical output.
- Splitters have pointer, keyboard, Collapse/Expand, and practical reset paths; collapse retains selection/scroll state and moves focus to the restore control.

[CDS §22](../design/consolidated-design-draft.md); supporting [WF §§38–43](../canonical/detailed-workflows.md).

### 18.2 Supported desktop matrix

| Reference | Logical window | Required interpretation |
|---|---:|---|
| Minimum | 1600 × 900 | All workflows operable; compact navigation/optional-inspector collapse allowed; Capture headings, active operation panel, primary actions, counters, and reasons stay visible |
| Standard | 1600 × 900 | Balanced main review reference |
| Wide | 1920 × 1080 | More viewer space or persistent detail, never new features |

Supported Windows scaling is 100%, 125%, 150%, and 200%. The shell and task order remain the same; there is no mobile layout. Optional metadata collapses first, then optional inspectors, then navigation compacts, then metric columns stack. The active operation panel, viewer/crop collection, fault banner, and primary action do not auto-hide. [CDS §23](../design/consolidated-design-draft.md).

### 18.3 Accessibility baseline

- Normal text contrast at least 4.5:1; meaningful controls/focus at least 3:1; equivalent contrast on dark viewers.
- Minimum pointer target 24 × 24 logical px; 32–40 preferred; primary operations 36–44 or larger.
- No state, class, focus, selection, Active Model, or DAQ-output meaning uses color alone.
- Titles/groups expose semantic hierarchy; status names combine label/value; metrics associate label/value; tables expose row/column headers; thumbnails expose crop ID/Class/state/selection.
- At 200% scaling, required text, primary actions, status, headings, and disabled reasons do not clip. Paths may elide only with full access elsewhere.
- Reduced motion removes nonessential panel/panel animation; sequence frame navigation remains user-controlled core content.

## 19. Qt Design Studio review and handoff boundary

This document does not authorize implementation. When a later visual baseline is authorized, the CDS draft requires designer-editable forms to own layout, visual hierarchy, components, tokens, visual states, and preview hooks only. Wrappers own presentational interaction/focus; view models expose authoritative projections/commands; domain services own hardware, files, locks, persistence, and science. Mocks must make every applicable state, fault, long-value, window size, and 100/125/150/200% scale previewable without hardware or production files. [CDS §24](../design/consolidated-design-draft.md); [PM §18](../canonical/product-model.md); [WF §§73–78](../canonical/detailed-workflows.md).

The visual system should centralize tokens and reuse shared components, but the exact suggested directory tree in CDS §24.4 is illustrative, not an approved repository structure. Qt Design Studio state selectors and mock controls are design-only and must never appear in the product.

## 20. Source conflicts and unresolved decisions

### 20.1 Explicitly superseded Detailed Workflow requirements

These are genuine textual conflicts. The interpretation above follows the named higher authority and does not silently merge the older requirement.

| Conflict | Lower-authority text | Controlling interpretation |
|---|---|---|
| Droplet Dataset Capture settings | WF §13.5 calls detection/crop settings user-controlled | PM §§7.1, 11, 20 and D-014/D-016 make detection/crop processing fixed and remove these controls |
| Training Advanced Parameters | WF §§15.4, 15.8 require/allow an Advanced Parameters surface | PM §§7.4, 11, 17, 20 and D-015 prohibit editable hyperparameters and the Advanced surface |
| Sequence Test placement | WF §19.2 places it under Models | PM §§4, 7.8, 20 and D-006 place it under Sort |
| Observed Route | WF §§19.8, 21.9, 22.6–22.7, 28–29 allow only Hit/Waste and omit Unresolved | PM §§7.7–7.9, 10, 19–20 and D-005 require Hit/Waste/Unresolved everywhere, including counters, matrix, CSV, and Run Summary |
| Sort Setup | WF §§20–21 define separate Setup and Live workspaces | PM §§4, 7.7, 20 and D-007 make them pre-run/running states of one Live workspace |
| Technical settings placement | WF §§20.4 and 23 place Camera/DAQ and detector/crop/routing/timing controls in setup/settings | PM §§5, 7.10, 11, 20 and D-016/D-017 put only Camera/DAQ technical settings in the bottom Hardware panel and keep processing fixed |
| Setup Profile management/content | WF §§20.12 and 30 define Select/Import/Export/Delete and include detector/crop/timing | PM §§9, 11, 20 and D-009 require ordinary files with Open/Save/Save As and narrowed approved fields |
| Settings scope | WF §23 recommends Camera, Detection, Crops, DAQ, Sorting, Training, Storage, Advanced | PM §7.10, §11, §20 and D-017 reduce Settings to Storage, Application Information, Diagnostics |

### 20.2 Subordinate planning alignment

The Reuse-First Plan and [current-slice.md](../implementation/current-slice.md) now align: the Single Image-only Capture composition is superseded, this page-composition review is the current gate, and no implementation is authorized. A corrected Qt Design Studio visual baseline requires a separate work order after user approval.

### 20.3 Decisions still requiring user confirmation

1. Which Completed-state action in Model Test receives primary styling: **Open Model Test Summary** or **Open Predictions CSV**. Both must exist; canonical sources do not choose the primary.
2. Exact qualified Camera and DAQ field inventory in the Hardware panel. Product sources define ownership and behavior but intentionally do not enumerate adapter-supported properties.
3. Draft visual choices such as exact color values, font/fallback, navigation compact/expanded policy, panel widths, icon set, animation duration, optional inspectors, optional filtering/search aids, and optional nonpersistent overlays remain review choices, not approved product decisions.

No other product-structure decision is needed to understand the approved pages. In particular, Capture composition, startup, state transitions, resource locks, fixed/editable boundaries, navigation, Results scope, and the Unresolved event meaning are already controlled by D-001 through D-019.

## 21. User validation checklist

Before any further Qt Design Studio or production implementation, confirm that:

- the ten-workspace navigation and absence of Home/Reports/Sort Setup are correct;
- the shell/header/panel structure is correct;
- Capture's one preview, three fixed headings, exact equal-height body allocation, independent scrolling, locking, and retained results are correct;
- every page's visible fields, actions, states, and contextual handoffs are correct;
- Predicted Class, Decision, and Observed Route including Unresolved are presented separately;
- only Camera/DAQ technical settings are editable and only in the panel;
- Settings and Setup Profile scope are correct;
- minimum-window, scaling, focus, and accessibility behavior are acceptable;
- the three unresolved review items in §20.3 are decided or explicitly deferred;
- the CDS visual recommendations are either accepted as the visual baseline or amended before forms are changed.
