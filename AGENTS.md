# Repository Agent Policy

Use this file as the repo-level `AGENTS.md` template for projects that should follow the token-saving and memory strategy.

## Priorities

1. Save tokens by using indexed, compressed, and targeted context.
2. Preserve useful project memory across sessions.
3. Keep raw file reads and broad shell output as fallback paths.

## Repo Workflow Order

For repo work, follow this order:

1. Read durable context first: `graphify-out/GRAPH_REPORT.md` and `graphify-out/wiki/index.md` when present.
2. Check semantic indexes next: run `grepai status` when `.grepai/` exists, then use `grepai search "<concept>" --json --compact` before lexical search when concepts or locations are unclear. Use `grepai trace` for call relationships.
3. Use RTK-native commands for repo shell work: `rtk read`, `rtk rg`, `rtk find`, `rtk tree`, `rtk diff`, `rtk test`, and `rtk git ...`.
4. Use RTK-native commands for repo shell work. If there is no RTK-native equivalent, use plain PowerShell directly.

Do not use `rtk powershell`; use RTK-native commands for repo work and plain PowerShell directly when no RTK-native equivalent exists.

RTK requirements apply only to repo-scoped work. For operations on paths outside the repository root, use plain PowerShell or another appropriate non-RTK tool unless the task is specifically about testing RTK.

## Navigation Policy

- Check existing project context first: `graphify-out/GRAPH_REPORT.md`, `graphify-out/wiki/index.md`, `.grepai/`, and any repo docs.
- Use graph/index/search before reading files broadly.
- Read raw files only after narrowing to a symbol, module, or at most 3 likely files.
- Prefer snippets, call traces, graph paths, and compact JSON over whole-file dumps.
- Prefer RTK-native commands such as `rtk read`, `rtk rg`, `rtk ls`, `rtk tree`, `rtk find`, `rtk diff`, `rtk test`, and compact tool-specific wrappers.
- Before using RTK, check whether the primary target path is inside the repository root.
- Do not use `rtk powershell`.
- For repo-scoped work without an RTK-native equivalent, use plain PowerShell directly.
- Do not use `rtk` or `rtk powershell` for ad hoc inspection of external data directories, removable drives, network shares, or user folders outside the repo root unless the user explicitly asks to use or validate RTK.

