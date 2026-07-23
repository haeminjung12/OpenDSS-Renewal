# OpenDSS v2 UI/UX Design Amendment

**Document ID:** ODSS-UXA-001  
**Version:** 1.0  
**Status:** Product-owner approved design amendment  
**Date:** July 23, 2026  
**Primary platform:** Windows 11  
**Product:** Open Droplet Sorting Suite (OpenDSS)

---

## 1. Purpose and authority

This document records the latest approved UI/UX and interaction decisions for OpenDSS v2.

It supplements the following existing sources:

1. `OpenDSS_v2_Approved_Product_Model.md`
2. `OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md`
3. `OpenDSS_v2_Consolidated_Product_Design_Specification.md`
4. `OpenDSS_Detailed_User_Workflow_Specification.md`
5. `Qt_Design_Studio_Workflow_Adoption.md`

Where this amendment conflicts with a prior UI, layout, naming, or interaction requirement, **this amendment controls**.

Decisions not explicitly changed here remain governed by the existing approved product model and downstream specifications.

This amendment is intentionally limited to the changes required to begin visual design and implementation immediately.

---

# 2. Application window and shell

## 2.1 Startup and resizing

OpenDSS shall:

- launch maximized;
- allow the user to restore the window;
- enforce a minimum logical window size of:

```text
1600 × 900
```

- preserve a **16:9 window aspect ratio** during manual resizing;
- prevent resizing below 1600 × 900;
- retain Minimize, Maximize/Restore, and Close behavior.

The restored window may be enlarged beyond 1600 × 900 only while retaining the 16:9 ratio.

## 2.2 Global status header

The global status header shall remain one compact horizontal line.

It shall show:

```text
Camera | DAQ | Active Model | Current Activity
```

Each status item shall use:

- an icon;
- a text label;
- a current value;
- readiness color.

Readiness shall not be communicated by color alone.

Representative treatment:

```text
Camera       Streaming
DAQ          Ready
Active Model DropletNet-04
Activity     Idle
```

## 2.3 Primary navigation

The approved primary navigation remains:

```text
Data
Models
Sort
Results
Settings
```

No Home screen shall be added.

## 2.4 Hardware panel

The previous right-side Hardware drawer is replaced by a **bottom-left overlay Hardware panel**.

The Hardware panel shall:

- open and close through a visible arrow control;
- overlay the lower-left portion of the workspace;
- not push the workspace upward;
- not span the entire application width;
- remain visually subordinate to the active workspace;
- retain Camera and DAQ as separate sections;
- close or lock according to operation ownership rules.

Its exact width and height shall be controlled through shared design tokens.

---

# 3. Startup Camera behavior

On application startup:

1. OpenDSS attempts to connect to the configured Camera automatically.
2. The application opens at `Data > Capture`.
3. If no Camera is available, OpenDSS shows:

```text
Camera unavailable. Continue?

[Yes] [No]
```

Behavior:

- **Yes** continues into OpenDSS without a Camera.
- **No** closes the application.
- The popup shall not repeat during the same session.
- Later Camera failures use the standard minimal error treatment.

The Camera device may be connected while its stream is stopped.

A user-facing **Start Camera** control starts or stops the live Camera stream.

---

# 4. Error presentation

Normal user-facing errors shall be deliberately minimal.

The primary visible message is:

```text
Error
```

Rules:

- no stack trace;
- no driver error code;
- no exception message;
- no technical diagnostic text;
- no detailed recovery narrative in the normal workspace;
- detailed information is written to the program log;
- the user may access logs through `Settings > Diagnostics`;
- success shall never be shown after a failed action;
- saved paths shall appear only after a confirmed successful save;
- contextual actions such as Retry or Open Folder may appear where useful.

---

# 5. Data > Capture

## 5.1 Workspace structure

Capture retains:

- one central live Camera preview;
- one right-side operation panel;
- three independently collapsible sections.

The entire Capture panel remains visible. Only the section bodies collapse.

The three sections are renamed and ordered as:

```text
Single Image
Image Sequence
Droplet Dataset Capture
```

All three headings remain visible.

The name **Droplet Dataset Capture** replaces **Dataset Capture** throughout:

- UI labels;
- state text;
- activity text;
- errors;
- contextual links;
- documentation;
- mock scenarios;
- acceptance evidence.

## 5.2 Collapsible behavior

- Sections collapse independently.
- Multiple sections may be expanded while idle.
- During an active operation, the owning section remains expanded.
- Other operation headings remain visible but disabled.
- The active/result section may remain expanded after completion until manually collapsed.

## 5.3 Layout

```text
┌──────────────────────────────────────┬─────────────────────────────┐
│                                      │ Single Image                │
│                                      ├─────────────────────────────┤
│          CAMERA PREVIEW              │ Image Sequence              │
│                                      ├─────────────────────────────┤
│                                      │ Droplet Dataset Capture     │
└──────────────────────────────────────┴─────────────────────────────┘
```

---

# 6. Data > Label

## 6.1 Layout

The central Droplet Crop grid shall dominate the workspace.

The right panel uses the shared 390 px expanded-width token and collapses as one outer panel. It contains, in order:

```text
Load Dataset
Dataset Summary
Label
Filter
Save As
```

**Load Dataset** is a static, always-expanded card/header. **Save As** is placed at the bottom-right.

## 6.2 Dataset Summary

Dataset Summary shows total and labeled counts plus a two-or-three-class selection. The selection remains available after initial setup, including for configured Datasets. Switching must preserve label consistency and must not silently reassign or discard existing labels.

## 6.3 Label and Filter

Label contains the selected-crop preview and exactly:

- Class 0;
- Class 1;
- Class 2;
- Exclude;
- Undo;
- Previous;
- Next.

Class 0, Class 1, and Class 2 remain visible. Class 2 is disabled for a two-class Dataset. Skip, Remove, Restore, and other former Label-side actions are absent.

Filter contains class list/count filters and Excluded and Unreviewed when applicable.

Dataset Save As creates an independent copy and makes it the current loaded Dataset. It is not version history. Ordinary label changes continue saving to the current Dataset under the existing persistence contract.

## 6.4 Droplet Crop visual states

Class identity uses three distinct high-contrast border colors.

Approved direction:

- **Class 0:** blue;
- **Class 1:** orange;
- **Class 2:** purple;
- **Unreviewed:** no class border;
- **Excluded:** gray overlay with an X.

Red and green shall not be used for Class identity.

Unreviewed and Excluded shall remain visibly distinct.

Selection, keyboard focus, Class identity, and crop state shall use separate visual treatments.

---

# 7. Data > Sequence Viewer

## 7.1 Naming

`Sequence Player` is renamed:

```text
Sequence Viewer
```

The rename applies throughout navigation, contextual links, documentation, mock states, and acceptance criteria.

## 7.2 Interaction model

Sequence Viewer is a frame-navigation workspace rather than an automatic playback workspace.

Remove:

- Play;
- Pause;
- automatic frame progression;
- Playback Speed;
- playback lifecycle states.

Retain:

- current frame;
- total frames;
- Previous;
- Next;
- direct frame seek;
- zoom;
- pan;
- Fit;
- 1:1.

## 7.3 Keyboard controls

```text
Left / Right                 Previous / next frame
Shift + Left / Right         Move 10 frames
Ctrl + Left / Right          Move 50 frames
Home / End                   First / last frame
+ / -                        Zoom in / out
F                            Fit
1                            1:1
```

If a referenced frame is missing, Sequence Viewer silently skips it and continues to the next readable frame.

No missing-frame popup or visible skip summary is required.

---

# 8. Models > Train

## 8.1 Pre-training inputs

The user enters the following before Training begins:

- Dataset;
- Model Type:
  - Faster;
  - More Accurate;
- Model Name;
- Save Location.

## 8.2 Mandatory save behavior

The approved flow is:

```text
Select Dataset
→ Select Faster or More Accurate
→ Enter Model Name
→ Select Save Location
→ Start Training
→ Training completes
→ Model Package saves automatically
→ Model becomes Active
```

