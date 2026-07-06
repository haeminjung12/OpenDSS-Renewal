# Agent Debugging Workflow

This workflow keeps bug fixes narrow and token-efficient.

## 1. Use The Sparse Workspace

Open work from:

```text
C:\Users\goals\Codex\OpenDSS\2. Agent Debug Workspace
```

It is a sparse worktree on branch `agent-debug-sparse`. It shows only:

- `AGENTS.md`
- `README.md`
- `app/runtime/**`
- selected docs needed for runtime debugging

## 2. Sync Before Work

From the sparse workspace:

```powershell
rtk git fetch origin
rtk git status --short --branch
rtk git merge --ff-only origin/main
```

If `--ff-only` fails, stop and inspect the branch relationship before editing.

## 3. Pick One Bug

Read:

```powershell
rtk read docs/active-bug-queue.md
rtk read docs/current-state.md
rtk read docs/build.md
```

Write down the symptom, expected behavior, and the smallest verification command before editing.

## 4. Locate The Code Narrowly

If you know the symbol or exact UI text:

```powershell
rtk grep -n "<symbol-or-text>" app/runtime docs
```

If you only know the concept:

```powershell
grepai status
grepai search "<concept>" --json --compact
```

Then read only the likely files or line ranges:

```powershell
rtk read app/runtime/desktop_app/main_window.cpp --lines 4400:4560
```

Avoid whole-file reads of `main_window.cpp` unless there is no narrower option.

## 5. Edit One Bug

Keep edits inside the smallest owning module. Prefer:

- workspace controllers for UI workflow behavior;
- writer modules for CSV/log format behavior;
- `app_types.h` only for shared runtime structs or helpers;
- `main_window.cpp` only when the behavior is still owned there.

Do not refactor unrelated code while fixing a bug.

## 6. Verify

Always run:

```powershell
rtk test cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target desktop_app
rtk test cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target opendss_runtime_metadata_loader_test
rtk test ctest --test-dir "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" -C Release --output-on-failure
```

For GUI changes, run the relevant verifier:

```powershell
rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --verify-camera-workspace
rtk test "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --mock-camera --no-startup-prompts --verify-daq-settings
```

Do not run DAQ output tests unless the user approves the exact action.

## 7. Commit And Push

For a completed bug:

```powershell
rtk git status --short
rtk git diff --stat
rtk git add -A
rtk git commit -m "Fix <specific bug>"
rtk git push origin HEAD
```

Use one commit per bug unless the user asks for a combined stabilization commit.
