# OpenDSS Visual Validation Checklist

Date: 2026-07-23

Use this checklist in Qt Design Studio 2D View and Live Preview. Compare against the baseline screenshots in `workspace-screenshots-2026-07-23-before-refinements/`.

## Window and scaling

- [x] Application opens maximized.
- [x] Content fills the complete window, including the bottom.
- [x] Application is reviewed in a maximized window using the current display's full available work area.
- [x] Validation remains in the maximized window; restored wide/tall aspect-ratio checks are removed.
- [x] No fixed aspect-ratio or restored-window validation is required.
- [x] Text Size Medium (100%) has no overlaps or clipping.
- [ ] Text Size Large (125%) has no overlaps or clipping.
- [ ] The separate 200% validation condition has no overlaps or clipping.
  화면 캡처 2026-07-23 231514.png
  화면 캡처 2026-07-23 231542.png
  ->Overall I think the defaul text size is way too small
- [x] Headers remain separate from expand/collapse chevrons at every text size.
- [ ] Buttons and fields grow proportionally with text.
  화면 캡처 2026-07-23 231623.png -> Font too small at 200%
- [ ] Overflowing content scrolls instead of being clipped.
  화면 캡처 2026-07-23 231708.png -> It doesn't scroll correctly. It is scrollable, but i think if the mouse pointer is over a textbox the scroll doesn't work

## Adjustable panels

- [x] The left navigation panel can be resized by dragging.
- [x] Each workspace right panel can be resized by dragging.
- [x] Right panels still collapse and expand correctly after dragging.
- [x] At 100% Text Size, right panels initially open at approximately 390 px.
- [x] Default navigation and right-panel widths grow with Text Size.
- [x] Closing and relaunching resets dragged panel widths.
- [x] Dragged panel widths are not persisted between launches.
  

## Global shell

- [x] The header remains one compact line.
  I think the header is too fat vertically?
- [x] Camera status is readable.
- [x] DAQ status is readable.
- [x] Active Model status is readable.
- [x] Current Activity status is readable.
- [x] Status items do not overlap at Small, Medium, or Large; the separate 200% validation condition also remains readable.
- [x] Redundant status wording such as `Ready (ready)` is absent.
- [ ] The sidebar uses the approved quiet light treatment.
  -explain
- [ ] The selected navigation item has a restrained blue marker.
  화면 캡처 2026-07-23 232106.png-> Yes but the white text is not readable
- [x] Clicking the selected navigation item does not deselect it.
- [x] Hardware remains a bottom-left overlay.
- [ ] Hardware uses a chevron and title without `Expanded` or `Collapsed`.
  ->화면 캡처 2026-07-23 232141.png Still has the redundant "close" button
- [ ] A meaningful `Disabled` cue remains when Hardware is unavailable.
  -explain
- [ ] Hardware does not obscure important workspace controls.
  -> Actually let's just make it a part of the left section so that it is not over the top window. Keep everything like as is but just the location of the drawer and it should match the size of the left pannel

## Capture

- [x] The Camera preview fills the available main area.
- [x] No empty strip appears at the bottom of the workspace.
- [x] The Capture right panel is draggable.
- [x] The Capture right panel collapses and expands correctly.
- [x] Single Image heading remains visible.
- [x] Image Sequence heading remains visible.
- [x] Droplet Dataset Capture heading remains visible.
- [x] Multiple idle sections may be expanded.
- [x] Each expanded body uses only its required height.
- [x] The next section header follows the expanded body immediately.
- [x] Unused panel space remains below the final section.
- [x] Primary Capture actions use blue styling.
- [x] Stop actions use destructive-red styling.
- [x] Camera-unavailable presentation is readable and unclipped.
- [x] Camera prompt controls do not overlap at enlarged Text Size.

-화면 캡처 2026-07-23 232850.png, 화면 캡처 2026-07-23 232811.png. -> These two have very awkward buttons. It should be the same size as "Start recording " combined, while the pause button appears on the right side about 1/4th the size of stop. 
-All notes section is inconsistant text box compared to other boxes
-Duration should be a box:box:box format in hh:mm:ss only letting number input


## Label

- [ ] The Droplet Crop grid remains the dominant workspace region.
  grid does NOT fill the entire screen
- [ ] The crop grid remains contained at Medium Text Size.
- [ ] The crop grid remains contained or scrollable at Large Text Size.
- [ ] The crop grid remains contained or scrollable under the separate 200% validation condition.
  화면 캡처 2026-07-23 233241.png -> Grid should adjust to the pannel size and the image size should stay