There is no normal completed-but-unsaved state.

If final saving fails:

- show `Error`;
- retain the completed temporary Training artifacts;
- allow Retry Save;
- do not make the Model Active until saving succeeds;
- log technical details.

## 8.3 Live Training presentation

Dataset Summary remains a main white region. A separate main white Results region sits below it.

During Training, show:

- elapsed time;
- epoch progress;
- overall progress;
- Training Loss;
- Validation Loss;
- Validation Accuracy;
- estimated remaining time;
- automatic execution device.

Two minimal plots update live:

1. Training Loss and Validation Loss;
2. Validation Accuracy.

## 8.4 Completion presentation

After successful completion and saving, the separate Results region shows:

- final overall-results table;
- final per-class-results table;
- confusion matrix when generated;
- saved Model path;
- Active Model confirmation.

The workspace shall not become a general analytics dashboard.

---

# 9. Active Model policy

## 9.1 Model Test

`Models > Model Test` always uses the current **Active Model**.

There is no local Model selector.

Dataset Summary remains a main white region. A separate main white Results region below contains the approved metrics, confusion matrix, and prediction summaries without adding new data semantics.

To test another Model:

```text
Models > Library
→ Set Active
→ Models > Model Test
```

## 9.2 Sequence Test

`Sort > Sequence Test` also always uses the current **Active Model** when a Model is required or available.

There is no local Model selector.

To test another Model:

```text
Models > Library
→ Set Active
→ Sort > Sequence Test
```

## 9.3 Model locking

The Active Model cannot be replaced, renamed, deleted, or otherwise mutated while Model Test, Live, or Sequence Test is using it.

---

# 10. Models > Library

## 10.1 Model list row

Each row displays only:

- a green check icon on the left when the Model is Active;
- Model Name in dark bold text;
- Model Type beneath it in smaller, lighter text.

No other metadata is shown in the list row.

The Active check icon shall also expose an accessible `Active Model` label.

## 10.2 Selection

- A selected Model uses a darker background than the list surface.
- Selecting a Model does not make it Active.
- `Set Active` remains an explicit action.

## 10.3 Selected Model panel

Selected Model information appears in one collapsible panel.

It contains:

- Model Name;
- Active state;
- trained date;
- source Dataset;
- Model Type;
- classes;
- Training results;
- package location;
- Set Active;
- Open in Model Test;
- Export;
- Duplicate;
- Rename;
- Delete.

The panel remains minimal and excludes unnecessary package internals from its default presentation.

---

# 11. Sort > Live

## 11.1 Layout

Live uses:

- central Camera preview;
- controls directly below the Camera preview;
- one right-side panel composed of collapsible sections.

Recommended right-panel sections:

```text
Setup Profile
Run Information
Trigger & Timing
Output & Recording
Running
```

Before sorting, `Running` remains collapsed.

When sorting begins:

1. all setup sections collapse;
2. `Running` expands;
3. setup values become read-only;
4. Camera preview remains visible;
5. Running controls remain available.

## 11.2 Camera action bar

Below the Camera preview:

```text
[Start Camera] [Start Sorting]
```

`Start Camera` starts or stops Camera streaming.

`Start Sorting` starts a Run when required technical prerequisites are satisfied.

## 11.3 Trigger & Timing section

Contains:

- Active Model;
- Hit Class;
- Trigger Every Droplet ON/OFF;
- DAQ Output ON/OFF;
- Send Test Pulse;
- Hit boundary calibration.

## 11.4 Trigger Every Droplet

Trigger Every Droplet is an independent ON/OFF toggle.

```text
OFF
→ Class-Based Sorting
→ Active Model and Hit Class control Decision

ON
→ every detected droplet produces Decision Hit
→ classification may still be logged
```

No separate Trigger Mode segmented control is required.

## 11.5 DAQ Output ON/OFF

DAQ Output is an explicit operational ON/OFF control.

When **DAQ Output is OFF**:

