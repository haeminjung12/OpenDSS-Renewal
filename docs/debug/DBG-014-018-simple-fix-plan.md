# Plan

Complete the five user-authorized, non-security simple fixes as one coherent debug batch while preserving existing camera acquisition and every-frame detector delivery. The working tree currently contains unverified candidate edits and failing regression fixtures from the interrupted implementation attempt; the first action is to audit those edits against this plan before any further implementation or verification.

## Scope
- In: `DBG-014` bounded Model Test/Training process output, `DBG-015` Model Library virtualization, `DBG-016` legacy post-processing thread cleanup, `DBG-017` shared-viewer image failure presentation, and `DBG-018` dataset-label role-map caching.
- Out: security findings, installer signing, unsafe-checkpoint hardening, reparse-point investigation, global QML context refactoring, Runs-history model restructuring, and every camera/DCAM/acquisition/detector change.

## Action items
[x] Audit the current unverified diff and retain only the authorized files and behaviors for `DBG-014` through `DBG-018`; do not alter pre-existing accepted debug work.
[x] Finalize `DBG-014` with a 1 MiB maximum JSONL message, a 64 KiB rolling stderr tail, explicit oversized-protocol failure, bounded child termination, and focused Model Test/Training regressions.
[x] Finalize `DBG-015` by bounding `ModelLibraryWorkspace`'s dynamic `ListView`, enabling delegate reuse, and preserving selection, buttons, empty state, and keyboard/mouse behavior.
[x] Finalize `DBG-016` by connecting the legacy `QThread::finished` signal directly to the thread object's `deleteLater`, independently of `MainWindow`, while keeping UI result delivery guarded by the window lifetime.
[x] Finalize `DBG-017` by treating `Image.Error` as a shared-viewer placeholder/error state while preserving valid-image retention, zoom, pan, scrollbar policy, and preview acknowledgement.
[x] Finalize `DBG-018` by caching the exact immutable role-name map without changing role IDs, names, values, or the dataset-label model contract.
[x] Run focused Release checks for `model_test_service_test`, `training_service_protocol_test`, `dataset_label_controller_test`, the legacy `desktop_app` compile, and the affected `ShellSingleImage` QML tests; distinguish any unrelated pre-existing failure from this batch.
[x] Run one fresh read-only Plan Guardian, then one integrated Release build and full CTest gate without repeating unchanged checks.
[x] Update `docs/debug/bug-ledger.md`, `docs/agent-state/current.md`, and a durable evidence report with expected/observed behavior, before/after results, remaining risks, and per-fix rollback instructions.

## Open questions
- None. Camera behavior remains frozen because existing USB HIL already proves zero-drop detector delivery at 710.858 fps ROI and 63.309 fps full-frame.
