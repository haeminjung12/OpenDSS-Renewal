# OpenDSS v2 visual scaffold — first two rounds

## Status and authority

**Status:** User-authorized implementation workflow  
**Authorized:** July 23, 2026  
**Scope:** Visual scaffold and its immediate visual/runtime seam only

This plan authorizes a visual-only navigation scaffold for every approved OpenDSS v2 workspace. It does not authorize speculative controls, operational behavior, production hardware integration, persistence, scientific processing, or later-workspace implementation.

**Current handoff:** Approved GUI forms and the deterministic hardware-free backbone are complete and reviewed through `9b27313`, `7e953d9`, `5be26c2`, `12be7e3`, `67930e5`, `2dd088c`, `53dc6a7`, `0f16cc0`, `639a60e`, `c850a8a`, and `f39f1b8`. Qt Design Studio generator registration was committed at `2019cb0`, followed by the visual test-host fix at `26edf96`. Configure in `odss-v2-dbg`, both targeted builds, `ShellSingleImage` CTest (1/1), and the offscreen event-loop smoke without QML runtime warnings passed, closing the generated registration/build gate. The slice remains open only for user-led overall visual validation in Qt Design Studio 2D view and Live Preview at 1600 × 900, maximized, and a larger 16:9 size, including focus and non-color cues. Automated GUI interaction is not accepted evidence because it was interrupted by user Escape and its first launch lacked the MinGW runtime on `PATH`. The tracked worktree was clean at handoff, and the protected untracked `.qtds` file was preserved.

Canonical product and interaction documents remain authoritative. This plan controls delivery order, worker ownership, and proportional validation.

## Simplest complete approach

1. Establish and manually accept one coherent visual skeleton.
2. Freeze the narrow form-to-runtime interface actually needed by that skeleton.
3. Split Round 2 into a design worker and a backbone worker with nonoverlapping files.
4. Integrate once and hand visual judgment to the user.

Do not create placeholder services, generalized UI frameworks, duplicate state owners, or controls for future behavior.

## Round 1 — Visual-only navigation scaffold

### Job VSC-R1-DESIGN — Scaffold every approved workspace

**Worker:** one OpenDSS design implementation worker  
**Objective:** Produce the Qt Design Studio-editable shell and skeletal page composition for:

```text
Data > Capture
Data > Label
Data > Sequence Viewer
Models > Train
Models > Model Test
Models > Library
Sort > Live
Sort > Sequence Test
Results > Runs
Settings
```

The scaffold includes the maximized/16:9 shell, compact status header, primary navigation, workspace host, bottom-left Hardware-panel frame, Camera-unavailable mock, Capture headings, and Mock Single Image. Every other workspace contains only approved structural regions, approved section names, and deterministic visual mock state needed to review composition.

**Not allowed:**

- speculative controls, fields, menus, settings, states, or behaviors;
- runtime navigation/controller implementation;
- production Camera, TIFF, DAQ, detector, inference, Training, Run, Results, or persistence calls;
- QML access to vendor SDKs, filesystem, trainer, or application services;
- hand edits to generated Qt Design Studio CMake files;
- changes to accepted user-edited forms unless the work order names them.

**Round 1 output:**

- one visually coherent scaffold diff;
- proposed exported aliases/signals/state names limited to immediate Round 2 consumers;
- no backbone implementation;
- user-led manual review in Qt Design Studio 2D view and Live Preview.

**Round 1 exit:** The user accepts the overall shell, page hierarchy, workspace skeletons, minimum-window presentation, and visual/runtime interface names. Rejected visual decisions return only to the design worker.

## Interface freeze between rounds

Before Round 2 starts, the orchestrator records in the two work orders:

- exact `*.ui.qml` forms and exported aliases owned by design;
- exact wrapper signals/properties consumed by backbone;
- authoritative state owner for window, navigation, header, Hardware panel, Capture disclosures, and mock availability;
- deterministic mock interface shape;
- exact file ownership and integration order.

The accepted Round 1 form diff is the visual contract. Do not create a separate architecture framework or duplicate specification. A breaking interface change pauses the dependent worker until the orchestrator updates both work orders.

## Round 2 — Design and backbone split

Round 2 uses two isolated worktrees/branches from the same accepted Round 1 base.

### Job VSC-R2-DESIGN — Refine the accepted visual baseline

**Owns:** authorized `*.ui.qml` forms, tokens, visual assets, design-time visual states, and Qt Design Studio mocks.

**Does:**

- refine spacing, hierarchy, sizing, focus presentation, disclosure visuals, and deterministic mock states;
- preserve the frozen exported interface;
- keep all approved workspace scaffolds visually consistent;
- prepare user review states at 1600 × 900, maximized, and one larger 16:9 size.

**Does not:** edit wrappers, C++, runtime state, durable CMake integration, production services, or generated files.

### Job VSC-R2-BACKBONE — Implement the minimal shell backbone

**Owns:** ordinary QML wrappers, the smallest C++/application-state projection needed by the accepted scaffold, narrow mock/runtime adapters, directly relevant tests, and explicitly authorized durable CMake files.

**Does:**

- implement application startup/window constraints, selected-workspace navigation state, compact header projection, Hardware-panel open/closed state, Camera-availability mock outcome, and Capture disclosure state;
- bind only the frozen Round 1 interface;
- keep one authoritative owner for each state;
- remain deterministic and hardware-free.

**Does not:** edit `*.ui.qml`, invoke vendor SDKs, implement real Camera/TIFF/DAQ/scientific operations, add later-workspace behavior, or build generalized service/factory/registry layers.

## Integration order

1. Accept Round 1 visual scaffold and freeze the seam.
2. Start the two Round 2 workers from the same accepted base in separate worktrees.
3. Integrate the design result first only when it preserves the frozen interface.
4. Integrate the backbone result second.
5. Resolve visual-contract changes by returning them to the owning worker; do not guess during integration.
6. Obtain user-led Round 2 visual acceptance after the completed generated Content CMake registration and targeted configure/build gate. Do not begin Full Capture or production integration automatically.

## Minimal validation policy

Workers run only checks proportional to their files:

- design worker: Qt Design Studio 2D view and Live Preview for changed forms, plus `qmllint` only where useful;
- backbone worker: one targeted configure/build and only directly relevant smoke/unit tests;
- orchestrator: narrow diff/ownership review and one integrated launch/build check when needed;
- user: manual visual review, navigation review, minimum-window/scaling review, and acceptance.

Do not run the full legacy C++/Python/hardware test matrix for these rounds. Do not repeat successful checks without a relevant change. Hardware qualification is out of scope.

## Planning boundary

This file authorizes the workflow and scope, not specific writes. Before each job, the orchestrator must issue a bounded work order naming exact files, base commit, worktree, branch, validation, and staging/commit authority.
