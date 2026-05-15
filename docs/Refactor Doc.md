Below is a revised, more practical agent brief. It merges the broad worker plan with the more specific `code_review.md` findings: `main.cpp` is the critical problem, runtime components are already fairly clean, formatting is useful but not the main issue, and the work should be done in **sequential phases**, not 15 parallel workers. The review notes that `main.cpp` is ~9,796 lines and contains ~53â€“56% of the source code, while existing runtime components like `PipelineRunner`, `OnnxClassifier`, `FastEventDetector`, `DaqTrigger`, `CameraWorker`, and `DatasetCaptureSession` are relatively well-structured.  The previous plan correctly emphasized preserving runtime behavior, not redesigning the GUI, and splitting work into reviewable tasks.

````markdown
# OpenDSS Refactor Agent Brief â€” Revised Execution Plan

## Objective

Refactor the `OpenDSS_clean` desktop runtime codebase to reduce `desktop_app/main.cpp`, improve maintainability, and prepare the project for testing and future development.

This is **not** a feature task. This is a structural cleanup task.

The current application is a Qt6/C++17 desktop app for real-time droplet sorting:

```text
Hamamatsu DCAM camera
â†’ frame acquisition
â†’ event/droplet detection
â†’ crop extraction
â†’ ONNX model inference
â†’ hit/waste classification
â†’ NI-DAQ trigger output
â†’ GUI visualization/logging/statistics
````

The critical issue is that `desktop_app/main.cpp` is still effectively the application core. It contains embedded widget classes, data structures, utility functions, application state, GUI construction, signal/slot wiring, frame-processing hooks, live logging, sequence replay, dataset capture hooks, and GUI self-test code.

A previous refactor already extracted some layout code into files like:

```text
workspace_camera.*
workspace_model.*
workspace_dataset.*
workspace_reports.*
workspace_settings.*
workspace_validator.*
```

That was a useful first pass, but those files are mostly layout factories. The controller logic still largely lives in `main.cpp`.

## High-Level Diagnosis

### Most important problem

`desktop_app/main.cpp` is a god file and likely contains a huge `main()` function. It must be broken apart carefully.

### What is already relatively good

Do **not** treat the whole repo as broken. The runtime layer is comparatively clean and should not be rewritten casually.

Preserve and reuse these components where possible:

```text
pipeline_runner.*
onnx_classifier.*
fast_event_detector.*
event_detector.*
daq_trigger.*
metadata_loader.*
camera_worker.*
dcam_controller.*
dataset_capture_session.*
theme.*
```

These should mostly need testability/build integration, not wholesale redesign.

### Important correction from earlier plan

Do **not** run 15 workers in parallel. That creates merge risk because most changes touch `main.cpp`.

Use **sequential phases**. Each phase must build before moving on.

---

# Hard Rules

1. Preserve current runtime behavior unless explicitly stated.
2. Do not redesign the GUI visually.
3. Do not replace Qt Widgets with QML.
4. Do not remove DCAM, ONNX Runtime, NI-DAQmx, mock-camera, no-DAQ, or test-mode behavior.
5. Do not rewrite the application from scratch.
6. Do not mix formatting-only changes with logic movement.
7. Do not perform broad â€œcleanupâ€ outside the current phase.
8. Each phase must end with a build attempt and a written summary.
9. Stop after each phase. Do not proceed to the next phase unless explicitly instructed.
10. Keep changes mechanical when the phase says mechanical extraction.

---

# Required Report After Every Phase

At the end of every phase, report:

```markdown
## Phase Summary

### Files changed

### What was moved or changed

### Behavior intentionally changed

### Behavior expected to remain unchanged

### Build command used

### Build result

### Known issues / follow-up
```

If the build fails, stop and report the exact failure. Do not continue refactoring on top of a broken build.

---

# Phase 0 â€” Baseline Audit

## Goal

Establish a safe baseline before changing anything.

## Tasks

1. Create a branch:

```bash
git checkout -b refactor/maincpp-decomposition
```

2. Build the current project exactly as-is.

3. Record the build command.

4. Record the executable name and launch command.

5. Record current warnings/errors.

6. Run the app if possible.

7. Record whether the following modes exist and how they are invoked:

   * normal hardware mode
   * `--mock-camera`
   * `--no-daq`
   * `--test-mode`
   * `--verify-camera-workspace`
   * `--verify-daq-settings`

8. Create:

```text
docs/refactor-baseline.md
```

## `docs/refactor-baseline.md` should contain

```markdown
# Refactor Baseline

## Commit hash

## Build environment

## Build command

## Build result

## Runtime launch command

## Existing runtime flags

## Existing self-test flags

## Known warnings

## Known broken behavior