- Start Sorting remains enabled if all non-DAQ prerequisites are satisfied;
- Camera processing continues;
- droplet detection continues;
- classification continues when applicable;
- trajectory tracking continues;
- event logging continues;
- Run creation and persistence continue;
- no physical DAQ output is issued.

When **DAQ Output is ON**:

- DAQ Ready is required;
- physical output follows the selected Decision behavior.

This control is not presented as a safety-rated Emergency Stop.

## 11.6 Send Test Pulse

Send Test Pulse:

- requires DAQ Ready;
- uses current applied DAQ settings;
- produces one output pulse;
- creates no Run;
- creates no Droplet Log event;
- appears inside Trigger & Timing.

## 11.7 Running section

Contains:

- operation status;
- elapsed time;
- Total Droplets;
- Predicted Class counts;
- Decision Hit;
- Decision Waste;
- Observed Hit;
- Observed Waste;
- Unresolved;
- Camera FPS;
- Inference Time;
- Pause/Resume;
- Stop.

Pause retains the same Run and stops:

- inference;
- new DAQ output;
- new event finalization.

Camera preview remains active.

---

# 12. Hit boundary calibration

## 12.1 Interaction

Before Start:

1. the user clicks one point in the Camera image;
2. a horizontal line begins at the selected point;
3. the line extends to the right edge of the displayed image;
4. the user selects:
   - `Top is Hit`; or
   - `Bottom is Hit`.

The line is always horizontal.

No additional direction arrow is required.

## 12.2 Mapping

```text
Top is Hit
→ Hit = −Y
→ Waste = +Y
```

```text
Bottom is Hit
→ Hit = +Y
→ Waste = −Y
```

## 12.3 Functional effect

The boundary affects **Observed Route only**.

It does not change:

- Predicted Class;
- Hit Class;
- Decision;
- physical DAQ output behavior.

## 12.4 Persistence

The calibration is:

- saved in the Setup Profile;
- snapshotted in the Run Summary;
- visible as a nonpersistent Camera overlay;
- editable only before Start.

Persist at least:

```text
boundary_y
hit_side = top | bottom
image_width
image_height
```

---

# 13. Sort > Sequence Test

## 13.1 Shared structure

Sequence Test shall reuse the same collapsible-panel language as Live while removing Camera-specific controls.

Add a dedicated:

```text
Sequence Test
```

section.

## 13.2 Sequence Test section

Contains:

- Load Sequence;
- selected sequence name;
- first-frame preview;
- frame count;
- recorded FPS;
- Processing FPS;
- available memory;
- selected memory-buffer size;
- Load to Memory;
- load status;
- Start;
- Stop.

## 13.3 Supported input

Sequence Test accepts only a valid OpenDSS Image Sequence folder containing:

```text
sequence.json
```

It does not import an arbitrary folder of TIFF images.

## 13.4 Custom sequence picker

The sequence-selection dialog shall display:

- sequence folders;
- first-frame thumbnails;
- sequence name;
- frame count;
- recorded FPS;
- sequence status.

The user selects the sequence through this visual picker rather than a generic folder-only dialog.

## 13.5 Processing FPS

Processing FPS:

- defaults to the recorded FPS in `sequence.json`;
- is user-editable;
- controls how quickly the processing pipeline reads frames;
- is not merely a viewer setting;
- is recorded in the Run Summary;
- displays actual achieved processing FPS while running.

When physical DAQ output is enabled, DAQ commands follow the processing schedule produced by this FPS setting.

## 13.6 Memory loading

Sequence Test uses a bounded memory buffer.

Flow:

```text
Select sequence
→ show available memory
→ calculate or select buffer size
→ Load to Memory
→ allocate bounded buffer
→ enable Start after successful load
```

If sufficient memory is not available:

- show `Error`;
- cancel the load;
- keep Start disabled;
- write details to the program log.

If the complete sequence exceeds the buffer, the application refills the buffer during processing.

## 13.7 Physical DAQ Output

Use:

```text
☐ Physical DAQ Output
```

Physical DAQ Output is **off by default**.

No additional explanatory paragraph is required.