- [x] The right panel is draggable.
- [x] The right panel collapses and expands correctly.
- [x] Panel order is Load Dataset, Dataset Summary, Label, Filter, Save As.
- [x] Load Dataset remains a static, always-expanded card.
- [x] Two-class mode keeps Class 2 visible.
- [x] In two-class mode, Class 2 is grey and disabled.
- [x] In three-class mode, Class 2 is purple and enabled.
- [x] Class 0 remains blue.
- [x] Class 1 remains orange.
- [x] Class 2 remains purple when enabled.
- [x] Exclude, Undo, Previous, and Next remain present.
- [x] Skip, Remove, and Restore are absent.
- [ ] Empty Dataset presentation is compact and understandable.
  -explain
- [x] Save As remains at the bottom-right of the panel.
  
  -The "Selected crop" image should be draggable downwards to enlarge/shink the image. Make the placeholder a fixed square.
  -Next/Prev button should have a arrow, and some distingtion with undo. put icons for all 3 
  
  ## 
  
  ## Sequence Viewer

- [x] The viewer remains the dominant workspace region.
- [x] The controller is at the bottom of the workspace.
- [x] The first-row order is `-50`, `-10`, Previous, Next, `+10`, `+50`, slider, frame count.
- [x] Direct frame entry appears below the primary controller row.
- [ ] Zoom − remains visible.
- [ ] Zoom + remains visible.
- [ ] Fit remains visible.
- [ ] 1:1 remains visible.
- [x] The controller remains accessible when Hardware is closed.
- [x] The controller remains accessible when Hardware is open.
- [ ] Large-text controls scroll instead of overlapping.
  ->??
- [ ] Empty, ready, large-count, and error presentations remain aligned.
  ->??

> Functional note: `-50`, `-10`, `+10`, `+50`, and the slider are visual-only in this DESIGN pass. Their aliases, authoritative frame state, clamping, and event wiring belong to a later atomic functional integration.

화면 캡처 2026-07-23 233806.png. The buttons are clipped. Since we are removing the hardware drawer, center align everything

## Train

- [x] Dataset Summary remains a distinct main white region.
- [x] Results remains a separate white region below Dataset Summary.
- [x] The right panel is draggable.
- [x] The right panel collapses and expands correctly.
- [ ] Training Setup disclosure remains usable.
  ->?
- [ ] Training Status disclosure remains usable.
  ->?
- [ ] The approved two live plots remain visible.
  ->화면 캡처 2026-07-23 234756.png no plots
- [x] Completion tables remain visible.
- [x] Model Name uses a light field treatment.
- [x] Save Location uses a light field treatment.
- [ ] Start Training uses primary-blue styling.
  ->its blocked so could not validate
- [ ] Stop Training uses destructive-red styling.
- [ ] Blocked or empty states provide a concise reason.
  
  
  Model Type should show the model name, also should be able to select blank imagenet weights or pre-trained weights. This is kind of complex where we will need model type, and weight type selector. for now have 2 separate selection list not boxes like currently

## Model Test

- [x] Dataset Summary remains a distinct main white region.
- [x] Results remains a separate white region below Dataset Summary.
- [x] The right panel is draggable.
- [x] The right panel collapses and expands correctly.
- [x] Test Setup disclosure remains usable.
- [x] Model Test Running disclosure remains usable.
- [x] Metrics remain visible.
- [x] The confusion matrix remains visible.
- [x] Prediction summaries remain visible.
- [x] Output Location uses a light field treatment.
- [ ] Start uses primary-blue styling.
- [ ] Stop uses destructive-red styling.
- [ ] Missing-model or Dataset prerequisites provide a concise reason.

## Model Library

- [x] The Model list remains the dominant main region.
- [x] The Selected Model panel is draggable.
- [x] The Selected Model panel collapses and expands correctly.
- [x] Selected and Active Model states are visually distinct.
- [x] Active Model has a checkmark plus readable or accessible meaning.
- [x] Set Active is visually primary.
- [x] Delete is visually separated and destructive red.
- [x] Locked actions remain visibly unavailable.
- [ ] The empty-library presentation is compact and clear.
  ->?
- [x] Long metadata and paths do not overlap headers.
  
  -화면 캡처 2026-07-23 235355.png -> It breaks overall in high font setting
  -Model type should show the architecture not "Faster"

## Live