## Notes
```

## Acceptance Criteria

* Baseline document exists.
* Existing build status is known.
* No source behavior has been changed.

STOP after this phase.

---

# Phase 1 â€” Mechanical Extraction from `main.cpp`

## Goal

Reduce `main.cpp` by extracting self-contained classes, structs, and utility functions without changing behavior.

This phase should be mechanical. Do not redesign architecture yet.

## 1.1 Extract embedded Qt widget classes

If these classes exist inside `main.cpp`, move them into separate files:

```text
desktop_app/zoom_image_view.h
desktop_app/zoom_image_view.cpp

desktop_app/viewer_window.h
desktop_app/viewer_window.cpp

desktop_app/dataset_labeler_dialog.h
desktop_app/dataset_labeler_dialog.cpp

desktop_app/image_validation_dialog.h
desktop_app/image_validation_dialog.cpp

desktop_app/stats_figure_window.h
desktop_app/stats_figure_window.cpp
```

Expected examples of classes to extract:

```text
ZoomImageView
ViewerWindow
DatasetLabelerDialog
ImageValidationDialog
StatsFigureWindow
```

Rules:

* Do not change class behavior.
* Do not rename public methods unless necessary.
* Preserve signal/slot behavior.
* Preserve widget names.
* Preserve styling and layout.
* Update CMake.

## 1.2 Extract shared data types

Move standalone structs/classes from the anonymous namespace in `main.cpp` into a header such as:

```text
desktop_app/app_types.h
```

Likely candidates:

```text
AppOptions
SequenceFrame
StatsTracker
StatsSnapshot
SequenceEventRecord
SequenceEventTracker
LiveLogRecord
```

Rules:

* Do not change fields unless required for compilation.
* Keep names stable.
* Keep semantics stable.

## 1.3 Extract utility functions

Move standalone utility functions into:

```text
desktop_app/app_utils.h
desktop_app/app_utils.cpp
```

Likely candidates:

```text
findProjectRootFromApp
runtimeModelArtifactPath
modelRegistryPath
loadModelRegistry
temporaryStaticModelRegistry
ensureDefaultWorkspaceAssets
registryEntrySummary
decideEventDirection
formatTimeSeconds
renderPieChart
```

If a utility function clearly belongs elsewhere, choose a precise file name, but avoid overengineering.

## 1.4 Extract crash/log infrastructure

Move crash/logging infrastructure into:

```text
desktop_app/crash_handler.h
desktop_app/crash_handler.cpp
```

Likely candidates:

```text
logMessage
logMessageNoPrune
qtLogHandler
termHandler
unhandledExceptionFilter
installLogTees
pruneLogs
```

## Deliverables

New files as needed:

```text
zoom_image_view.*
viewer_window.*
dataset_labeler_dialog.*
image_validation_dialog.*
stats_figure_window.*
app_types.h
app_utils.*
crash_handler.*
```

Updated:

```text
main.cpp
desktop_app/CMakeLists.txt
```

## Acceptance Criteria

* `main.cpp` is substantially smaller.
* Extracted files compile.
* No behavior intentionally changed.
* Existing app launches as before.
* Existing self-test flags still run if they previously ran.

STOP after this phase.

---

# Phase 2 â€” App Bootstrap and Main Window Extraction

## Goal

Move top-level application setup and GUI ownership out of `main.cpp`.

After Phase 1, `main.cpp` should still contain too much application setup. This phase introduces clearer ownership.

## 2.1 Extract app option parsing

Create:

```text
desktop_app/app_options.h
desktop_app/app_options.cpp
```

Move command-line parsing into a function like:

```cpp
AppOptions parseAppOptions(const QStringList& arguments);
```

Preserve all existing flags, including any hardware-free or verification flags.

## 2.2 Extract path resolution

Create:

```text
desktop_app/app_paths.h
desktop_app/app_paths.cpp
```

Move project/runtime/model/workspace/log path logic into this module.

Example structure:

```cpp
struct AppPaths {
    QString applicationDir;
    QString projectRoot;
    QString runtimeDir;
    QString modelsDir;
    QString workspaceDir;
    QString logsDir;
};
```

## 2.3 Create `MainWindow`

Create:

```text
desktop_app/main_window.h
desktop_app/main_window.cpp
```

Move top-level GUI ownership into `MainWindow`.

`MainWindow` should own:

```text
menu bar
major panels
workspace stack
status strip
top-level actions
workspace registration
```

`main.cpp` should not directly construct hundreds of widgets.

## 2.4 Introduce `AppContext`

Create:

```text
desktop_app/app_context.h
desktop_app/app_context.cpp
```

Use this to replace the current pattern of 300+ local variables in `main()` captured by lambdas.

Initial version may be simple:

```cpp
struct AppContext {
    AppOptions options;
    AppPaths paths;