When off, Sequence Test may process and create a Run without DAQ hardware.

---

# 14. Results > Runs

## 14.1 Layout

The main center workspace shows the currently loaded Run details.

The right-side collapsible panel contains:

```text
Runs
├── Run list
└── Load button
```

The Load button remains at the bottom of the panel.

Selecting a Run row does not immediately replace the center content.

The user:

1. selects a Run;
2. selects Load;
3. the Run detail is displayed in the center workspace.

## 14.2 Center Run detail

Display:

- Run identity;
- operation type;
- status;
- key counts;
- Predicted Class group;
- Decision group;
- Observed Route group;
- Decision-versus-Observed Route matrix;
- Notes;
- file actions;
- collapsed provenance.

Do not add:

- first-class charts;
- an integrated event-by-event browser;
- Training or Model Test history.

---

# 15. Settings

Settings uses a centered content column.

It retains only:

```text
Storage
Application Information
Diagnostics
Visuals
```

Camera and DAQ controls do not appear in Settings.

Visuals contains only application-wide **Text Size**, selectable from **80%** through **200%**, default **100%**. No other setting is added.

---

# 16. Bottom Hardware panel

## 16.1 Camera section

Show:

- Status;
- Device;
- Resolution preset;
- Custom resolution;
- Width and Height when Custom is selected;
- Bit Depth, default 8-bit;
- Exposure;
- Readout mode, default Fastest;
- LUT controls.

Use the term **Bit Depth**, not Bit Rate.

LUT controls affect preview presentation only.

LUT does not modify:

- saved TIFF values;
- Droplet Crop generation;
- detector input;
- model input.

## 16.2 DAQ section

Show:

- Status;
- auto-detected Device;
- Device selector when more than one supported device is found;
- Output Channel;
- maximum supported voltage range when reported by the device;
- maximum supported output frequency when reported by the device.

Voltage and frequency limits are read-only hardware capability information.

Only settings supported by the connected and qualified hardware adapter shall appear.

---

# 17. Shared design components

The initial Qt Design Studio component set shall include:

```text
StatusHeaderItem
CollapsibleSection
BottomHardwarePanel
DarkImageViewer
CameraActionBar
ErrorMessage
CropThumbnail
SelectedCropSection
ClassesFilterSection
ModelListRow
SelectedModelSection
HitBoundaryOverlay
RunListSection
TrainingPlot
```

These components shall use centralized tokens and remain editable in Qt Design Studio.

Production logic remains outside `.ui.qml` forms.

---

# 18. Development order

Proceed in this order:

```text
1. Shell and Mock Single Image
2. Full Capture and Hardware panel
3. Label
4. Sequence Viewer
5. Train
6. Library
7. Model Test
8. Live
9. Sequence Test
10. Results
11. Settings
```

The current implementation slice remains focused on:

- the OpenDSS shell;
- maximized startup;
- 1600 × 900 minimum restored size;
- enforced 16:9 resizing;
- bottom-left Hardware panel frame;
- startup Camera availability mock;
- Capture section headers;
- Mock Single Image;
- minimal Error presentation.

Real Camera, TIFF, DAQ, Training, Run persistence, and later workspaces remain separate authorized slices.

---

# 19. Superseded requirements summary

This amendment supersedes prior requirements for:

1. 1280 × 720 minimum window size;
2. unconstrained restored-window aspect ratio;
3. right-side Hardware drawer;
4. `Dataset Capture` naming;
5. detailed visible fault messages;
6. Sequence Player automatic playback;
7. optional post-Training Model save;
8. no required Training plots;
9. local Model selection in Model Test;
10. local Model selection in Sequence Test;
11. Trigger Mode segmented control;
12. discrete Hit Outlet Direction without an interactive boundary;
13. Sequence Test processing as quickly as possible;
14. Sequence Test Physical DAQ Output enabled by default;
15. Results list-to-detail replacement layout;
16. Live requiring physical DAQ output to begin processing.

All product behavior not addressed by this amendment remains governed by the Approved v2 Product Model and current downstream specifications.
