# OpenDSS v2 UI/UX Visual Review Amendment — 2026-07-23

## Status and authority

**User-approved visual-review amendment.** This amendment is subordinate to the [Approved Product Model](canonical/product-model.md) for product policy and scope. For the topics below, it controls and supersedes conflicting UI layout, naming, disclosure, and composition language in the [Approved UI/UX Design Amendment](OpenDSS_v2_UIUX_Design_Amendment.md), Information Architecture, consolidated design drafts, and review artifacts.

It authorizes no production Camera, DAQ, vendor, filesystem, persistence, detector, inference, trainer, Run, Results, or other production behavior.

## Approved visual-review decisions

### Shared disclosure and shell behavior

- Inner disclosure sections use the shared titled disclosure-header treatment, with a chevron and actual click-to-collapse behavior. Workspace outer right panels are visually distinct: each has a light top strip with the exact visible title **Capture**, **Label**, **Train**, **Model Test**, **Library**, **Live**, **Sequence Test**, or **Runs**, left-aligned while expanded, and its existing narrow chevron toggle fixed on the panel's right edge at vertical center, with the same screen x/y position in expanded and collapsed states. The outer toggle is a fixed **28 × 36 px** icon control with a **14 px** chevron and does not grow with Text Size. When collapsed width is insufficient, only the fixed chevron may remain visible. Settings has no outer collapsible panel. Existing exported toggle aliases and wrapper seams are preserved.
- Right-panel typography uses a consistent hierarchy: workspace/outer-panel titles are strongest, disclosure headers are secondary, and setting labels use the same normal font baseline as control content and buttons. Smaller muted typography is reserved for intentional supporting, warning, or status text rather than ordinary setting labels.
- The left navigation and workspace outer right panels are user-adjustable with direct split-handle dragging. **Approximately 536 px is the right-panel default at 100% Text Size, not a fixed width.** Navigation and right-panel default proportions scale with application-wide Text Size, reset on each application launch, and are not persisted. This expressly supersedes earlier fixed-width wording for these panels.
- Workspace outer right panels still collapse as a whole. Inner disclosure sections retain intrinsic-height stacking where applicable.
- An active primary-navigation item remains selected when clicked; it cannot toggle itself off.
- Every expanded right-panel body, including Capture, takes only its intrinsic needed height. The next header follows immediately; unused space stays below. Local scrolling appears only when real content exceeds available height. This supersedes any Capture equal-remaining-height direction.
- All ten approved destinations use one consistent workspace-title treatment and naming.
- Within the maximized window, enlarged content uses the full available workspace through the window bottom; it does not preserve a smaller fixed canvas inside that window. At larger Text Size settings, content remains contained and scrolls where necessary. Visual and Computer Use validation use the maximized window only; restored-window and fixed-resolution checks are not required.
- The shell uses a quiet light sidebar, a restrained selected-item marker, a compact and visually stronger fixed status header, navy/blue primary actions, light fields, destructive red actions, and restrained icons.
- The fixed shell status header is not expandable. **Hardware Configuration** is the visible title of the Hardware panel docked at the bottom of the resizable left-navigation column; it is not an illustrative-mock label. The panel matches that column's width, expands upward within it, and is not a workspace overlay. Its content has two titled collapsible groups, **Camera** and **DAQ**, using the same white section surface. **Output Configuration** is nested inside DAQ as a visually inset, bordered white subordinate group and contains Output Channel, Amplitude, Frequency, Event Duration, Decision-to-trigger Delay, and a single **Start Sine Wave** button styled as a high-visibility primary action. The DAQ form shows no separate sine-wave heading, status subsection, or explanatory paragraph. Its open height is vertically user-adjustable within bounded limits, starts from a compact design-token default, and is not persisted. Hardware content scrolls when it exceeds the selected height. Its chevron is right-aligned, and no redundant visible **Close** action appears. Hardware does not move into the status header or a workspace right panel. This hierarchy is visual/product authority; actual clickable group collapse must use the existing DESIGN/FUNCTIONAL seam and does not authorize new runtime handlers. The accepted form seam exports `cameraSectionHeadingButton`, `cameraSectionExpanded`, `daqSectionHeadingButton`, and `daqSectionExpanded`; functional wrapper code owns the two click-to-toggle connections.
- The once-per-session `Camera unavailable. Continue?` prompt is a shell-global modal overlay above the entire shell and current workspace. `Data > Capture` remains selected beneath it; Yes continues without Camera and leaves ordinary unavailable status visible, while No closes OpenDSS.
- Inner disclosure headers retain the chevron and title but do not print redundant **Expanded** or **Collapsed** labels. A meaningful **Disabled** or unavailable cue remains when interaction is genuinely unavailable.
- The existing **Start Camera** / **Stop Camera** action appears at the bottom of the Capture right panel. It reuses its existing visual ID and wrapper seam; this decision does not authorize a duplicate Camera action or a second state owner.
- Empty, unavailable, and unmet-prerequisite presentations use one cohesive treatment: a concise explanation adjacent to the existing recovery or selection action. This decision adds no new workflow actions.