    // Shared runtime state
    // Shared model registry state
    // Shared DAQ/camera/settings state
};
```

Do not overdesign. The first goal is to make ownership visible.

## 2.5 Keep `main.cpp` small

Target shape:

```cpp
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    AppOptions options = parseAppOptions(app.arguments());
    AppPaths paths = resolveAppPaths(options);
    AppContext context(options, paths);

    MainWindow window(context);
    window.show();

    return app.exec();
}
```

This exact code is not required, but the responsibility level is the goal.

## Deliverables

```text
app_options.*
app_paths.*
app_context.*
main_window.*
```

Updated:

```text
main.cpp
desktop_app/CMakeLists.txt
```

## Acceptance Criteria

* `main.cpp` mainly starts the app and launches `MainWindow`.
* Top-level GUI ownership is inside `MainWindow`.
* Existing app behavior is preserved.
* Existing runtime flags are preserved.
* Existing self-test flags are preserved or moved cleanly.

STOP after this phase.

---

# Phase 3 â€” Workspace Controller Refactor

## Goal

Move controller logic out of `main.cpp` / `MainWindow` and into workspace-specific controller modules.

The existing `workspace_*.cpp` files are mostly layout factories. That is acceptable temporarily, but the behavior/controller logic should not all live in `main.cpp`.

## Important

Do not rewrite all workspaces at once. Do one workspace at a time.

Recommended order:

```text
1. Camera / Live workspace
2. Model workspace
3. Dataset workspace
4. Reports workspace
5. Settings / DAQ workspace
6. Validator / sequence workspace
```

## 3.1 Create controller classes

For each workspace, create a controller class where useful:

```text
camera_workspace_controller.h/.cpp
model_workspace_controller.h/.cpp
dataset_workspace_controller.h/.cpp
reports_workspace_controller.h/.cpp
settings_workspace_controller.h/.cpp
validator_workspace_controller.h/.cpp
```

The controller should own behavior. The layout file may continue building widgets at first.

Example:

```cpp
class CameraWorkspaceController : public QObject {
    Q_OBJECT

public:
    explicit CameraWorkspaceController(AppContext& context, QObject* parent = nullptr);

