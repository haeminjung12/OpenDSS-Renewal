# DBG-014 through DBG-018 simple-fix gate

Date: 2026-07-31

## Scope

This batch implements only the user-authorized non-security fixes:

- `DBG-014`: bound Model Test and Training child-process output;
- `DBG-015`: restore Model Library `ListView` virtualization;
- `DBG-016`: make legacy post-processing thread cleanup independent of `MainWindow`;
- `DBG-017`: show the shared viewer placeholder when an image fails to load;
- `DBG-018`: cache the immutable dataset-label role map.

Camera, DCAM, acquisition, detector, crop, ONNX, persistence, installer, Runs-history, and global QML-context behavior were explicitly excluded. The batch does not modify camera or detector files. Existing physical USB HIL evidence remains the authority for every-frame delivery.

## Reproduction evidence

- `DBG-014`: new oversized-output fixtures failed before the production fix. Training reported `Oversized trainer output was not bounded`; Model Test failed to publish the required maximum-message rejection.
- `DBG-015`: a 100-row Model Library fixture showed that the list expanded to its full content and materialized the last delegate instead of remaining viewport-bounded.
- `DBG-016`: inspection showed that the unparented worker thread's `deleteLater` connection used `MainWindow` as its lifetime context, so window destruction could disconnect cleanup before thread completion.
- `DBG-017`: a nonempty invalid image URL suppressed the empty placeholder even after `Image.Error`.
- `DBG-018`: the fixed four-entry role map was constructed on each `roleNames()` call.

Observations were kept separate from the accepted conclusions below; no camera-performance hypothesis was converted into a production change.

## Accepted root causes and fixes

- `DBG-014`: the process protocols had no byte ceiling. Model Test and Training now reject JSONL messages above 1 MiB; Training retains only the newest 64 KiB of stderr and terminates a child that violates the protocol.
- `DBG-015`: binding `ListView.height` to `contentHeight` removed the bounded viewport needed for delegate virtualization. The list is now bounded by available viewport height and uses delegate reuse.
- `DBG-016`: cleanup and UI delivery shared the window lifetime. `QThread::finished -> QThread::deleteLater` is now owner-independent while UI result delivery remains guarded by `MainWindow`.
- `DBG-017`: visibility depended only on source presence. `Image.Error` now hides the failed image and exposes the existing placeholder.
- `DBG-018`: constant role metadata was rebuilt on demand. The byte-identical mapping is now immutable static data.

## Focused verification

- Release build passed for `model_test_service_test`, `training_service_protocol_test`, `dataset_label_controller_test`, `desktop_app`, and `tst_ShellSingleImage`.
- Focused CTest passed 3/3 for Model Test service, Training protocol, and dataset-label model behavior.
- The Shell QML suite passed with 55 passed, 0 failed. It verifies the bounded Model Library and failed-image placeholder behavior.
- A fresh read-only Plan Guardian returned `PASS` after checking canonical state, the saved plan, the actual diff, protected boundaries, and focused evidence.

## Integrated verification

- `C:\b\odss-debug-lead-hil` Release build: passed.
- `C:\b\odss-debug-lead-hil` full CTest: 53/53 passed in 150.67 seconds.
- `C:\b\d13` actual-v2 Release build: passed.
- `C:\b\d13` full CTest: 2/2 passed in 4.10 seconds.

The first deliberately parallel gate exposed resource contention:

- `model_test_controller_test` reached its existing 30-second CTest limit while a full v2 MSBuild ran concurrently. It passed directly in 27.819 seconds, passed isolated CTest in 27.43 seconds, and passed the final non-overlapped full suite in 28.31 seconds. No timeout or production change was made.
- The concurrent v2 build hit a tracking-file lock inside `C:\b\d13`. No build-root-owned process remained afterward; the isolated rebuild passed.
- One Shell run intermittently failed the unrelated `test_labelControllerDirectWiring` selection assertion. The required isolated rerun passed, and the following complete v2 CTest gate passed 2/2. No label-selection behavior was changed.

## Remaining risk

- The 1 MiB JSONL and 64 KiB stderr limits are intentionally generous but could reject a future, undocumented child-process protocol message larger than 1 MiB. Such a protocol expansion must change the contract and tests deliberately.
- Model Library virtualization is covered with 100 rows in an offscreen QML test; unusually large real libraries still require normal GUI usability observation.
- The legacy thread cleanup compiles and follows Qt's documented finished/deferred-delete lifecycle, but teardown during a real long-running legacy post-processing job was not manually exercised.
- Invalid-source presentation is verified; decoder-specific failures on unusual image formats may produce different Qt status timing but use the same `Image.Error` path.
- Camera acquisition and detector throughput are unchanged. This batch neither adds nor removes evidence for CoaXPress HIL.

## Rollback

- `DBG-014`: revert only the size constants, bounded stderr retention, oversized-message termination path, and their two regression fixtures.
- `DBG-015`: revert the `ModelLibraryWorkspace.ui.qml` viewport-height/reuse bindings and its bounded-list test.
- `DBG-016`: remove the independent `finished -> deleteLater` connection and restore the prior context-bound cleanup connection.
- `DBG-017`: restore source-only image/placeholder visibility and remove the invalid-source viewer assertion.
- `DBG-018`: restore inline role-map construction.

Rollback is file-local for each fix. Do not roll back or alter the protected camera, detector, ONNX, trainer, export, or persistence mechanics.
