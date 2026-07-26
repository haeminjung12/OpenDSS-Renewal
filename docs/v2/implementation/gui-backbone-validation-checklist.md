# OpenDSS v2 GUI–Backbone Validation Checklist

Use this checklist to validate the current OpenDSS v2 GUI integration in Qt Creator.

## Validation baseline

- [ ] Repository branch is `codex/splitview-maximum-width-guards`.
- [ ] Repository includes integration commit `f8a0a02`.
- [ ] Build configuration is **Debug** with Qt **6.11.1 MSVC 2022 64-bit**.
- [ ] `C:\Qt\6.11.1\msvc2022_64\bin` is present in `PATH`.
- [ ] The application builds without compiler or linker errors.
- [ ] The new GUI opens without a startup **Camera unavailable** modal.
- [ ] No missing `Qt6QuickTest.dll` system-error window appears.

Previously validated executable:

```text
C:\b\odss-v2-final\Debug\Desktop_app_v2App.exe
```

## Visual-regression checks

No `*.ui.qml` visual forms were changed by the final integration.

- [ ] Existing workspace layout is unchanged.
- [ ] Existing colors, typography, spacing, borders, icons, and tokens are unchanged.
- [ ] Navigation still opens every existing workspace.
- [ ] The application is maximized and fills the current display's full available work area.
- [ ] Keep the window maximized throughout validation; do not restore or resize it to a fixed test resolution.
- [ ] Collapsed and expanded side panels retain their previous appearance.
- [ ] No panel overlaps, clipped headings, or unexpected horizontal scrollbars appear.

## Camera hardware GUI

Validate these items with the production application controller, not a design-time mock.

- [ ] The Camera area does not say **mock**, **mock only**, or **Illustrative Camera**.
- [ ] Camera status is factual: unavailable, opening, streaming, stopped, or faulted.
- [ ] The detected camera/device identifier is shown when available.
- [ ] A factual inline error is shown when camera discovery or streaming fails.
- [ ] **Start Camera** requests the real camera controller to open and stream.
- [ ] A live preview appears after valid frames arrive.
- [ ] **Stop Camera** stops the real camera stream.
- [ ] **Recover Camera** retries the production camera controller after a fault.
- [ ] Unsupported camera-configuration controls remain disabled instead of changing mock values.
- [ ] Starting the application without a camera does not open a blocking startup modal.
- [ ] During Live Starting, Running, Paused, or Stopping, camera hardware actions are disabled.
- [ ] Camera hardware actions become available again after Live releases the operation.

## DAQ hardware GUI

- [ ] The DAQ area does not say **mock** or **mock only**.
- [ ] DAQ status is factual: unavailable, discovering, ready, or faulted.
- [ ] The detected DAQ device identifier is shown when available.
- [ ] The configured output/counter channels are shown.
- [ ] **Refresh** performs real DAQ discovery.
- [ ] Amplitude/Vpp changes are sent to the production DAQ controller.
- [ ] Frequency changes are sent to the production DAQ controller.
- [ ] Duration changes are sent to the production DAQ controller.
- [ ] Delay changes are sent to the production DAQ controller.
- [ ] A failed apply can be retried without restarting the application.
- [ ] DAQ Refresh is disabled while Live owns the operation.
- [ ] DAQ Refresh is disabled during a DAQ-enabled Sequence Test.
- [ ] DAQ Refresh remains available during a Sequence Test with physical DAQ output OFF.

## Live Sorting

