# Repository Agent Policy

Use this file as the repo-level `AGENTS.md` template for projects that should follow the token-saving and memory strategy.

## Priorities

1. Save tokens by using indexed, compressed, and targeted context.
2. Preserve useful project memory across sessions.
3. Keep raw file reads and broad shell output as fallback paths.

## Repo Workflow Order

For repo work, follow this order:

1. Read durable context first: `graphify-out/GRAPH_REPORT.md` and `graphify-out/wiki/index.md` when present.
2. Check semantic indexes next: run `grepai status` when `.grepai/` exists, then use `grepai search "<concept>" --json --compact` before broad grep/find when locations are unclear.
3. Use RTK-native commands for repo shell work: `rtk read`, `rtk grep`, `rtk find`, `rtk tree`, `rtk diff`, `rtk test`, and `rtk git ...`.
4. Use RTK-native commands for repo shell work. If there is no RTK-native equivalent, use plain PowerShell directly.

Do not use `rtk powershell`; use RTK-native commands for repo work and plain PowerShell directly when no RTK-native equivalent exists.

RTK requirements apply only to repo-scoped work. For operations on paths outside the repository root, use plain PowerShell or another appropriate non-RTK tool unless the task is specifically about testing RTK.

## Navigation Policy

- Check existing project context first: `graphify-out/GRAPH_REPORT.md`, `graphify-out/wiki/index.md`, `.grepai/`, and any repo docs.
- Use graph/index/search before reading files broadly.
- Read raw files only after narrowing to a symbol, module, or at most 3 likely files.
- Prefer snippets, call traces, graph paths, and compact JSON over whole-file dumps.
- Prefer RTK-native commands such as `rtk read`, `rtk grep`, `rtk ls`, `rtk tree`, `rtk find`, `rtk diff`, `rtk test`, and compact tool-specific wrappers.
- Before using RTK, check whether the primary target path is inside the repository root.
- Do not use `rtk powershell`.
- For repo-scoped work without an RTK-native equivalent, use plain PowerShell directly.
- Do not use `rtk` or `rtk powershell` for ad hoc inspection of external data directories, removable drives, network shares, or user folders outside the repo root unless the user explicitly asks to use or validate RTK.

## Repo Initialization

- When setting up this policy in a repo, initialize local token-saving indexes unless the repo is too large, unsupported, or the user asks not to:

```powershell
grepai init --provider ollama --backend gob --model nomic-embed-text --yes
grepai watch --background
graphify extract .
```

- If initialization is skipped, note why and leave the repo-local `AGENTS.md` in place.

## Graphify

- If no graph exists during repo initialization, or if the task requires architecture, cross-document context, onboarding, or broad repo understanding, run:

```powershell
graphify extract .
```

- If `graphify-out/GRAPH_REPORT.md` exists, read it before architecture or codebase questions.
- For relationship questions, prefer:

```powershell
graphify query "<question>"
graphify path "<A>" "<B>"
graphify explain "<concept>"
```

- After meaningful code changes, refresh the graph:

```powershell
graphify update .
```

## grepai

- On repo start, check whether `.grepai/` exists.
- If `.grepai/` exists, run:

```powershell
grepai status
```

- If the watcher is not running, run:

```powershell
grepai watch --background
```

- This default workflow uses local Ollama embeddings and does not require an embedding API key.
- If `.grepai/` is absent during repo initialization, or if semantic search will materially reduce exploration during ordinary work, initialize once:

```powershell
grepai init --provider ollama --backend gob --model nomic-embed-text --yes
grepai watch --background
```

- Prefer local Ollama embeddings with `nomic-embed-text` to avoid cloud API keys and code upload.
- If using a cloud provider instead, the relevant embedding API key must be set before indexing/search will work.

- For conceptual search:

```powershell
grepai search "<concept>" --json --compact
```

- For call relationships:

```powershell
grepai trace callers "<symbol>" --json --compact
grepai trace callees "<symbol>" --json --compact
```

## Shell Output

- Use RTK for shell workflows that inspect repo files, git state, search results, tests, build output, package-manager output, logs, or generated evidence. This is mandatory, not optional.

```powershell
rtk git status
rtk git diff
rtk rg "<pattern>"
rtk pytest -q
rtk npm test
```

- Prefer RTK-native commands for repo inspection and developer workflows:

```powershell
rtk read path\file.md
rtk grep "pattern" path
rtk ls path
rtk tree path
rtk find path -name "*.md"
rtk diff
rtk test
```

- Use `rtk init -g` when the global RTK hook is missing or needs to be refreshed. This is a user/environment setup step, not a per-repo routine.
- Do not use `rtk powershell`.
- If exact repo work requires a PowerShell cmdlet and no RTK-native command exists, run plain PowerShell directly instead.
- If exact file text is needed, first narrow with graphify, grepai, `rtk grep`, `rtk find`, a heading/symbol query, or another compact search/trace. Then read the smallest useful snippet with `rtk read` or another compact RTK-native command.
- Do not use raw `Get-Content`, `Get-ChildItem`, `Select-String`, `git`, `rg`, build, test, or package-manager commands for repo work unless testing RTK itself, working around an RTK-specific failure, using a non-shell MCP/tool, or editing with `apply_patch`.
- If the primary target path is outside the repository root, do not route the command through RTK unless the task is specifically about testing RTK behavior.
- Check savings when relevant:

```powershell
rtk gain
```

Examples:

```powershell
# Repo path
rtk read src\main.cpp
rtk grep "pattern" src
powershell -NoProfile -Command "Test-Path .\build\app.log"

# Non-repo path
Test-Path "D:\dataset\run1"
Get-ChildItem -LiteralPath "D:\dataset\run1" -File
Get-ChildItem "\\server\share\images" -Filter *.tif
```

## Headroom

- Use Headroom MCP tools for large logs, long pasted context, large JSON, reports, generated artifacts, or repeated retrieval.
- Prefer compress-cache-retrieve behavior over keeping large raw output in context.
- For local proxy sessions, verify:

```powershell
Invoke-RestMethod http://127.0.0.1:8787/stats
```

## Token Usage

- Use `tokscale` for token/context usage analytics:

```powershell
tokscale --client codex --today
tokscale monthly --client codex
tokscale tui --client codex
```

## Freshness

- After edits, refresh relevant indexes before continuing broad navigation.
- Keep this file short. Put detailed project knowledge in graph reports, grepai indexes, or dedicated docs rather than expanding this policy.

## OpenDSS Process Handling

- When a process blocks the selected OpenDSS build output, identify its PID, executable path, and command line. If those details show that it belongs to the exact intended build root, terminate it and continue the build even when the PID was not previously recorded.
- This applies to stale OpenDSS executables and build tools such as CMake, MSBuild, Ninja, `cl.exe`, and linker processes whose command line targets the selected OpenDSS build output.
- Never terminate an OpenDSS instance while training or testing is active. Never terminate unrelated user applications, camera/vendor tools, DAQ utilities, or build processes targeting another workspace.
- After terminating a blocking process, report its PID/path or command line and retry the original build. Do not create a one-off build output merely to avoid a verified lock.

## OpenDSS v2 source authority

Apply the following authority order repository-wide:

1. `docs/v2/source/OpenDSS_v2_Approved_Product_Model.md` controls approved product decisions D-001 through D-019, product structure, scope, terminology, configuration boundaries, and product-state ownership constraints.
2. `docs/v2/source/OpenDSS_v2_Information_Architecture_and_Screen_Inventory.md` and `docs/v2/source/OpenDSS_v2_Low_Fidelity_Interaction_and_Application_State_Specification.md` control the current shell, navigation, workspaces, interactions, application states, resource ownership, locks, and contextual recovery.
3. `docs/v2/source/OpenDSS_Detailed_Workflow_Specification.md` supplies detailed scientific, artifact, persistence, recovery, and acceptance requirements that do not conflict with higher-authority documents.
4. `docs/v2/OpenDSS_v2_Consolidated_Product_Design_Specification.md` is the current consolidated visual/component design baseline under review and does not override the approved product model or interaction baselines.
5. `docs/v2/source/OpenDSS_Product_Design_Specification_Draft_v0.1.md` is historical, non-normative design evidence only.
6. Existing repository code is implementation evidence and a source of reusable technical components; it is not authority for v2 UX, product structure, terminology, product policy, or exposed configuration.

D-001 through D-019 belong only to the Approved Product Model. Repository code must not be used to reintroduce superseded navigation, terminology, product states, scientific policy, or editable settings. Existing v1 behavior is not automatically a v2 requirement. The consolidated design specification remains **Consolidated Draft for Review** and must not be silently marked Approved.

## Protected reusable technical assets

The following technical areas are presumed reusable and must not be rewritten, replaced, deleted, or behaviorally changed without documented justification and regression evidence:

- Hamamatsu/DCAM integration;
- National Instruments/NI-DAQmx integration;
- camera acquisition mechanics;
- DAQ output mechanics;
- qualified droplet-detection behavior;
- ONNX Runtime integration;
- model preprocessing and inference mechanics;
- Python training implementation;
- model export mechanics;
- proven background file-writing and atomic-persistence mechanics.

Representative protected paths include, but are not limited to:

- `app/runtime/dcam_camera.*`
- `app/runtime/daq_trigger.*`
- `app/runtime/event_detector.*`
- `app/runtime/fast_event_detector.*`
- `app/runtime/onnx_classifier.*`
- `app/runtime/metadata_loader.*`
- `training/python/droplet_trainer/**`
- `app/runtime/desktop_app/json_persistence.*`
- `app/runtime/desktop_app/live_data_collection_writer.*`
- `app/runtime/desktop_app/live_log_writer.*`
- `app/runtime/desktop_app/sequence_summary_writer.*`

Protected does not mean exempt from review. It means no replacement or behavioral modification without evidence.

## Change-control rule for protected assets

Before consolidating, deleting, replacing, or materially changing a protected module, a future implementation task must provide:

1. current consumers and build targets;
2. characterization or regression tests;
3. representative fixtures;
4. behavior comparison before and after;
5. performance comparison where timing matters;
6. hardware-in-the-loop evidence where hardware behavior is affected;
7. documented justification;
8. rollback strategy.

## Application-layer boundary

- QML or UI code must not call vendor SDKs or the trainer directly.
- Old product policy must be separated from reusable technical mechanics.
- User-facing detector, crop, routing, internal timing, and training hyperparameter controls must not be restored from old code.
- One authoritative owner must exist for each domain state.
- Duplicated widget-local state must not become the v2 architecture.
