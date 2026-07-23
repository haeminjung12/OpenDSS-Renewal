# OpenDSS Pre-Verification Design and UI/UX Review

> **Historical review record:** This pre-verification assessment predates the approved July 23, 2026 UI/UX amendment. Its risk observations remain evidence, but its UI names, compositions, and proposed controls are superseded wherever they conflict with the amendment and the revised canonical/design documents. It is not an active implementation or design-QA plan.

**Status:** Archived review reference  
**Purpose:** Preserve the broader pre-verification review for later return without blocking current visual design and development.  
**Date:** July 22, 2026

## Review basis and evidence boundary

This review used the following OpenDSS v2 sources:

- `OpenDSS_v2_Consolidated_Product_Design_Specification.md`
- `OpenDSS_Approved_v2_Product_Model.md`
- `OpenDSS_Detailed_User_Workflow_Specification.md`
- `OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md`
- `Qt_Design_Studio_Workflow_Adoption.md`

The Approved v2 Product Model is controlling. Conflicting behavior from the historical repository or superseded workflow material is not treated as a valid product alternative.

The review evaluates specification readiness and identifies later verification needs. It does not replace visual design, prototype construction, implementation, or user testing.

---

# 1. Executive assessment

## Readiness by stage

| Stage | Assessment |
|---|---|
| Continued visual implementation | **Proceed.** The product structure, workspace boundaries, terminology, components, states, accessibility direction, and Qt handoff contract are sufficiently defined. |
| Formative usability testing | **Prepare after a thin interactive prototype exists.** Static comprehension reviews can begin earlier. |
| Hardware-connected usability testing | **Later development stage.** Requires implemented Camera/DAQ behavior and an instrumented build. |
| Final design verification | **Not applicable yet.** Requires the implementation, realistic data, accessibility evidence, performance evidence, and hardware testing. |

## Overall judgment

The OpenDSS v2 design is sufficiently defined to move into visual design and implementation.

The broader items that should be retained for later verification are:

1. physical-output comprehension;
2. image-axis and Hit Outlet Direction mapping;
3. applied hardware settings versus profile values that did not apply;
4. real-time persistence and counter meaning;
5. actual user comprehension of scientific terminology;
6. accessibility, high-DPI, and performance verification;
7. recovery from hardware and file faults.

These do not justify delaying normal visual development of the shell, Capture, Label, Train, Library, Sequence Player, Results, or shared components.

---

# 2. What is already strong

## Authority and scope

The authority order is explicit and should remain unchanged:

1. Approved v2 Product Model;
2. current IA and interaction/state baseline;
3. nonconflicting Detailed Workflow requirements;
4. old design material as supporting evidence only;
5. old repository as implementation evidence only.

The specification also clearly excludes unapproved first-release additions such as a Home dashboard, separate Sort Setup workspace, managed Setup Profile library, editable detector/training parameters, model approval states, charts, and legacy migration UI.

## Scientific terminology

The required relationship is clearly defined:

```text
Predicted Class
    → Decision
        → Observed Route
```

Important distinctions are preserved:

- Label versus Predicted Class;
- Predicted Class versus Decision;
- Decision versus Observed Route;
- Hit Class versus Hit Outlet Direction;
- DAQ Output Channel versus Hit Outlet Direction;
- Class Score rather than Confidence;
- Selected Model versus Active Model;
- Hit, Waste, and Unresolved as Observed Route values.

## Information architecture

The approved navigation is coherent:

```text
Data
├── Capture
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

Strong decisions include:

- no Home screen;
- no separate Sort Setup screen;
- one persistent Live workspace;
- Sequence Test under Sort;
- Results restricted to sorting Runs;
- Capture as one shared live-camera workspace;
- contextual artifact handoffs rather than a mandatory wizard.

## Shared shell and state ownership

The shell clearly separates:

- global status;
- navigation;
- current workspace;
- workspace-owned side panel;
- shell-owned Camera/DAQ drawer;
- contextual workspace fault communication.

The resource-ownership model is also sufficiently defined for implementation:

- one long-running operation at a time;
- Camera and DAQ locking by the owning operation;
- Dataset read locks during Training and Model Test;
- Model Package use locks during Model Test, Live, and Sequence Test;
- Run output ownership during Live and Sequence Test;
- passive navigation without terminating the active operation.

## Faults and recovery

The design correctly requires a fault presentation to state:

- what stopped;
- why it stopped;
- whether partial data was preserved;
- what the user can do next.

This is stronger and more useful than generic error dialogs.

## File ownership and reproducibility

Datasets, sequences, model packages, profiles, and Runs remain normal user-owned Windows files and folders. Canonical files and direct folder actions support reproducibility, backup, transfer, and recovery.

## Label workflow

The Label workspace is well specified for large-scale work:

- virtualized collection;
- pointer and keyboard selection;
- bulk labeling;
- separate selection, focus, class, skipped, and removed states;
- autosave;
- Undo;
- Restore;
- Dataset lock behavior.

## Qt Design Studio boundary

The adopted pattern is appropriate:

```text
designer-editable *.ui.qml form
    → ordinary QML behavior wrapper
        → authoritative application-state projection
            → domain/application services
