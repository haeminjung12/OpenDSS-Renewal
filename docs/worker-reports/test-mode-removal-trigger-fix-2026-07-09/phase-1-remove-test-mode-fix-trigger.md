# Worker Report: Phase 1 Remove Test Mode And Fix Live Trigger

Agent: Qt/C++ implementation worker
Date: 2026-07-09
Assigned Scope: test/mock/no-DAQ removal, Live View manual trigger direct DAQ path, active verifier docs

## Summary

Removed the production app fake/no-hardware modes and kept the useful Socrates direction by preserving the new Live View manual-trigger verifier while replacing the hidden Settings-button dependency with a direct DAQ trigger lambda.

The visible Live View `Manual Trigger` now invokes finite `DaqTrigger` output directly through shared manual-trigger code. The Settings-side `DaqManualTriggerButton` is hidden from users and no longer acts as the Live View dependency.

No DAQ output was fired during this Phase 1 implementation/verification run.

## Files Changed

- `AGENTS.md` - replaced stale no-hardware verifier commands with hardware-required verifier commands.
- `app/runtime/desktop_app/app_context.cpp` - removed `hardwareFreeMode()`.
- `app/runtime/desktop_app/app_context.h` - removed `hardwareFreeMode()` declaration.
- `app/runtime/desktop_app/app_options.cpp` - removed parsing for `--test-mode`, `--mock-camera`, and `--no-daq`; kept hardware-required verifier flags.
- `app/runtime/desktop_app/app_state.h` - removed app-level `testMode` state.
- `app/runtime/desktop_app/app_types.h` - removed fake/no-DAQ option fields.
- `app/runtime/desktop_app/camera_workspace_controller.cpp` - removed mock/hardware-free camera behavior and retained only explicit startup-skip status for `--no-startup-prompts`.
- `app/runtime/desktop_app/camera_workspace_controller.h` - removed hardware-free dependency field/helper.
- `app/runtime/desktop_app/main.cpp` - removed persisted `settings/testMode` read and no-DAQ startup state.
- `app/runtime/desktop_app/main_window.cpp` - hid Settings manual trigger, routed Live View manual trigger directly to `DaqTrigger`, removed no-DAQ branches, updated hardware-required verifiers.
- `app/runtime/desktop_app/settings_workspace_controller.cpp` - removed no-DAQ discovery/probe/control bypasses.
- `app/runtime/desktop_app/settings_workspace_controller.h` - removed no-DAQ dependency field.
- `app/runtime/desktop_app/workspace_reports.cpp` - removed mock/no-hardware/no-DAQ diagnostics labels.
- `app/runtime/desktop_app/workspace_reports.h` - removed fake/no-DAQ controls.
- `app/runtime/desktop_app/workspace_settings.cpp` - removed persisted Settings test-mode checkbox and writes.
- `app/runtime/desktop_app/workspace_settings.h` - removed test-mode/hardware-free control plumbing.
- `docs/agent-debugging-workflow.md` - replaced stale no-hardware verifier commands with hardware-required verifier commands.
- `docs/build.md` - added hardware-required GUI verifier commands, including Live View manual trigger verifier.
- `docs/worker-reports/test-mode-removal-trigger-fix-2026-07-09/phase-1-remove-test-mode-fix-trigger.md` - wrote this completion report.

## Files Read

- `AGENTS.md` - repo policy and verifier commands.
- `WORKSPACE_ORCHESTRATION_RULE_V2.md` - worker/orchestrator constraints.
- `docs/agent-operating-rules.md` - report format and source boundaries.
- `docs/current-state.md` - accepted runtime baseline.
- `docs/migration-manifest.md` - active source boundary.
- `docs/build.md` - build and verifier commands.
- `docs/agent-results/test-mode-removal-trigger-fix-2026-07-09/test-mode-removal-trigger-fix-2026-07-09-results.md` - current wave tracker and Socrates dirty patch note.
- `docs/worker-reports/test-mode-removal-trigger-fix-2026-07-09/phase-0-current-diff-map.md` - Phase 0 map of fake modes and manual-trigger path.
- Assigned desktop/DAQ source files under `app/runtime/desktop_app/` and `app/runtime/daq_trigger.*` as needed through targeted `rtk read`, `rtk grep`, and narrow snippets.

## Decisions Needed

- None for Phase 1 implementation.

## Dependencies and Blockers

- `settings_workspace_controller.*` was outside the written Phase 1 source write-scope list but was required to remove `--no-daq` completely and keep the build compiling. I edited it deliberately and limited the change to deleting the no-DAQ bypasses.
- Live View manual-trigger output validation was not run in Phase 1 because it intentionally fires DAQ output. The task allows isolating that output validation to Phase 2.

## Acceptance Criteria Check

- [x] Removed `--test-mode` parsing and option state.
- [x] Removed `--mock-camera` parsing and option state.
- [x] Removed `--no-daq` parsing and option state.
- [x] Removed persisted `settings/testMode` reads/writes and Settings UI.
- [x] Removed app-level `testMode` state.
- [x] Removed mock/no-hardware camera behavior from the production path.
- [x] Replaced stale fake/no-hardware verifier commands in active docs with hardware-required verifier commands.
- [x] Hid Settings > Hardware manual trigger from users.
- [x] Made Live View `Manual Trigger` call direct finite `DaqTrigger` output instead of pipeline readiness or hidden Settings-button delegation.
- [x] Kept/updated `--verify-live-view-manual-trigger`; it asserts hardware state and clicks visible `LiveForceTriggerButton` once after switching to Live View.

## Validation Performed

- `rtk git diff --check` - passed.
- `rtk test cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target desktop_app` - passed.
- `rtk test cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target opendss_runtime_metadata_loader_test` - passed.
- `rtk test ctest --test-dir "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" -C Release --output-on-failure` - passed, 2/2 tests.
- `rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --verify-camera-workspace` - passed.
- `rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --verify-daq-settings` - passed.

## Risks or Follow-Up

- Phase 2 should run `--verify-live-view-manual-trigger` with approved DAQ output reporting: trigger source, channel, waveform/settings, count, and observed/logged result.
- Historical docs and prior worker reports still mention removed flags as history; active source and active verifier docs were cleaned.