- [ ] Live uses the real Camera controller and received camera frames.
- [ ] The first valid frame enables dimension-dependent Start readiness.
- [ ] A camera dimension or bit-depth change refreshes Live readiness.
- [ ] The Active Model shown in Live matches the authoritative model registry.
- [ ] Models with reordered, numeric, or textual class IDs show the correct Hit Class.
- [ ] Selecting a Hit Class sends its exact class ID, not its row number.
- [ ] **Trigger Every Droplet** and **DAQ Output** remain independent.
- [ ] DAQ Output OFF permits Start when all non-DAQ prerequisites pass.
- [ ] DAQ Output ON blocks Start until DAQ is Ready.
- [ ] DAQ Output ON uses the production DAQ hit-pulse callback.
- [ ] **Record Full Image Sequence** can be enabled or disabled.
- [ ] **Start Sorting** begins a real Live operation.
- [ ] **Pause** stops new processing while preserving the accepted operation state.
- [ ] **Resume** continues the same Run.
- [ ] **Stop** finalizes without freezing the GUI.
- [ ] Timed completion does not freeze the GUI.
- [ ] Live runtime status becomes visible while Running.
- [ ] Elapsed time updates.
- [ ] Persisted event count updates.
- [ ] Integrity status is factual.
- [ ] Diagnostic text is factual.
- [ ] Output/DAQ status is factual.
- [ ] Final outcome is shown after completion or failure.
- [ ] The Run artifact path is shown after creation.
- [ ] Runs/Results refreshes after Live finalization.
- [ ] Closing the application during an operation does not produce a false DAQ failure.

### Live full-sequence artifact checks

When full-sequence recording is enabled, inspect the produced Run:

- [ ] `run_summary.json` exists only for a durably finalized Run.
- [ ] `events.csv` exists and is readable.
- [ ] `sequence/sequence.json` exists and is readable.
- [ ] Full frames are stored under `sequence/frames/`.
- [ ] The Run summary references the sequence manifest.
- [ ] Run and sequence final statuses agree.
- [ ] A failed sequence publication does not leave a falsely completed Run summary.

## Sequence Test

Two prepared test sequences are available:

```text
D:\[2026] Visual Sorting Data\0226 Final please\20260226_163405_BEST SO FAR\sequence.json
D:\[2026] Visual Sorting Data\0226 Final please\20260226_173838\sequence.json
```

- [ ] **Load Sequence** opens a file dialog for `sequence.json`.
- [ ] Selecting a manifest shows the source folder.
- [ ] The first-frame preview appears.
- [ ] Frame count is shown.
- [ ] Recorded FPS is shown.
- [ ] Processing FPS defaults from recorded FPS.
- [ ] Processing FPS remains editable before Start.
- [ ] **Load to Memory** performs asynchronous loading.
- [ ] The GUI remains responsive during memory loading.
- [ ] Successful loading says **Ready in memory**.
- [ ] Memory/load failure shows a direct factual error.
- [ ] Start remains disabled until memory loading succeeds.
- [ ] The Active Model matches the authoritative model registry.
- [ ] Reordered, numeric, or textual class IDs select the correct Hit Class.
- [ ] Trigger/routing selections are passed to the real controller.
- [ ] DAQ Output defaults OFF.
- [ ] DAQ Output OFF does not require DAQ readiness.
- [ ] DAQ Output ON requires DAQ Ready.
- [ ] DAQ Output ON issues production DAQ hit pulses.
- [ ] **Start** begins processing from the accepted in-memory snapshot.
- [ ] Requested processing FPS controls processing pace.
- [ ] Achieved FPS updates from completed frames.
- [ ] Progress and processed-frame count update.
- [ ] **Stop** ends processing without freezing the GUI.
- [ ] Results refresh after completion or Stop.

### Sequence provenance checks

- [ ] The Run archives `source/sequence.json`.
- [ ] Archived manifest bytes match the manifest accepted at memory-load completion.
- [ ] Later compatible edits to the source manifest do not alter the accepted Run facts.
- [ ] Removing or changing source frames after a successful memory load does not alter already-loaded frames.
- [ ] Run Summary model, FPS, classes, detector settings, and source facts are truthful.

## Models and training

- [ ] Model Library displays registered packages.
- [ ] Selecting a row does not silently activate it.
- [ ] **Activate** updates the authoritative Active Model.
- [ ] Import registers a valid Model Package atomically.
- [ ] Export produces a complete Model Package.
- [ ] Duplicate creates a distinct package identity.
- [ ] Delete removes the selected package without corrupting the registry.
- [ ] Model Test uses the Active Model.
- [ ] Training completion registers the exported package.
- [ ] A successfully trained package becomes available in Model Library.
- [ ] Training/package-registration failure presents a retry path.
- [ ] Python training remains behind the application controller boundary; QML does not call Python directly.