### Sequence Viewer

- Keep the controls centered and unobstructed by Hardware.
- The primary frame-control row order is exactly **-50**, **-10**, **Previous**, **Next**, **+10**, **+50**, timeline slider, then current/total.
- Direct frame entry appears below that row. Existing zoom controls, **Fit**, and **1:1** remain available without changing the required primary-row order.

### Label

- The Droplet Crop grid remains dominant in the main area.
- An adjustable right panel, with the shared approximately 536 px default at 100% Text Size, collapses as one outer panel and contains, in order:
  1. **Load Dataset**, a static, always-expanded card/header;
  2. **Dataset Summary**, with total and labeled counts plus a two-or-three-class selection that remains switchable for configured Datasets; switching must preserve label consistency and never silently reassign or discard existing labels;
  3. **Label**, with a vertically adjustable selected-crop preview that can be enlarged or reduced, and exactly **Class 0**, **Class 1**, **Class 2**, **Exclude**, **Undo**, **Previous**, and **Next**; all three Class actions remain visible, and Class 2 is disabled for a two-class Dataset;
  4. **Filter**, with class list/count filters plus **Excluded** and **Unreviewed** when applicable;
  5. **Save As** at the bottom-right.
- Class identity remains blue, orange, and purple for Classes 0, 1, and 2. **Skip**, **Remove**, **Restore**, and other former Label-side actions are not part of this composition.
- The Class 2 action is purple when enabled and grey when disabled for a two-class Dataset; it remains visible in both cases.
- Dataset **Save As** creates an independent copy and makes it the current loaded Dataset. It is not version history; normal changes continue saving to the current Dataset under the existing persistence contract.
- For design-time review only, the crop browser shows exactly **455** blank tiles. Their Class 0, Class 1, and Class 2 colors are assigned by a fixed index-derived pseudo-random pattern; runtime randomness is prohibited. The mock includes **Page** and **Images per page** controls, with exactly **100**, **200**, and **500** images-per-page choices and **500** selected by default. These are visual IDs only and define no runtime Dataset count, persistence, selection, filtering, or pagination contract.

### Train and Model Test

- Train and Model Test keep **Dataset Summary** in a main white region and place **Results** in a separate main white region below.
- Train Results uses the approved two live plots and completion tables. Model Test Results uses the approved metrics, confusion matrix, and prediction summaries. This placement introduces no new data semantics.
- Train shows separate **Architecture** and **Weights** selectors. Architecture presents **MobileNet — Faster** and **EfficientNet — More Accurate** on one line, with the supporting label smaller and lighter than the architecture identity. The Weights selector keeps every option fully readable using the concise visible labels **ImageNet-pretrained**, **OpenDSS droplet checkpoint — bundled**, and **OpenDSS droplet checkpoint — user-added**.
- The Architecture selector's closed field and popup options both use that readable single-line presentation. Hover and selected-option treatment keep the dark architecture identity and muted supporting label visible. This supersedes the former two-line presentation.
- The fixed split and **Seed** are not shown as persistent minor text in the normal Train setup panel; both remain recorded in model metadata. **Load Weights** may appear as a non-exported, visual-only button while its file-selection, compatibility, validation, and state semantics remain undefined; it gains no handler or functional seam in this visual pass.
- Train includes a **Compute Device** selector with **GPU** selected by default and **CPU** available. The setup panel does not show a persistent GPU-fallback explanation beneath the selector. A GPU request still uses qualified GPU when available and otherwise falls back to effective CPU with a direct runtime explanation; a CPU request stays on CPU. Requested and effective devices remain recorded. Model Test retains its automatic GPU/CPU policy and CPU fallback.
- Model Library displays the technical architecture together with its supporting **Faster** or **More Accurate** label. It does not substitute the supporting label for architecture identity.

### Live and Settings

