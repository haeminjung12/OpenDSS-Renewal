# OpenDSS v2 UI/UX Visual Review Amendment — 2026-07-23

## Status and authority

**User-approved visual-review amendment.** This amendment is subordinate to the [Approved Product Model](canonical/product-model.md) for product policy and scope. For the topics below, it controls and supersedes conflicting UI layout, naming, disclosure, and composition language in the [Approved UI/UX Design Amendment](OpenDSS_v2_UIUX_Design_Amendment.md), Information Architecture, consolidated design drafts, and review artifacts.

It authorizes no production Camera, DAQ, vendor, filesystem, persistence, detector, inference, trainer, Run, Results, or other production behavior.

## Approved visual-review decisions

### Shared disclosure and shell behavior

- Almost all workspace right panels use the shared collapsible disclosure-header treatment, with actual click-to-collapse behavior. The bottom-left Hardware overlay uses the same header language.
- An active primary-navigation item remains selected when clicked; it cannot toggle itself off.
- Every expanded right-panel body, including Capture, takes only its intrinsic needed height. The next header follows immediately; unused space stays below. Local scrolling appears only when real content exceeds available height. This supersedes any Capture equal-remaining-height direction.
- All ten approved destinations use one consistent workspace-title treatment and naming.

### Sequence Viewer

- Keep the controls centered and unobstructed by Hardware.
- Row 1 is Previous, current/total, direct timeline/seek, and Next. Row 2 is zoom controls, Fit, and 1:1.

### Label

- **Selected Crop** presents the fixed 64 × 64 artifact as a large square, unclipped `PreserveAspectFit` preview.
- Always show Class 0, Class 1, and Class 2 buttons; disable Class 2 in two-class mode. Place the prominent class grid beneath the preview and retain the blue, orange, and purple class identity.
- Keep Skip, Remove, Restore, and Undo as secondary actions below the class grid. The full Selected Crop body collapses; **Classes & Filter** remains a separate disclosure section.

### Live and Settings

- Approved Live disclosure bodies show their source-grounded content, not placeholder headings.
- Settings controls remain inside their section borders.

## Implementation consequence

Future visual work orders must read this amendment when touching the listed topics, implement its stated visual behavior, and report a conflict with the Product Model rather than extending scope.