- [x] The Camera/viewer fills the available main region.
- [x] The right panel is draggable.
- [x] The right panel collapses and expands correctly.
- [x] Setup sections scroll above the fixed footer.
- [x] The footer remains fixed at the bottom of the right panel.
- [x] Camera off shows Start Camera.
- [x] Camera off shows disabled Start Sorting.
- [x] Camera on shows Stop Camera.
- [x] Camera on shows readiness-gated Start Sorting.
- [x] Running shows Pause and destructive-red Stop.
- [x] Paused shows Resume and destructive-red Stop.
- [x] Completed shows Start New Run and Open Run Summary.
- [ ] Error and unavailable states do not show irrelevant actions.
- [ ] Trigger Every Droplet and DAQ Output appear as independent controls.
- [x] Run Status is fixed information rather than an expandable section.
- [x] Hardware does not obscure the footer.

> Functional note: authoritative Trigger Every Droplet, DAQ Output, Active Model, Hit Class, and readiness behavior still belongs to functional state and wrapper integration.

-right panel, the headers do NOT take up the entire space it should only takes up half 화면 캡처 2026-07-23 235821.png
-Trigger timing should be enabled during active runs
-"DAQ Always On" or something like that that turns on the DAQ sinewave continuously should be next to send test pulse button as another button.

## Sequence Test

- [x] The first-frame preview remains visible.
- [x] Results remains a separate visible region.
  -> Yes, but the two section should also be draggable
- [x] The right panel is draggable.
- [x] The right panel collapses and expands correctly.
- [x] The right panel scrolls when its content exceeds available height.
- [x] Load and memory states remain understandable.
- [ ] Disabled controls remain understandable without relying on color alone.
  -> cannot enable "Start", and no explanation is given
- [ ] Error presentation remains clear and contained.
- [ ] No empty strip appears at the bottom.
  ->화면 캡처 2026-07-24 000225.png There is

-화면 캡처 2026-07-24 000059.png. Same issue as live, right pannel headers does not take up the whole space

## Runs

- [x] The selected Run content fills the main region vertically.
- [x] The Runs panel is draggable.
- [x] The Runs panel collapses and expands correctly.
- [x] Selected and loaded Run states remain visually distinct.
- [x] Load Selected Run is visually primary.
- [ ] No Runs Found presentation is compact and clear.
- [ ] No Run Loaded presentation is compact and clear.
- [ ] Completed and stopped presentations remain aligned.
- [x] Notes editing remains contained.
- [ ] Error presentation remains clear and contained.
- [x] No empty strip appears at the bottom.
  
  

->Overall broken with high font 화면 캡처 2026-07-23 235715.png, 화면 캡처 2026-07-24 000254.png
-wtf is "provenance", remove

## Settings

- [x] Exactly four sections appear.
- [x] Storage is present.
- [x] Application Information is present.
- [x] Diagnostics is present.
- [x] Visuals is present.
- [x] Every control remains inside its section border.
- [x] Paths look read-only rather than broken or disabled.
- [x] Visuals contains only Text Size.
- [x] Text Size choices are Small (80%), Medium (100%), and Large (125%).
- [x] Text Size default is Medium (100%).
- [x] 200% is validation-only and is not a selectable preference.
- [x] Settings remains contained at Medium Text Size.
- [x] Settings scrolls cleanly at Large Text Size.
- [x] Settings scrolls cleanly under the separate 200% validation condition.

## Final regression checks

- [ ] No accepted alias, property, signal, or state name appears to be missing.
- [ ] No new speculative control or workflow appears.
- [ ] No production Camera, DAQ, filesystem, persistence, Training, inference, or hardware behavior was added to a visual form.
- [ ] All edited forms remain usable in Qt Design Studio 2D View.
- [ ] All edited forms render in Live Preview.
- [ ] Baseline and refined screenshots can be compared workspace by workspace.

overall WHAT??? aren't these things what you have to worry about?

## Issues and notes

Record the workspace, Text Size, display/work-area dimensions, state, and a short description for every failed check. Keep the application maximized.



Screenshots are in
C:\Users\goals\OneDrive\Documents\OpenDSS Renewal\docs\v2\screenshots


화면 캡처 2026-07-23 233314.png, Overall the collapsed panel does not look good. A center aligned arrow that changes directions for open/close that stays in the same potition is more intuitive

| Workspace | Text Size | Maximized work area | State | Issue |
| --------- | ---------:| ----------- | ----- | ----- |
|           |           |             |       |       |
|           |           |             |       |       |
|           |           |             |       |       |
|           |           |             |       |       |
|           |           |             |       |       |
