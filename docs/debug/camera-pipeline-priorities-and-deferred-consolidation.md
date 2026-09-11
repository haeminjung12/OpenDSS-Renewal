# Camera Pipeline Priorities and Deferred Consolidation

Status: user decisions accepted; sequential debug implementation authorized

Recorded: 2026-08-01

This is a planning note, not canonical product authority. Implementation proceeds through one stable `DBG-*` record at a time under the controlled master and current-slice authority.

Accepted decisions:

- Rename the visible `LUT` section to `Contrast`.
- Lock exposure and contrast while Image Sequence or Dataset Capture owns the Camera, allowing one truthful effective Start snapshot.
- Begin iterative Auto Exposure characterization with an unadjusted-frame p95 target near `180`, plus clipping, convergence, bounds, timeout, and cancellation safeguards.

## Immediate priority fixes

These behavior fixes take priority over the API/consolidation work described later in this document.

### 1. Auto contrast control

- Add an explicit Auto Contrast button to the camera workflow.
- The control must calculate and apply useful contrast settings from camera image data rather than requiring the user to tune contrast manually.
- Add numeric input boxes for the manual contrast LOW and HIGH values alongside the existing contrast slider/range control.
- The LOW input must remain bidirectionally synchronized with the slider's low value, and the HIGH input must remain bidirectionally synchronized with the slider's high value. Changing either representation must immediately update the other and apply the resulting contrast setting.
- Numeric entry and slider movement must use the same bounds, ordering constraint, step size, and precision. Invalid, out-of-range, or crossed LOW/HIGH values must be handled consistently without allowing the displayed controls and effective processing values to diverge.
- The exact contrast objective, limits, and interaction with manual controls still require characterization against the current camera and processing contracts.

### 2. Iterative auto exposure

- Auto exposure must not make its decision from only one image at one exposure value.
- It must operate as a bounded feedback process: apply an exposure candidate, acquire/evaluate a resulting frame or frames, adjust the exposure, and repeat until it reaches a defined target, limit, or convergence condition.
- The implementation should use an established exposure-evaluation approach, with the objective and safeguards selected after characterizing the current camera behavior. Likely evaluation inputs include intensity distribution and clipped-pixel behavior, but no algorithm is selected by this note.
- Hardware-dependent behavior, convergence limits, cancellation, and failure handling must be defined before implementation.

### 3. Contrast is part of the processing pipeline

- Applied contrast must not be a display-only effect.
- The contrast-adjusted image must be the image used by downstream processing, including detection, crop generation, and saved collection artifacts.
- The pipeline must have one clear authoritative adjusted-image boundary so the preview, detector input, crops, and saved outputs do not silently use different image representations.
- Existing detector, inference, camera-acquisition, and persistence behavior remains protected until the active debug record explicitly authorizes and characterizes the change.

### 4. Collection-time camera and image-adjustment provenance

- Dataset collection output must record the effective camera and image-adjustment settings used for the collected data.
- At minimum, this includes the effective exposure and contrast configuration. The implementation investigation must identify all other active camera settings that materially describe acquisition, such as gain or other supported controls, and record the actual effective values rather than only requested UI values.
- Store these values in the dataset drop metadata/file so a user can retrieve the collection settings later.
- This metadata is informational provenance only. Training, dataset validation, model loading, and inference compatibility checks must not reject, warn on, or otherwise fail because current settings differ from the recorded collection settings.
- Older datasets without these fields must remain usable.

## Intended sequencing

The immediate fixes above should be characterized and implemented before the cleanup work below. They are separate behaviors and should not be treated as one unbounded implementation change. Their final grouping into stable `DBG-*` records depends on the actual ownership boundaries and test seams found during investigation.

## Deferred API and coherence work

The following consolidation candidates were identified and intentionally deferred until the immediate camera-pipeline fixes are complete:

1. Extract the common deterministic Live/Sequence Test run-event state machine while leaving camera acquisition, detector invocation, Live threading/persistence queues, Sequence Test scheduling, and DAQ dispatch with their current owners.
2. Establish one prepared inference-model contract, preferably owned by the existing model-loading domain, instead of parallel Live, Sequence Test, and Model Test representations.
3. Consolidate artifact I/O primitives: no-replace publication first, then carefully policy-scoped atomic writes and file SHA-256.
4. Consolidate artifact-folder naming/sanitization and maintained Qt CSV encoding/parsing semantics.
5. Resolve model-registry ownership by routing the legacy workspace path through the existing registry service or retiring the legacy path; do not create a third registry API.

### Explicit non-goals for the deferred work

- Do not extract trivial global error, failure, or timestamp helpers merely to reduce line count.
- Do not touch duplicated generated or Qt Design Studio dependency QML.
- Do not introduce speculative factories, registries, strategy selectors, service locators, compatibility layers, or unused APIs.