```

Visual forms remain editable in Qt Design Studio, while production logic, hardware access, persistence, and state ownership remain outside the form.

---

# 3. Priority risk register for later verification

| ID | Area | Risk | Severity | Classification | Later action |
|---|---|---|---|---|---|
| ODSS-UX-001 | Live, Sequence Test, Send Test Pulse | Users may not know exactly which DAQ device, channel, and pulse settings will produce output. | P0 | Specification ambiguity | Define a read-only physical-output summary before consequential actions. |
| ODSS-UX-002 | Camera orientation and Hit Outlet Direction | Rotation, mirroring, or display transforms could reverse the user’s physical direction interpretation. | P0 | Specification ambiguity | Define and record the Camera-to-display transform and show an orientation overlay. |
| ODSS-UX-003 | Hardware drawer and Profiles | A user may mistake loaded Profile values for successfully applied hardware values. | P0 | Specification ambiguity | Visibly separate applied, rejected, unavailable, locked, and not-applied Profile values. |
| ODSS-UX-004 | Capture, Live, Sequence Test | Counts may be interpreted as safely written even when data is only detected or queued. | P0 | Specification ambiguity | Define acquired, queued, persisted, finalized, and dropped count semantics. |
| ODSS-UX-005 | Current frontend | The current Qt form is still the generated starter template. | P1 | Incomplete implementation | Build the approved shell and mock workflows. |
| ODSS-UX-006 | Lifecycle terminology | Clean user Stop may be confused with natural completion. | P1 | Specification ambiguity | Use Completed for normal/timed completion and Stopped for clean user Stop. |
| ODSS-UX-007 | Real-time monitoring | A frozen or stale preview may still look operational. | P1 | Specification ambiguity | Define stale-preview and stale-counter behavior. |
| ODSS-UX-008 | Performance | Responsiveness requirements are not yet measurable. | P1 | Specification ambiguity | Add performance budgets later against qualified hardware and realistic data. |
| ODSS-UX-009 | Model Test and Sequence Test | Local selected Model may be confused with global Active Model. | P1 | Specification ambiguity | Label the model used by the current operation explicitly. |
| ODSS-UX-010 | First launch without hardware | Users may not understand what remains usable or which Capture section creates which artifact. | P1 | Usability hypothesis | Test concise contextual orientation without adding a Home screen. |
| ODSS-UX-011 | Fixed qualified processing | Users may not know which processing configuration produced an artifact. | P1 | Specification ambiguity | Show and record a processing configuration identifier/version. |
| ODSS-UX-012 | Accessibility | No implementation evidence exists yet. | P1 | Incomplete implementation | Verify keyboard, focus, screen-reader, contrast, and scaling behavior later. |
| ODSS-UX-013 | High DPI | Minimum effective workspace at 200% scaling is not fully defined. | P1 | Specification ambiguity | Define the minimum device-independent viewport and test mixed-DPI monitors. |
| ODSS-UX-014 | Qt Design Studio handoff | No accepted OpenDSS screen has completed the full designer-to-runtime round trip. | P1 | Incomplete implementation | Complete the shell/Single Image round trip first. |
| ODSS-UX-015 | Documentation | Documentation deliverables and ownership are not yet formalized. | P1 | Specification ambiguity | Add user, hardware, file, recovery, and terminology documentation before release. |
| ODSS-UX-016 | Live monitor | The specified metric density may still be difficult to scan under time pressure. | P1 | Usability hypothesis | Test with realistic counters, update rates, and abnormal states. |
| ODSS-UX-017 | Recovery | Partial and failed artifacts are specified but not implemented or tested. | P1 | Incomplete implementation | Create deterministic interruption fixtures and recovery tests. |
| ODSS-UX-018 | Faster versus More Accurate | The practical tradeoff may not be obvious. | P2 | Usability hypothesis | Add benchmark-based explanatory copy later. |
| ODSS-UX-019 | Large Dataset labeling | Bulk shortcuts may amplify selection errors. | P2 | Usability hypothesis | Test bulk-scope visibility, Undo, and save feedback. |
| ODSS-UX-020 | Artifact discoverability | Users may not know which canonical file to move, copy, or open. | P2 | Usability hypothesis | Test file-location tasks and provide an artifact map. |

---

# 4. End-to-end task-flow review

## First launch without hardware

The approved startup is acceptable:

- open Data > Capture;
- retain the preview region;
- show Camera unavailable;
- keep all three Capture headings visible and collapsed;
- allow navigation to file-based workspaces.

Later review should test whether users understand what remains usable without hardware.

## Save one TIFF

The flow is sufficiently defined:

1. Camera reaches Streaming;
2. user expands Single Image;
3. user optionally enters a name/location;
4. user selects Capture Image;
5. exactly one TIFF is written;
6. the saved path is confirmed.

The design should visibly distinguish Camera Connected from Camera Streaming and should not claim success until the file write completes.

## Image Sequence

The required flow is clear:

```text
Ready
→ Starting
→ Running
→ Paused
→ Running
→ Stopping
→ Stopped or Completed
```

The live preview remains visible. Pause does not split the sequence. The main later issue is defining whether displayed frame counts are acquired, queued, or written.

## Dataset Capture fault recovery

The required fault path is appropriate:

```text
Camera disconnect
→ stop new capture and detection
→ flush available data
→ finalize a recoverable Dataset when possible
→ show Interrupted or Failed
→ Open Dataset or Open Folder
```

The actual preservation behavior must later be tested against real partial files.

## Large Dataset labeling

The design supports high-throughput work and is ready for implementation. Later testing should focus on:

- bulk-selection scope;
- accidental shortcut activation;
- Undo visibility;
- autosave latency;
- filter and selection behavior;
- long-session fatigue.

## Train and save Model

The workflow is sufficiently defined:

1. select Dataset;
2. select Faster or More Accurate;
3. start Training;
4. monitor progress and factual metrics;
5. complete Training;
6. name and save Model Package;
7. saved model becomes Active.

The interface should make the separate Save Model step visually obvious after technical completion.

## Live Sorting

The pre-run and active operation are correctly modeled as one workspace. The required groups are:

- Setup Profile;
- run metadata;
- Trigger Mode;
- Active Model and Hit Class when applicable;
- Hit Outlet Direction;
- full-sequence option;
- Send Test Pulse;
- Start Sorting.

During Running, the most important information is:

- operation state;
- elapsed time;
- Total Droplets;
- Decision Hit/Waste;
- Observed Hit/Waste/Unresolved;
- physical-output context;
- Pause and Stop.

Predicted Class counts and performance metrics are secondary.

## Sequence Test

The structure is correct:

- no Camera dependency;
- explicit Physical DAQ Output control;
- Class-Based and Trigger Every Droplet modes;
- local model selection;
- no playback-speed control;
- creates a Run;
- source sequence remains unchanged.

Before implementation is finalized, the physical-output summary and selected-versus-Active Model distinction should be clarified.

## Run review

Results should retain separate groups for:

- Predicted Class;
- Decision;
- Observed Route;
- Decision-versus-Observed Route matrix;
- provenance;
- files;
- Notes.

The current first-release scope is sufficient. Charts and an integrated event browser should remain deferred.

---

# 5. Scientific comprehension checks for later testing

Ask users to explain:

- what a Dataset contains that an Image Sequence does not;
- why Dataset Capture is used before Label and Train;
- why Sequence Player does not create a Run;
- who assigns a Label and who produces a Predicted Class;
- whether Predicted Class and Decision can differ;
- whether Decision and Observed Route can differ;
- what Unresolved means;
- the difference between Hit Class and Hit Outlet Direction;
- the difference between DAQ Output Channel and Hit Outlet Direction;
- whether selecting a Model in Model Test makes it Active;
- whether a Class Score is necessarily a probability;
- how Trigger Every Droplet behaves with and without a model;
- which hardware/Profile values are actually applied;
- what Starting and Stopping mean;
- why a Run is Stopped rather than Completed.

---

# 6. Real-time operation items to verify later

## Capture

Later tests should verify:

- preview remains responsive;
- Pause and Resume preserve one operation;
- counts clearly distinguish detection and persistence;
- partial data is preserved after a Camera or write fault;
- Stop finalization is visible.

## Live

Later tests should verify:

- the operator can see the physical-output state at a glance;
- Decision and Observed Route remain visually distinct;
- Unresolved is visible;
- Pause stops inference, DAQ output, and event finalization;
- Stop remains visible without scrolling;
- stale Camera or counter data is unmistakable;
- write backlog or failure is not hidden.

## Sequence Test

Later tests should verify:

- Physical DAQ Output is noticed before Start;
- output disabled truly permits hardware-free processing;
- the selected local Model is clear;
- progress and finalized-event counts are not conflated;
- DAQ failure stops physical output before the terminal error presentation.

---

# 7. Documentation to prepare before release

The following should eventually exist:

- Getting Started;
- terminology guide;
- hardware readiness guide;
- Capture workflow guide;
- Label keyboard reference;
- Training and Model Test guide;
- Live and Sequence Test guide;
- canonical artifact/file map;
- fault and recovery guide;
- provenance reference;
- accessibility and keyboard guide;
- diagnostics and support guide.

These are release requirements, not blockers for beginning visual implementation.

---

# 8. Later usability-test plan

## Mock-data testing

Use deterministic mocks for:

- first launch without hardware;
- Capture operation selection;
- large Dataset labeling;
- Train completion and model activation;
- selected versus Active Model;
- Trigger Mode comprehension;
- Run interpretation;
- Setup Profile partial application;
- file and recovery states;
- keyboard, focus, and scaling.

## Real-hardware testing

Use qualified hardware for:

- Camera readiness and TIFF capture;
- recording Pause/Resume/Stop;
- Camera disconnect recovery;
- Hit Outlet Direction;
- Send Test Pulse;
- Live Trigger Modes;
- Live Pause and Stop;
- DAQ fault behavior;
- output-write faults;
- Sequence Test output enabled and disabled.

## Critical errors

Treat these as critical:

- unintended physical output;
- wrong DAQ channel;
- wrong Hit Outlet Direction;
- inability to Stop;
- false successful-save indication;
- treating queued data as persisted;
- using the wrong Model;
- conflating Decision and Observed Route;
- treating Unresolved as Waste;
- opening an incomplete artifact as completed.

---

# 9. Missing definitions to return to later

The main later decisions are:

1. effective Camera/image transform;
2. physical-output summary;
3. applied hardware-state presentation;
4. live counter semantics;
5. writer backlog and dropped-data behavior;
6. Completed versus Stopped;
7. stale data thresholds;
8. measurable performance budgets;
9. Faster versus More Accurate explanation;
10. fixed-processing configuration identifier;
11. selected versus Active Model presentation;
12. minimum effective DPI-scaled workspace;
13. documentation ownership;
14. test-pulse intentionality.

---

# 10. Evidence required before final verification

- authoritative low-fidelity interaction/state document;
- complete OpenDSS `.ui.qml` forms and QML wrappers;
- component gallery;
- screen-state gallery;
- interactive prototype;
- responsive/scaling renders;
- representative valid and malformed artifacts;
- partial/recovery fixtures;
- qualified hardware and orientation record;
- fault-injection evidence;
- performance traces;
- accessibility evidence;
- Qt Design Studio round-trip evidence;
- user-test evidence;
- documentation drafts.

---

# 11. Recommended later specification amendments

Add testable requirements for:

- physical-output summaries;
- Camera-to-display orientation;
- applied and unapplied Profile values;
- persistence and counter semantics;
- Completed/Stopped/Interrupted/Failed vocabulary;
- stale data presentation;
- performance budgets;
- fixed-processing identifiers;
- local versus Active Model identity;
- documentation;
- user research;
- accessibility evidence;
- effective DPI-scaled minimum size;
- Qt Design Studio form acceptance.

---

# 12. Later verification gates

1. specification completeness;
2. design-system consistency;
3. static screen review;
4. interactive prototype review;
5. formative usability testing;
6. hardware-connected testing;
7. accessibility verification;
8. final approval.

These gates are retained as future review structure and should not block beginning the current visual design and implementation work.

---

# 13. Development-oriented conclusion

The OpenDSS v2 UI/UX is sufficiently defined to proceed.

The immediate work should now focus on:

1. visual design language and tokens;
2. application shell;
3. shared components;
4. layout composition;
5. deterministic mock states;
6. Qt Design Studio-editable forms;
7. QML wrappers and narrow state interfaces;
8. iterative build and visual review.

The broader verification items in this document should remain logged and be revisited when the relevant screen, workflow, hardware behavior, or persistence implementation exists.

---

# Source references

## OpenDSS sources

- OpenDSS v2 Consolidated Product Design Specification
- Approved v2 Product Model
- Detailed User Workflow Specification
- OpenDSS v2 Information Architecture and Screen Inventory
- Qt Design Studio Workflow Adoption

## External scientific-software UX sources

- Better Scientific Software: Framing User Experience Across the Scientific Software Lifecycle
- Better Scientific Software: User Experience Design in the Lifecycle of Scientific Software
- Better Scientific Software: User Experience Engineering in the Lifecycle of Scientific Software
- Better Scientific Software: Design Systems to Help Amplify Development of Usable Scientific Software Interfaces
- NCSA: Designed for Better Scientific Software
- STRUDEL Design System: Overview, Monitor Activities, Run Computation, and Track State
- Qt Design Studio and Qt High-DPI documentation
- WCAG 2.2
