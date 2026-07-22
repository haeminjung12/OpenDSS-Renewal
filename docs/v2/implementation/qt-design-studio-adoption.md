# Qt Design Studio workflow adoption

## Status

Adopted and bootstrap-complete on 2026-07-22 as an implementation constraint for the OpenDSS v2 Qt Quick frontend. This record does not approve or elevate the Consolidated Product Design Specification; the authority order in `docs/v2/CONTEXT.md` remains unchanged.

## Adopted operating model

| Surface | Ownership |
|---|---|
| Qt Design Studio | Visual forms, components, tokens, assets, visual states, transitions, mock-backed previews |
| Qt Creator | Runtime QML, C++, durable CMake integration, builds, debugging, profiling, and tests |
| Codex | Bounded work orders, architecture enforcement, multi-file implementation, verification, and review |

The required implementation pattern is a designer-editable `*.ui.qml` form, an ordinary `*.qml` behavior wrapper, one authoritative C++/application-state projection where production behavior is needed, and a matching design-time mock interface. Production logic never belongs in the form.

## Environment audit

The following local tools were found and no download is currently required:

- Qt Design Studio 4.8.2: `C:\Qt\Tools\QtDesignStudio\bin\qtdesignstudio.exe`
- Qt Creator 20.0.0: `C:\Qt\Tools\QtCreator\bin\qtcreator.exe`
- Qt 6.11.1 MinGW kit tooling, including `qmllint` and `qmltestrunner`: `C:\Qt\6.11.1\mingw_64\bin\`
- CMake 4.3.2: `C:\Program Files\CMake\bin\cmake.exe`
- Ninja: `C:\Qt\Tools\Ninja\ninja.exe`

Qt's current official agent-skill repository publishes six Codex-compatible skills, and all six are already installed: `qt-qml`, `qt-qml-review`, `qt-cpp-review`, `qt-qml-docs`, `qt-cpp-docs`, and `qt-qml-profiler`. The names `qt-ui-design`, `qt-cmake-project`, `qt-qml-test`, and `qt-qml-test-run` from the earlier recommendation are not present in the current official bundle and were not fabricated locally.

The project Codex configuration registers:

- the official Qt Documentation MCP endpoint as `qtDocumentation`;
- Qt Creator 20.0.0 as `qtCreator`, through a repository-local stdio compatibility bridge targeting the server-disclosed Streamable HTTP root endpoint `http://127.0.0.1:45678/`.

The Qt Creator endpoint was verified with an MCP `initialize` request using protocol version `2025-03-26`; the response identified `qt-creator-mcp-server` version 20.0.0 and returned a valid MCP session ID.

Qt Creator 20.0.0 rejects the standard `MCP-Protocol-Version` header on the initialized notification, while the current Codex Streamable HTTP client always sends it. `.codex/qt_creator_mcp_bridge.mjs` forwards Codex's stdio MCP messages to the verified endpoint and deliberately omits only that rejected header. Remove the bridge and restore a direct `url` entry after either side resolves this protocol-header incompatibility.

## Repository changes made for adoption

- Added the enforceable Qt Design Studio compatibility contract to `AGENTS.md`.
- Added the official Qt Documentation MCP endpoint to `.codex/config.toml`.
- Added the Design Studio bootstrap gate and Design Studio validation requirements to `current-slice.md`.
- Updated `docs/v2/CONTEXT.md` so future work sees the adopted boundary before the current slice.
- Added the exact Qt Creator MCP endpoint to repository-scoped Codex configuration and validated its tools through the live server.
- Created repo-local grepai configuration. Initial indexing is still pending because the single background watcher is currently attached to a different WSL project; do not terminate that unrelated watcher implicitly.
- Created a local code-only Graphify graph at `graphify-out/graph.json` (3,201 nodes and 8,479 edges). A full documentation-aware extraction and clustered `GRAPH_REPORT.md` remain optional follow-up when the local semantic backend is available.

Qt Design Studio generated the untouched application baseline at `app/runtime/Desktop_app_v2/`. It configures and builds successfully in Qt Creator with Desktop Qt 6.11.1 MinGW 64-bit using `C:\Users\goals\qtbuild\odss-v2-dbg`. Codex did not create or alter its visual form, QML, C++, or generated CMake source.

The repository-local `app/runtime/Desktop_app_v2App-Debug/` directory is a generated build-output tree, is excluded by `.gitignore`, and is not part of the source baseline.

## Bootstrap verification

1. Source root: `app/runtime/Desktop_app_v2/`.
2. Active Qt Creator project: `Desktop_app_v2App` from the generated root `CMakeLists.txt`.
3. Active build configuration: `Debug`.
4. Qt kit: Qt 6.11.1 at `C:\Qt\6.11.1\mingw_64`.
5. External build directory: `C:\Users\goals\qtbuild\odss-v2-dbg`.
6. Qt Creator MCP listener: localhost only, port 45678, cross-origin access disabled.
7. MCP transport: Streamable HTTP at `http://127.0.0.1:45678/`.
8. A fresh, ephemeral Codex client loaded the repository configuration, enumerated the Qt Creator MCP server, and successfully called `get_current_project`, `get_current_build_config`, `get_qt_directory`, `list_projects`, and `get_build_status`.
9. Qt Creator reported no active build and zero current issues after build verification; the external Debug executable exists under `C:\Users\goals\qtbuild\odss-v2-dbg`.

The starter `Screen01.ui.qml` remains the untouched Qt Design Studio template. It is not an approved visual baseline for the OpenDSS shell or Single Image workflow.

## Discovered Qt Creator MCP tools

The live Qt Creator 20.0.0 server exposed 64 tools:

```text
add_breakpoint
add_watch_expression
build
call_action
close_file
create_new_file
debug
debugger_continue
debugger_get_status
debugger_interrupt
debugger_step_in
debugger_step_out
debugger_step_over
delete_breakpoint
evaluate_expression
execute_command
file_plain_text
find_actions
find_files_in_projects
get_breakpoints
get_build_status
get_call_stack
get_current_build_config
get_current_project
get_current_session
get_last_test_results
get_qt_directory
get_run_configurations
get_test_details
get_test_status
get_threads
get_variable
get_variables
known_repositories_in_projects
list_build_configs
list_file_issues
list_issues
list_open_files
list_projects
list_sessions
list_tests
list_visible_files
load_session
open_file
project_dependencies
quit
reformat_file
remove_watch_expression
replace_in_directory
replace_in_file
replace_in_files
run_project
run_tests
save_file
save_session
search_in_directory
search_in_file
search_in_files
select_frame
select_thread
set_file_plain_text
set_variable
stop_debug
switch_build_config
```

## Continuing the current slice

After this untouched generated application baseline is committed, resume `Qt Quick/QML v2 Shell and Mock Single Image` without changing slices:

1. Read `AGENTS.md`, `docs/v2/CONTEXT.md`, and `docs/v2/implementation/current-slice.md`.
2. Create and approve the shell and Single Image visual baseline in Qt Design Studio before backend integration.
3. Once that visual baseline is committed, authorize exact `*.ui.qml` forms in the work order and add ordinary QML wrappers, the narrow mock authoritative state owner, the fake Camera service, and tests required by the current slice.
4. Keep DCAM, DAQ, real TIFF output, Python, GPU, production persistence, and legacy Widgets changes out of scope.
5. Build the standalone v2 target, run `qmllint`, run relevant Qt Quick/C++ tests, and verify 2D view plus Live Preview before review.
6. Stop after this slice is reviewed; the real DCAM/TIFF slice remains separately authorized work.

A suitable continuation request is:

> Continue the current OpenDSS v2 Shell and Mock Single Image slice by creating and approving the Qt Design Studio visual baseline first. Preserve the form-wrapper-controller boundary, do not add production integration before that visual commit, and do not start the later DCAM/TIFF slice.

## Primary references

- [Qt Design Studio designer-developer workflow](https://doc.qt.io/qtdesignstudio/studio-designer-developer-workflow.html)
- [Qt Design Studio UI file restrictions](https://doc.qt.io/qtdesignstudio/creator-quick-ui-forms.html)
- [Qt Design Studio CMake Generator and mock data](https://doc.qt.io/qtdesignstudio/studio-cmake-generator.html)
- [Qt Creator MCP server setup](https://doc.qt.io/qtcreator/creator-how-to-mcp-server.html)
- [Qt official agent skills](https://github.com/TheQtCompanyRnD/agent-skills)
