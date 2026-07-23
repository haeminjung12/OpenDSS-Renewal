# OpenDSS v2 UI/UX Visual Review Amendment — 2026-07-23

## Status and authority

**User-approved visual-review amendment.** This amendment is subordinate to the [Approved Product Model](canonical/product-model.md) for product policy and scope. For the topics below, it controls and supersedes conflicting UI layout, naming, disclosure, and composition language in the [Approved UI/UX Design Amendment](OpenDSS_v2_UIUX_Design_Amendment.md), Information Architecture, consolidated design drafts, and review artifacts.

It authorizes no production Camera, DAQ, vendor, filesystem, persistence, detector, inference, trainer, Run, Results, or other production behavior.

## Approved visual-review decisions

### Shared disclosure and shell behavior

- Almost all workspace right panels use the shared collapsible disclosure-header treatment, with actual click-to-collapse behavior. The bottom-left Hardware overlay uses the same header language.
- Workspace outer right panels use one consistent expanded width, represented by the current **390 px** implementation token, and collapse as a whole. Inner disclosure sections retain intrinsic-height stacking where applicable.
- An active primary-navigation item remains selected when clicked; it cannot toggle itself off.
- Every expanded right-panel body, including Capture, takes only its intrinsic needed height. The next header follows immediately; unused space stays below. Local scrolling appears only when real content exceeds available height. This supersedes any Capture equal-remaining-height direction.
- All ten approved destinations use one consistent workspace-title treatment and naming.

### Sequence Viewer

- Keep the controls centered and unobstructed by Hardware.
- Row 1 is Previous, current/total, direct timeline/seek, and Next. Row 2 is zoom controls, Fit, and 1:1.

### Label

- The Droplet Crop grid remains dominant in the main area.
- A fixed-width right panel collapses as one outer panel and contains, in order:
  1. **Load Dataset**, a static, always-expanded card/header;
  2. **Dataset Summary**, with total and labeled counts plus initial setup for exactly two or three classes; a configured Dataset instead shows its immutable class schema with no class-count switching;
  3. **Label**, with the selected-crop preview and exactly **Class 0**, **Class 1**, **Class 2**, **Exclude**, **Undo**, **Previous**, and **Next**; Class 2 is disabled for a two-class Dataset;
  4. **Filter**, with class list/count filters plus **Excluded** and **Unreviewed** when applicable;
  5. **Save As** at the bottom-right.
- Class identity remains blue, orange, and purple for Classes 0, 1, and 2. **Skip**, **Remove**, **Restore**, and other former Label-side actions are not part of this composition.
- Dataset **Save As** creates an independent copy and makes it the current loaded Dataset. It is not version history; normal changes continue saving to the current Dataset under the existing persistence contract.

### Train and Model Test

- Train and Model Test keep **Dataset Summary** in a main white region and place **Results** in a separate main white region below.
- Train Results uses the approved two live plots and completion tables. Model Test Results uses the approved metrics, confusion matrix, and prediction summaries. This placement introduces no new data semantics.

### Live and Settings

- Approved Live disclosure bodies show their source-grounded content, not placeholder headings.
- Settings controls remain inside their section borders. Settings contains only **Storage**, **Application Information**, **Diagnostics**, and **Visuals**.
- Visuals contains only application-wide **Text Size**, from **80%** through **200%**, default **100%**.

## Implementation consequence

The current visual slice authorizes only deterministic visual/mock seams for these decisions. Real file copy, Dataset loading and persistence, labeling, filtering, Training or Model Test execution, Settings persistence, and application-wide scaling require following explicitly bounded functional-integration work.

Future visual work orders must read this amendment when touching the listed topics, implement its stated visual behavior, and report a conflict with the Product Model rather than extending scope.
