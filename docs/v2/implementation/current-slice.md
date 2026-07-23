# OpenDSS v2 visual navigation scaffold and Mock Single Image slice

## Status

User-authorized for Round 1 visual-only scaffold planning and implementation through bounded work orders. Production behavior remains unauthorized.

All approved GUI forms and the deterministic hardware-free backbone are complete and reviewed through `9b27313`, `7e953d9`, `5be26c2`, `12be7e3`, `67930e5`, `2dd088c`, `53dc6a7`, `0f16cc0`, `639a60e`, `c850a8a`, and `f39f1b8`. Qt Design Studio generator registration was committed at `2019cb0`, followed by the visual test-host fix at `26edf96`. Configure in `odss-v2-dbg`, the `Desktop_app_v2App` and `tst_ShellSingleImage` builds, `ShellSingleImage` CTest (1/1), and the offscreen runtime smoke reaching the event loop with no QML runtime warnings passed. The generated registration/build gate is closed.

This slice remains open only for user-led overall visual validation: Qt Design Studio 2D view and Live Preview at 1600 × 900, maximized, and a representative larger 16:9 size, including keyboard focus and non-color cues. Do not claim an automated GUI interaction pass: that attempt was interrupted by user Escape, and its first launch lacked the MinGW runtime on `PATH`. At validation handoff the tracked worktree was clean, and the protected untracked `Desktop_app_v2.qmlproject.qtds` was preserved.

The earlier baseline committed as `188a649` remains evidence. Its Single Image-only Capture composition, former smaller minimum, and former Hardware placement are superseded and must not be extended.

## Current objective

Establish the amended shell, Mock Single Image presentation, and a visual-only navigation scaffold for every approved workspace without connecting production Camera, TIFF, DAQ, Training, Live, Sequence Test, Results, or persistence behavior.

## Required reading

1. [Repository agent policy](../../../AGENTS.md).
2. [Implementation context](../CONTEXT.md).
3. [Approved UI/UX amendment](../OpenDSS_v2_UIUX_Design_Amendment.md), §§2–5, 17, and 18.
4. [Approved Product Model](../canonical/product-model.md), §§1, 4, 5, and 7.1.
5. [Information Architecture](../canonical/information-architecture.md), §0 and §§1–4.
6. [Interaction and Application-State Specification](../canonical/interaction-and-state.md), §0 and Capture/error sections.
7. [Consolidated Design Draft](../design/consolidated-design-draft.md), amendment integration, shell, shared components, Capture, responsive/accessibility, and Qt Design Studio handoff sections. It remains **Consolidated Draft for Review**.
8. [Qt Design Studio adoption record](qt-design-studio-adoption.md), continuing-slice constraints.
9. [Visual scaffold first-two-round plan](visual-scaffold-two-round-plan.md).

## Authorized visual-baseline consequences

- maximized startup presentation;
- restored minimum logical size of 1600 × 900;
- enforced 16:9 manual resizing;
- compact one-line status header with Camera, DAQ, Active Model, and Current Activity items using icon, label, value, readiness color, and a non-color cue;
- bottom-left overlay Hardware panel frame with visible arrow, without real Camera or DAQ settings behavior;
- one-session Camera-unavailable startup mock with `Camera unavailable. Continue?`, Yes, and No states;
- Capture panel with permanently visible headings **Single Image**, **Image Sequence**, and **Droplet Dataset Capture**;
- independently collapsible Capture bodies, including active-section-expanded and other-headings-disabled mock states;
- deterministic Mock Single Image states;
- shared `CollapsibleSection` visual component;
- minimal `ErrorMessage` presentation showing `Error` and only directly useful contextual actions.
- Qt Design Studio-editable skeletal hosts for Label, Sequence Viewer, Train, Model Test, Library, Live, Sequence Test, Results, and Settings using only approved regions and section names;
- deterministic visual selection/mock states sufficient to review each approved workspace composition;
- proposed exported aliases, signals, and state names only where an immediate Round 2 consumer is identified.

## Authorized files

No files are authorized by this planning record alone. The orchestrator must issue a bounded visual work order naming every writable `*.ui.qml`, wrapper QML, mock, asset, test, and durable CMake file before implementation starts. Accepted user-edited forms remain read-only unless named explicitly.

## Out of scope

- production Camera connection or streaming;
- real TIFF capture or writing;
- Image Sequence and Droplet Dataset Capture implementation;
- DAQ integration or physical output;
- detector, inference, Training, Model Test, Live, Sequence Test, Results, Run, or persistence behavior;
- speculative placeholder controls, fields, settings, or states inside the skeletal workspace hosts;
- changes to protected reusable technical assets;
- hand edits to Qt Design Studio-generated CMake files;
- a Home screen, separate Sort Setup workspace, local Model selectors, detailed visible diagnostics, or any behavior copied from legacy UI policy.

## Required mock and review states

- 1600 × 900 restored and maximized window presentations;
- header ready, unavailable, active, and idle combinations with non-color readiness meaning;
- Hardware panel closed and bottom-left overlay-open states;
- Camera available and Camera unavailable startup states, including Yes/No prompt;
- Capture with all bodies collapsed, each body expanded, multiple idle bodies expanded, and an active body expanded while other headings are disabled;
- Mock Single Image unavailable, ready, capturing, completed, and `Error` states;
- each approved workspace host selected in the shell with only its approved structural regions visible;
- keyboard focus order and supported scaling at the minimum window.

## Acceptance criteria

- The shell opens visually at Data > Capture and reflects maximized startup.
- Restored layout never presents below 1600 × 900 and preserves 16:9.
- The status header remains one compact line and readiness is not color-only.
- The Hardware panel overlays only the lower-left workspace and does not push content or span the window.
- The Capture panel and all three headings remain visible in every required state.
- Only Capture bodies collapse; multiple idle bodies can be expanded; the active body stays expanded; other headings remain visible but disabled.
- The user-facing name is **Droplet Dataset Capture**.
- The Camera-unavailable mock appears once per session and exposes the approved Yes/No outcomes.
- Normal mock failure presentation is exactly `Error`; technical detail is absent from the workspace.
- All approved workspaces have a recognizable, source-grounded visual host without speculative controls or runtime behavior.
- Modified forms remain editable in Qt Design Studio 2D view and render in Live Preview.
- No production, generated, protected, or later-slice source is changed.

## Validation for the later visual work order

1. User-led Qt Design Studio 2D view, Live Preview, and manual review for the changed forms and representative mock states.
2. User-led review at 1600 × 900, maximized, and one representative larger 16:9 layout, including keyboard focus and non-color meaning.
3. `qmllint` only for changed QML when useful.
4. One configure/build or directly relevant targeted test only when the changed files require it.
5. Narrow diff review proving no generated file, speculative behavior, or production integration was added.

Do not run the full legacy, Python, or hardware test matrix for these visual rounds.

## Round workflow and following slices

Round 1 is the single-writer visual scaffold in [visual-scaffold-two-round-plan.md](visual-scaffold-two-round-plan.md). After user acceptance and interface freeze, Round 2 splits into nonoverlapping design and backbone work orders in separate worktrees.

The approved implementation order is:

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

The all-workspace scaffold does not start the later behaviors listed above. Do not begin Full Capture or any later production behavior without explicit authorization after user-led Round 2 visual acceptance.