On Windows, use `rtk rg` for lexical or exact-text search. It does not replace required grepai semantic steps: when a concept or location is unclear, use `grepai search` before lexical search, and use `grepai trace` for call relationships. Use `rtk rg` after narrowing or immediately for a known exact symbol, heading, or pattern.

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
rtk rg "pattern" path
rtk ls path
rtk tree path
rtk find path -name "*.md"
rtk diff
rtk test
```

- Use `rtk init -g` when the global RTK hook is missing or needs to be refreshed. This is a user/environment setup step, not a per-repo routine.
- Do not use `rtk powershell`.
- If exact repo work requires a PowerShell cmdlet and no RTK-native command exists, run plain PowerShell directly instead.
- If exact file text is needed and its location is unclear, first narrow with graphify, `grepai search`, `grepai trace`, `rtk find`, or another compact semantic/search tool. Then use `rtk rg` for the known symbol, heading, or pattern and read the smallest useful snippet with `rtk read` or another compact RTK-native command.
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
rtk rg "pattern" src
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

## RULE-TOOL-001 — validation tool priority

- Use Windows Computer Use only when strictly necessary to establish user-observable GUI behavior that cannot be proven reliably through a narrower interface.
- Prefer, in order: focused Qt unit/integration tests and `QTest`; direct Qt object, property, signal, state, or in-process verifier probes; application CLI/verifier interfaces and structured logs; then Computer Use for the remaining visual or interactive question.
- Do not use Computer Use merely to launch an app, read text or state available through Qt/CLI, repeat unchanged successful evidence, or diagnose behavior that a deterministic probe can isolate.
- Appropriate Computer Use scope includes layout, rendering, focus, pointer interaction, flicker, timing perception, and other user-visible behavior for which Qt/CLI evidence is insufficient. State the exact unresolved GUI question and intended action before use, follow existing approval and hardware rules, and perform only the minimum interaction needed.
- Preserve the maximized full-available-work-area GUI validation authority. Stop Computer Use as soon as decisive evidence is captured and return further diagnosis to Qt tooling, tests, probes, CLI interfaces, or logs.

## OpenDSS v2 source authority

`docs/v2/design/consolidated-design-draft.md` (`ODSS-DES-002`) is the single master product and design specification. Verify it with `docs/v2/design/verify-consolidated-design-lock.ps1` before product, design, implementation, review, or validation work.

- If code, forms, plans, `current-slice.md`, `CONTEXT.md`, derived documents, reviews, existing behavior, or proposed behavior deviate, follow the master specification.
- If the applicable master text is ambiguous, internally inconsistent, missing, or cannot be followed safely, stop the affected work and clarify with the user. Do not invent, average, infer, preserve a provisional fallback, or let another document decide.
- Other canonical documents, amendments, reviews, archives, and repository code are provenance or implementation evidence only. They may help locate context but cannot override the master.
- `current-slice.md` limits what work is authorized; it does not change the required behavior. A work order must cite the exact `ODSS-DES-002` section and heading it implements.
- Because the master is long, do not read it wholesale. Use grepai for conceptual narrowing, then `rtk rg` for exact headings/terms and read only the cited local section. Carry only a short decisive excerpt into a work order.
- Every implementation, design handoff, test, review finding, and validation result must cite the controlling `ODSS-DES-002` section. Absence of a citation is a stop condition.
- Only an explicit user-approved amendment may change the master. Update the master and `consolidated-design-lock.json` together; never silently edit or re-hash it.

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

## Qt Design Studio compatibility contract

Qt Design Studio is the authoritative visual editing environment for the OpenDSS v2 Qt Quick frontend. Qt Creator owns C++, runtime QML, CMake integration, builds, debugging, profiling, and tests. Codex owns repository-level orchestration, architecture enforcement, implementation, and verification.

For questions about Qt APIs, QML behavior, Qt Design Studio compatibility, CMake integration, or version-specific Qt features, use the official Qt Documentation MCP before model memory or general web search.

### Qt skill routing

Qt skills supplement this repository policy; they do not override canonical product authority, bounded work orders, protected assets, Qt Design Studio ownership, generated-file boundaries, or proportional validation. When a generic skill instruction conflicts with those constraints, follow this repository policy and report the conflict.

Load and apply the smallest applicable skill set:

| Task | Required skill routing |
|---|---|
| Create or materially redesign screens, layouts, navigation, or interaction composition | `qt-ui-design` first, then `qt-qml` when QML is examined or changed. Canonical OpenDSS design sources replace the skill's generic design heuristics where they differ. |
| Write, fix, refactor, debug, optimize, or examine QML, including `*.ui.qml` | `qt-qml`. Also use the official Qt Documentation MCP for Qt API, version, behavior, or Qt Design Studio compatibility questions. |
| Create or change durable Qt CMake integration | `qt-cmake-project`, plus the official Qt Documentation MCP for command signatures and version behavior. Limit writes to CMake files explicitly authorized by the work order; never edit generator-owned CMake output. |
| Author Qt Quick Test cases | `qt-qml` and `qt-qml-test`. Source edits proposed by the test skill, including adding `objectName`, require explicit authorization for those exact source files. |
| Build or run QML tests | `qt-qml-test-run`. Do not use its wiring mode unless the work order explicitly authorizes every CMake and test-harness write. Apply the slice's proportional-validation limits. |
| Review or audit QML | `qt-qml` and `qt-qml-review`. Treat `qt-qml-review` as read-only and keep its scope to the work order or requested diff. |
| Investigate QML or Qt Quick performance | `qt-qml-profiler`. Use it only for an explicit performance investigation, not routine validation. Load `qt-qml` if the resulting work includes QML edits. |
| Review Qt/C++ | `qt-cpp-review`. Treat it as read-only and preserve the protected-asset change-control requirements. |
| Produce developer reference documentation for QML | `qt-qml-docs`; use only when documentation is requested. |
| Produce developer reference documentation for Qt/C++ | `qt-cpp-docs`; use only when documentation is requested. |
| Extract a Figma design system into QML tokens | `qt-figma-token-extraction`, only for an explicit Figma-sourced workflow with an available Figma connector. Do not let the skill scaffold a replacement project or rewrite generator-owned CMake in this repository. |
| Generate QML controls from Figma components | `qt-figma-component-generation` after token extraction prerequisites exist, plus `qt-qml` for QML authoring. Require a bounded work order for every output file and preserve `*.ui.qml` restrictions. |

Do not load documentation, review, profiler, test, or Figma skills merely because the project uses Qt. Trigger them only for their named task. When multiple rows apply, use all listed skills in the stated order without duplicating their work.

### File ownership

- `*.ui.qml` files contain designer-editable visual composition, token references, visual states, transitions, and exported property aliases only.
- Ordinary `*.qml` wrapper files contain signal handlers, controller bindings, navigation commands, validation presentation, focus routing, and runtime presentation behavior.
- C++ application/controller code owns authoritative application state, hardware, filesystem access, persistence, threading, and domain behavior.
- Qt Design Studio mock implementations belong in the generated project's `MockDatas/` folder and must mirror only interfaces consumed by the current slice.
- QML must not call vendor SDKs, filesystem APIs, persistence internals, or the trainer directly.

### `.ui.qml` restrictions

Every modified `*.ui.qml` file must remain editable in Qt Design Studio's 2D view. Do not add JavaScript blocks, signal handlers, `Timer`, `Behavior`, `Binding`, `Canvas`, `ShaderEffect`, imperative backend calls, direct hardware dependencies, or unsupported root types. Use exported property aliases when wrapper QML needs access to visual controls.

### Generated project and CMake boundaries

- The initial v2 Qt Quick project shell must be generated by Qt Design Studio in this working tree; do not hand-create a substitute project layout.
- Preserve the Qt Design Studio-generated project structure and `.qmlproject` source.
- Do not hand-edit generated `CMakeLists.txt` files that the CMake Generator overwrites.
- Durable manual CMake changes are limited to the generated project root and `App` CMake files plus explicitly separate backend/test integration files.
- Do not add an independent QML module or `qmldir` inside the generated content folder when it conflicts with the CMake Generator.
- Keep the v2 executable separate from the legacy Qt Widgets executable until an approved slice explicitly changes that boundary.

### Work-order requirements

Every frontend work order must identify:

- the exact applicable Qt skills from **Qt skill routing**, their order, and any skill capabilities intentionally excluded as out of scope;
- for any QML job, that the agent must load and apply the `qt-qml` skill before writing, reviewing, diagnosing, refactoring, optimizing, or validating QML; this supplements the official Qt Documentation requirement for Qt API, version, and Design Studio behavior;
- for a QML job affecting the visual-review topics, the exact relevant sections of `docs/v2/OpenDSS_v2_UIUX_Visual_Review_Amendment_2026-07-23.md` as required reading;
- visual `*.ui.qml` files authorized for modification;
- visual files that are read-only;
- wrapper QML, C++, mock, test, and durable CMake files authorized for modification;
- generated files that must not be edited;
- the Qt Design Studio states and window/scaling conditions that must be previewed.

### Validation

The bounded work order selects only proportional, applicable checks. For the current visual-scaffold rounds:

1. User-led Qt Design Studio 2D view, Live Preview, and manual visual review are primary.
2. Check keyboard focus in a maximized window using the current display's full available work area. Do not restore or resize the window to a fixed test resolution.
3. Run `qmllint` only for changed QML when useful.
4. Run one configure/build or directly relevant targeted test only when the changed files require it.
5. Do not run the full legacy or hardware test suites.

Always verify that no generated file was unintentionally hand-edited and no production QML calls hardware directly.

## Required v2 task context

For v2 implementation work:

1. Read `AGENTS.md`.
2. Read `docs/v2/CONTEXT.md`.
3. Read `docs/v2/implementation/current-slice.md`.
4. Read only the canonical sections referenced by the current slice.
5. Report a conflict instead of silently resolving it.
6. Work on one approved slice at a time.

## Role-locked primary orchestrators

- A primary OpenDSS thread explicitly assigned functional or integration ownership must load the repository-local `opendss-orchestrator` skill first, then `opendss-functional-orchestrator`.
- A primary OpenDSS thread explicitly assigned visual, design, or graphics ownership must load the repository-local `opendss-orchestrator` skill first, then `opendss-design-orchestrator`, plus the Qt skills required by **Qt skill routing**.
- Declare one role lock and retain it for the thread lifetime. Change it only when the user explicitly reassigns the role.
- These role skills do not apply to `opendss-worker` or `opendss-reviewer` agents.
- Mock ownership is path-specific: design may own only explicitly named design-time files in the generated project's `MockDatas/` folder (or another exact design-only path named by authority); functional may own `app/runtime/Desktop_app_v2/Desktop_app_v2/MockAppState.qml` and other runtime/mock backbone state only when an exact work order names them. The word `mock` alone assigns no ownership; pause and consult the user for another or uncertain path.
- Base orchestrator rules and canonical authority always win.

## Lean implementation rules

- Make the smallest coherent change that satisfies the current slice. Prefer direct, conventional code over generalized frameworks, and do not add code for hypothetical future requirements.
- Every new production class, function, field, flag, and abstraction must have an immediate production consumer or a required characterization test.
- Do not add unused hooks, placeholder APIs, commented-out implementations, speculative configuration, or TODO scaffolding.
- Do not add a factory, registry, plugin system, strategy selector, service locator, or dependency-injection framework unless the current slice demonstrates the need.
- A hardware or test boundary may justify a narrow interface limited to behavior currently required by real consumers.
- Do not maintain duplicate authoritative state or retain an obsolete path merely “in case.”
- A temporary parallel implementation requires a documented reason, a concrete removal condition, and tests protecting the retained behavior.
- Remove code made unreachable by the current slice when removal is safe, tested, and in scope. When immediate removal is unsafe, identify the exact missing evidence instead of creating permanent compatibility machinery.
- Preserve qualified DCAM, NI-DAQmx, detector, ONNX, trainer, export, and persistence mechanics unless the current slice explicitly authorizes a behavior change.
- Do not perform broad cleanup or unrelated refactoring in a feature PR. New code must not introduce avoidable warnings.
- Comments explain non-obvious constraints or rationale; they do not narrate straightforward code.

## Simplicity is a hard constraint

Every agent must implement the smallest direct solution that satisfies the approved current requirement. Do not add unrequested or convenience features, optional modes, extra settings, states, controls, hidden fallbacks, speculative future capability, generalized frameworks, extension or plugin systems, event buses, service locators, dependency-injection frameworks, single-implementation factories or interfaces without a current boundary, registries, excessive wrappers, placeholder backends, random mock behavior, or unrelated refactors.

Future flexibility, scalability, extensibility, or a subjective claim of cleaner architecture is not a current requirement. Prefer direct code, explicit properties, ordinary Qt mechanisms, small readable functions, one obvious execution path, one authoritative owner for each state, deterministic mock values, existing files that already own the responsibility, and the fewest necessary files and layers.

Every work order, worker handoff, validation report, and orchestrator acceptance decision must answer: **What is the simplest implementation that fully satisfies the current requirement?** Reject work when a materially simpler implementation satisfies the same acceptance criteria. Every new file, component, class, abstraction, or layer requires a concrete current consumer or repository boundary; otherwise, do not create it.

## User-led visual editing

After a visual baseline is accepted, the user edits `*.ui.qml` forms in Qt Design Studio and that Git diff is authoritative. The orchestrator inspects the changed properties and signals, then issues a narrow integration work order limited to the required wrapper, controller, or targeted test files. Workers must not rewrite the user-edited form unless an explicit UI-form work order authorizes it. Build and runtime validation belongs in Qt Creator, followed by user visual review.

An accepted `*.ui.qml` form is protected by default. An agent may modify it only when the current work order names that exact form as an authorized write and includes the required Qt Design Studio 2D-view, Live Preview, window/scaling, and focus validation.

## Visual scaffold two-round workflow

For the current visual-scaffold slice, Round 1 explicitly permits one visual writer to create Qt Design Studio-editable skeletal hosts for every approved workspace using only approved regions and headings. Do not add speculative controls or behavior. The user-accepted Round 1 form diff is the visual contract; freeze its exact exported aliases, signals, and state names before Round 2, and require the orchestrator to issue exact bounded work orders and obtain user acceptance between rounds.

Round 2 uses separate isolated worktrees with nonoverlapping ownership: design owns authorized `*.ui.qml` forms, tokens, assets, design-time visual states, and explicitly named design-time mock files in `MockDatas/`; backbone owns ordinary QML wrappers, C++ authoritative state, narrow adapters, directly relevant tests, and explicitly authorized durable CMake files. The backbone remains hardware-free in these rounds: no vendor SDK, TIFF, DAQ, detector, inference, trainer, Run, Results, or persistence implementation.

Validation is proportional to the files changed: workers run only targeted checks, the orchestrator runs one integrated check only when needed, and the user leads Qt Design Studio and manual visual review. Do not run the full legacy or hardware matrix, or repeat a successful check without a relevant change.
