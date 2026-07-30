# OpenDSS Final Visual Acceptance Checklist

**Date:** July 24, 2026  
**Review tool:** Qt Design Studio  
**Project:** `app/runtime/Desktop_app_v2/Desktop_app_v2.qmlproject`  
**Primary form:** `app/runtime/Desktop_app_v2/Desktop_app_v2Content/Screen01.ui.qml`

Use Qt Design Studio 2D View or Live Preview in a maximized window using the current display's full available work area and, when restored behavior is relevant, at exactly 1600 × 900 logical px or larger, with **Text Size: Medium**. Never validate below 1600 × 900. Check only what is listed below; build, lint, interface preservation, and the five-second runtime smoke have already passed.

## 1. Text Size and typography

- [ ] In **Settings → Visuals**, the dropdown contains exactly **Small**, **Medium**, and **Large**.
- [ ] The dropdown does not show percentages, 150%, 175%, 200%, or a separate Regular option.
- [ ] **Medium** is selected by default.
- [ ] At Medium, ordinary text, control text, and button labels are comfortably readable and no longer look undersized.
- [ ] Setting labels are visually consistent with their associated fields and controls.
- [ ] Captions, warnings, status text, and metadata remain readable but are visually secondary.
- [ ] **Small** reduces text without clipping labels, arrows, fields, or buttons.
- [ ] **Large** enlarges text without overlapping disclosure arrows, panel titles, fields, or buttons.
- [ ] At Large, long content wraps or scrolls instead of being cut off.
- [ ] Switching Small → Medium → Large does not cause the outer-panel arrow to grow.

## 2. Collapsible workspace panels

- [ ] Collapse the **Capture** panel: only the small vertically centered arrow remains; there is no empty white strip beside it.
- [ ] Expand Capture again: the panel returns at its normal adjustable width and the title is visible.
- [ ] Collapse and expand the **Label** panel: the same no-gap behavior is present.
- [ ] Spot-check **Train, Model Test, Library, Live, Sequence Test, and Runs**: each collapsed panel leaves only the fixed arrow and no extra gap.
- [ ] In every checked workspace, the arrow remains fixed at the panel’s right edge and vertical center.
- [ ] Drag an expanded panel boundary narrower and wider; the panel remains adjustable and its contents stay usable.

## 3. Capture workspace

- [ ] With the camera stopped, **Start Camera** is presented as a clear blue primary button.
- [ ] In a streaming design/runtime state, the same action becomes **Stop Camera** and is destructive red.
- [ ] **Capture Image**, **Start Recording**, and **Start Droplet Dataset Capture** use the same blue primary-action treatment.
- [ ] Sequence and Dataset **Stop** actions use the same destructive-red treatment.
- [ ] The **Droplet Dataset Capture** fields have comfortable vertical spacing and no longer feel compressed together.
- [ ] Dataset Name, Experiment Type, Notes, Duration, Save Location, supporting text, and the start action remain in the approved order.
- [ ] The Dataset Capture section scrolls when needed and does not run underneath the fixed camera action footer.
- [ ] Expanding Dataset Capture does not hide or overlap the next accordion header.

## 4. Label workspace

- [ ] **Class 0**, **Class 1**, and **Class 2** buttons now share the same shape, padding, typography, focus border, and overall component styling.
- [ ] Class identity remains clear: Class 0 blue, Class 1 orange, and Class 2 purple.
- [ ] With **3 classes** selected, Class 2 is enabled and purple.
- [ ] With **2 classes** selected, Class 2 remains visible but becomes disabled grey.
- [ ] Disabled Class 2 text remains readable and does not appear purple.
- [ ] The class buttons remain distinct from **Exclude**, **Undo**, **Previous**, and **Next**.
- [ ] Keyboard focus is visibly distinguishable from the class identity color.

## 5. Shared-component consistency

- [ ] Buttons in Capture, Label, Train, Model Test, Live, Sequence Test, and Runs look like members of the same component system.
- [ ] Text fields and text areas use consistent borders, padding, fonts, and disabled treatment.
- [ ] Dropdowns use consistent height, text alignment, arrow placement, popup rows, and selected-row treatment.
- [ ] Checkboxes, switches, radio buttons, spin boxes, progress bars, and navigation items look consistent wherever they appear.
- [ ] Accordion headers use consistent height, typography, disclosure arrows, borders, and expanded/collapsed presentation.
- [ ] Disabled controls are visibly grey and remain readable.
- [ ] Focused controls have a clear focus border that is not confused with error, primary-action, or class colors.

## 6. Final window and scrolling check

- [ ] In the **maximized window / Medium**, the application fills the available work area with no unused bottom region.
- [ ] Keep the window maximized throughout the review; do not run restored-window or fixed-resolution checks.
- [ ] Navigation, Hardware Configuration, workspace content, and right panels scroll independently where applicable.
- [ ] No sticky action footer covers the last field or button when scrolling.
- [ ] No panel title, accordion title, dropdown text, or button label is visibly clipped.
- [ ] No control unexpectedly changes size when an unrelated panel is collapsed or expanded.

## 7. Acceptance

- [ ] I accept the final visual baseline for backend integration.
- [ ] Any remaining issue is written below with its workspace, Text Size, panel state, and a screenshot.

## Notes

- Workspace:
- Text Size:
- Expanded/collapsed state:
- Problem:
- Screenshot filename:

