# OpenDSS Agent Policy

@C:\Users\goals\.codex\RTK.md

This repo has two working modes:

- Full repo: `C:\Users\goals\Codex\OpenDSS\0. Codebase`
- Sparse debug workspace: `C:\Users\goals\Codex\OpenDSS\2. Agent Debug Workspace`

For ordinary bug fixes, prefer the sparse debug workspace. It contains the active runtime source plus only the docs needed to debug, build, and verify.

## Token-Efficient Debug Rules

1. Start with `docs/active-bug-queue.md`, `docs/current-state.md`, and `docs/build.md`.
2. If the bug location is unclear, use `grepai status` and `grepai search "<concept>" --json --compact` before broad grep.
3. Use RTK-native repo commands: `rtk read`, `rtk grep`, `rtk find`, `rtk diff`, `rtk test`, and `rtk git ...`.
4. Read at most 3 likely source files before forming a bug hypothesis.
5. Prefer line-targeted reads over whole-file reads, especially for `app/runtime/desktop_app/main_window.cpp`.
6. Fix one bug per branch or commit.
7. Keep packaging, trainer/exporter, old wave history, and archived workspaces out of ordinary runtime bug fixes.
8. Do not fire DAQ output unless the user explicitly approves the exact action.

## Safe Verification

Default safe checks:

```powershell
rtk test cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target desktop_app
rtk test cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target opendss_runtime_metadata_loader_test
rtk test ctest --test-dir "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" -C Release --output-on-failure
```

For GUI-adjacent fixes, also run the relevant no-hardware verifier:

```powershell
rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --verify-camera-workspace
rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --mock-camera --no-startup-prompts --verify-daq-settings
```

## Workflow

Follow `docs/agent-debugging-workflow.md` for the step-by-step process.