## Other integrated workspaces

- [ ] Settings storage-root selection persists.
- [ ] Text Size exposes only Small 80%, Medium 100%, and Large 125%.
- [ ] Legacy Text Size values normalize correctly.
- [ ] Runs lists persisted Runs.
- [ ] Results loads the selected Run.
- [ ] Sequence Viewer loads a valid sequence and displays frames.
- [ ] Dataset Label loads persisted crops and labels.
- [ ] Model Test runs against the Active Model.
- [ ] Training and Model Library share the authoritative registry.

## Provisional items

These are wired and usable but are not yet qualified final scientific calibration:

- [ ] GUI clearly reports the route boundary as a **Provisional midpoint route boundary**.
- [ ] No production path falsely claims the midpoint is a user-calibrated boundary.
- [ ] The midpoint is derived from the actual camera/sequence dimensions, not fixed at 1024 × 1024.
- [ ] Fast detector defaults are treated as provisional.

Still required for final scientific qualification:

- [ ] Add the approved click-on-image Hit Boundary calibration interaction.
- [ ] Add Top-is-Hit / Bottom-is-Hit selection.
- [ ] Persist the selected calibration in Setup Profile and Run Summary.
- [ ] Validate detector settings against representative recorded sequences.
- [ ] Qualify device-specific timing and DAQ output behavior.

## Physical hardware-in-the-loop validation

Do not mark this section complete based only on builds, mocks, or controller tests.

### DCAM camera

- [ ] Test with the intended Hamamatsu camera physically attached.
- [ ] Confirm the correct device identifier.
- [ ] Start and stop streaming repeatedly.
- [ ] Confirm preview frame dimensions and bit depth.
- [ ] Confirm recovery after a controlled disconnect/reconnect.
- [ ] Run Live long enough to detect dropped frames or freezes.

### NI-DAQmx

- [ ] Test with the intended NI device physically attached.
- [ ] Confirm device and channel discovery.
- [ ] Confirm configured Vpp, frequency, duration, and delay at the physical output.
- [ ] Measure a test pulse with an oscilloscope or appropriate acquisition instrument.
- [ ] Confirm one physical pulse per qualified Hit.
- [ ] Confirm no pulse for Waste.
- [ ] Confirm no physical output when DAQ Output is OFF.
- [ ] Confirm Sequence Test pulse timing follows requested processing FPS.
- [ ] Confirm Live/Sequence locks prevent conflicting DAQ operations.
- [ ] Confirm Stop and application shutdown leave the output in a safe state.

## Automated validation record

Current integration evidence:

- [x] Full Debug application build passed.
- [x] Changed-QML `qmllint` passed with baseline warnings only.
- [x] `run_writer_v2_test` passed.
- [x] `live_sorting_service_test` passed.
- [x] `live_sorting_controller_test` passed.
- [x] `sequence_test_service_test` passed.
- [x] `sequence_test_controller_test` passed.
- [x] Focused backend set passed 5/5.
- [x] New production-controller QML wiring test passed.
- [ ] Full `ShellSingleImage` CTest passes without exceptions.

The Shell suite currently reports 34 passed and 2 failed. The same two failures reproduce on the exact pre-integration base and are not caused by the GUI–backbone integration:

- `test_realUnavailableCameraDoesNotShowStartupPrompt`
- `test_startupPromptAndNavigation`

Treat these as separate camera-fixture/focus test corrections; do not use them to hide a new integration regression.

## Sign-off

- [ ] Software wiring reviewed.
- [ ] Qt Creator build reviewed.
- [ ] Qt Design Studio visual review completed.
- [ ] Camera HIL completed.
- [ ] DAQ HIL completed.
- [ ] Provisional detector defaults qualified or replaced.
- [ ] User Hit Boundary calibration implemented and validated.
- [ ] Final validation date recorded: ____________________
- [ ] Validator: ________________________________________
- [ ] Hardware identifiers: ______________________________
- [ ] Notes: ____________________________________________