    QWidget* widget() const;

signals:
    void startCameraRequested();
    void stopCameraRequested();

private:
    AppContext& context_;
    QWidget* root_ = nullptr;
};
```

## 3.2 Reduce lambda capture complexity

Move long `QObject::connect(...)` lambda blocks out of `main.cpp`/`MainWindow`.

Goal:

* Avoid lambdas capturing 20+ variables.
* Avoid camera-thread hooks directly mutating GUI-owned state.
* Use small methods on controllers.
* Use signals/slots for cross-thread events.

## 3.3 Preserve widget names

Existing self-test code may rely on `objectName()` values.

Do not remove `nameWidget(...)` calls unless the tests are updated accordingly.

## 3.4 Do not over-prioritize QWidget subclass conversion

It is acceptable for the first controller refactor to leave layout factory functions in place.

The immediate goal is not:

```text
Every workspace must be a perfect QWidget subclass.
```

The immediate goal is:

```text
Controller logic should not live in main.cpp.
State ownership should be visible.
Signal/slot wiring should be localized.
```

## Deliverables

Controller files as needed.

Updated:

```text
main_window.*
workspace_*.*
desktop_app/CMakeLists.txt
```

## Acceptance Criteria

* At least one major workspace controller is extracted per commit.
* Long lambdas are reduced.
* Workspace behavior is easier to locate.
* Existing GUI still behaves the same.
* Existing object names are preserved.
* Build passes after each workspace extraction.

STOP after each workspace controller extraction unless explicitly instructed to continue.

---

# Phase 4 â€” Model Registry and Logging Cleanup

## Goal

Move high-risk but self-contained support logic out of GUI/bootstrap code.

## 4.1 Model registry service

If hardcoded model registry fallback JSON exists in C++ source, move registry handling into:

```text
desktop_app/model_registry_service.h
desktop_app/model_registry_service.cpp
```

or, if more appropriate:

```text
app/runtime/model_registry_service.h
app/runtime/model_registry_service.cpp
```

Responsibilities:

```text
load model registry JSON
validate required fields
resolve model paths
expose available models
expose default/selected model
report missing model files
```

Prefer moving fallback registry data to a JSON resource/file rather than embedding a long JSON string in `main.cpp`.

Create:

```text
docs/model-registry-schema.md
```

## 4.2 Runtime logging service

Move live log / CSV / sequence event output logic into:

```text
desktop_app/live_log_writer.h
desktop_app/live_log_writer.cpp
desktop_app/sequence_summary_writer.h
desktop_app/sequence_summary_writer.cpp
```

or runtime-level equivalents if independent of GUI.

Responsibilities:

```text
write event rows
write sequence summaries
define CSV headers
flush logs
handle file paths
```

Create:

```text
docs/runtime-logs.md
```

## Deliverables

```text
model_registry_service.*
live_log_writer.*
sequence_summary_writer.*
docs/model-registry-schema.md
docs/runtime-logs.md
```

## Acceptance Criteria

* No long hardcoded registry JSON inside `main.cpp`.
* CSV writing is not inside `main.cpp`.
* Sequence event summary writing is isolated.
* GUI can display/report logs without owning CSV formatting.
* Existing model selection behavior is preserved.
* Existing logging output format is preserved unless documented.

STOP after this phase.

---

# Phase 5 â€” Build Target and Testability Setup

## Goal

Prepare the cleaner codebase for unit tests without rewriting runtime components.

The runtime components are already relatively well-designed. Do not rewrite them unnecessarily.

## 5.1 Create runtime library target

Create a CMake target such as:

```text
opendss_runtime
```

Include non-GUI runtime files such as:

```text
pipeline_runner.*
onnx_classifier.*
fast_event_detector.*
event_detector.*
daq_trigger.*
metadata_loader.*
camera_worker.*
dcam_controller.*
dataset_capture_session.*
```

If some files still depend on Qt Core, that is acceptable. Avoid Qt Widgets in the runtime library if possible.

GUI executable should link against this library.

## 5.2 Add basic tests

Use one framework:

```text
Catch2
GoogleTest
Qt Test
```

Recommended initial tests:

```text
test_metadata_loader.cpp
test_sequence_event_tracker.cpp
test_fast_event_detector_synthetic.cpp
test_daq_trigger_config.cpp
```

Do not require physical hardware.

## 5.3 Extract GUI self-tests if practical

Existing self-test flags are useful. Move their implementation out of `main.cpp` into:

```text
desktop_app/gui_self_tests.h
desktop_app/gui_self_tests.cpp
```

Keep the command-line flags working.

## Deliverables

```text
updated CMakeLists.txt
tests/*
desktop_app/gui_self_tests.*
```

## Acceptance Criteria

* GUI executable still builds.
* Runtime library builds.
* At least one no-hardware test builds and runs.
* Existing GUI self-test flags still work or are cleanly relocated.

STOP after this phase.

---

# Phase 6 â€” Formatting, CI, and Documentation Polish

## Goal

Lock in maintainability after the structural cleanup.

This phase comes after mechanical extraction, not before. Formatting a 9,796-line god file is less useful than first reducing it.

## 6.1 Add `.clang-format`

Create:

```text
.clang-format
```

Suggested baseline:

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
BreakBeforeBraces: Attach
PointerAlignment: Left
SortIncludes: false
```

Run formatting in a dedicated commit.

## 6.2 Add build documentation

Create or update:

```text
docs/build.md
docs/dependencies.md
docs/desktop-architecture.md
docs/runtime.md
```

Keep personal/local-machine notes separate:

```text
docs/build-local-haemin.md
```

## 6.3 Add CI if feasible

Create:

```text
.github/workflows/windows-build.yml
```

Minimum goal:

```text
configure
build no-hardware target
run no-hardware tests
```

Do not require DCAM or NI hardware in CI.

## Deliverables

```text
.clang-format
docs/build.md
docs/dependencies.md
docs/desktop-architecture.md
docs/runtime.md
docs/build-local-haemin.md
.github/workflows/windows-build.yml
```

## Acceptance Criteria

* Formatting is applied in its own commit.
* Docs match the refactored structure.
* CI does not require physical hardware.
* Build still passes locally.

STOP after this phase.

---

# Work Order Summary

Execute in this order only:

```text
Phase 0 â€” Baseline audit
Phase 1 â€” Mechanical extraction from main.cpp
Phase 2 â€” App bootstrap and MainWindow extraction
Phase 3 â€” Workspace controller refactor
Phase 4 â€” Model registry and logging cleanup
Phase 5 â€” Runtime library target and tests
Phase 6 â€” Formatting, CI, and docs
```

Do not run these phases in parallel unless explicitly instructed.

---

# Highest Priority Now

Start with **Phase 0 and Phase 1 only**.

Do not start workspace controller refactoring, CMake library splitting, interface redesign, or CI until the mechanical extraction from `main.cpp` is complete and the app still builds.

The first target is to reduce `main.cpp` by moving self-contained classes, structs, utility functions, and crash/logging helpers into separate files with minimal behavior change.

```
```