- Live does not show a persistent minor **Hit boundary: calibrated outlet region** helper beneath Trigger & Timing; the approved boundary interaction and state remain unchanged.
- In Sequence Test, **Load Sequence** and **Load to Memory** share one row and each occupies half of the available content width.
- Approved Live disclosure bodies show their source-grounded content, not placeholder headings.
- Live keeps a fixed action footer inside the workspace right panel, with setup/settings disclosure content scrolling above it. Ready with the camera off shows **Start Camera** plus disabled **Start Sorting**. Ready with the camera on shows **Stop Camera** plus **Start Sorting**, with sorting enabled only when its existing readiness seam allows it. Running shows **Pause** and destructive-red **Stop**; paused shows **Resume** and destructive-red **Stop**. Completed retains its existing action pair, while error presentations show no irrelevant actions.
- Live run status is fixed information, not an expandable status surface.
- **Trigger Every Droplet** and **DAQ Output** remain independent authoritative choices. Enabling either does not implicitly enable, disable, or redefine the other.
- While Live is active, the visible **Active Model**, **Hit Class**, **Trigger Every Droplet**, **DAQ Output**, and hit-boundary fields remain editable under the Product Model's immediate-apply and effective-configuration-history rules.
- **Send Test Sine Wave** remains a discrete action. **Continuous configured waveform** start/stop and factual state belong in the shared Hardware > DAQ section, not beside Send Test Sine Wave in Live.
- Settings controls remain inside their section borders. Settings contains only **Storage**, **Application Information**, **Diagnostics**, and **Visuals**.
- Visuals contains only application-wide **Text Size**, presented as one dropdown with exactly **Small (80%)**, **Medium (100%)**, and **Large (125%)**. **Medium (100%)** is the default. **200%** is a validation-only condition and is not exposed as a selectable Text Size preference. At Medium, body text, standard control text, and button text use **16 px**. Body and standard controls retain approximately **20 px** line height; buttons retain approximately **18–20 px** line height. Ordinary field and settings labels use **15 px** with approximately **18 px** line height. Captions, status, warning, and metadata use **13 px** with approximately **16–18 px** line height. SettingsRepository and the UI integration must expose only 80, 100, and 125. Before publication or persistence, legacy persisted values must normalize as 90 → 100 and 150/175/200 → 125. The UI must never silently expose an unsupported value.
- OpenDSS has one light application theme. There is no application dark mode or theme selector. Dark surfaces are limited to Camera, image, crop, and sequence-viewer canvases where image contrast requires them.

### Results

- Results does not show the visible **Provenance ›** disclosure row. Stored scientific provenance and artifact facts remain present in their existing factual groups and persisted artifacts; this removes only the redundant visible row.

## Implementation consequence

The current visual slice authorizes only deterministic visual/mock seams for these decisions. Real file copy, Dataset loading and persistence, labeling, filtering, Training or Model Test execution, Settings persistence, and application-wide scaling require following explicitly bounded functional-integration work.

This amendment freezes existing exported QML aliases, properties, signals, state names, and the ordinary-wrapper/C++ ownership boundary. It authorizes no new runtime handlers, persistence, backend calls, hardware calls, hidden functional behavior, or replacement interface seams.

The Sequence Viewer jump controls and frame slider, Label pagination controls, and Train **Load Weights** button remain visual IDs only in this pass; they are deliberately not exported or wired here. A later functional work order must add and wire any accepted interface atomically. Live retains its existing `primaryActionButton` and `secondaryActionButton` seam, and Capture retains the existing Camera-action visual ID and seam.

Camera **Bit Depth** and **Readout mode** may use one explicitly illustrative adapter mock showing **8-bit**, **12-bit**, and **16-bit**, and **Fast** and **Slow**, respectively. Production options remain adapter-derived; Hardware must not add placeholder controls or claim universal support. A two-handle LUT RangeSlider is approved for preview-presentation minimum and maximum only. It does not affect TIFF values, Droplet Crop generation, detector input, or model input, and its functional interface is deferred.

All physical DAQ output is sine-wave output; no pulse or other physical waveform mode is defined. Hardware > DAQ includes **Amplitude** from 0–10 Vpp, default 5 Vpp, centered at 0 V with extrema `-Vpp/2` and `+Vpp/2`; **Frequency** from 1–1000 kHz, default 10 kHz; **Event Duration** from 1–500 ms, default 5 ms; and **Decision-to-trigger Delay** from 0–500 ms, default 0 ms. Visual increments are 1 Vpp, 1 kHz, 1 ms, and 1 ms respectively. Amplitude and Frequency apply to continuous, event-triggered, and test sine output. Event Duration applies only to event-triggered and test finite sine waves. Decision-to-trigger Delay is measured from an accepted `Decision = Hit` and applies only to decision-triggered output, not Send Test Sine Wave.

Valid settings apply immediately. Supported Amplitude and Frequency edits retune active continuous output immediately; unsupported values or live retunes are rejected with a direct explanation while the last applied value remains active. Event Duration and Decision-to-trigger Delay changes apply only to future, not-yet-issued event output. Continuous output runs until Stop, application exit, DAQ disconnect, or DAQ fault, then resets and returns output to 0 V. It has priority over event-triggered finite sine waves; suppressed output is recorded as **suppressed / not issued** and discarded, never queued. This visual authority does not implement or authorize changes to protected NI-DAQmx mechanics; those require a separate bounded functional work order plus characterization, regression, performance, hardware-in-the-loop, justification, and rollback evidence.

Future visual work orders must read this amendment when touching the listed topics, implement its stated visual behavior, and report a conflict with the Product Model rather than extending scope.
