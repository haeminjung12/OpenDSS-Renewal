# Refactor Baseline

Date: 2026-05-14
Branch: `refactor/maincpp-decomposition`

## Commit hash

`9cd154be9e62c0af12d02b8b153498b94b5192cb`

## Build environment

- Host OS: Windows (`America/Chicago` workspace session, 2026-05-14)
- CMake generator: `Visual Studio 18 2026`
- Windows SDK selected by CMake: `10.0.26100.0`
- Build directory: `C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release`
- Qt prefix: `C:\Qt\6.10.1\msvc2022_64`
- ONNX Runtime: `C:\onnxruntime-gpu`
- DCAM SDK: `C:\Users\goals\Codex\CNN for Droplet Sorting\archive\python_pipeline\Hamamatsu_DCAMSDK4_v25056964\dcamsdk4`
- NI-DAQmx include: `C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\include`
- NI-DAQmx library: `C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc\NIDAQmx.lib`

## Build command

Configure:

```powershell
rtk cmake -S "C:\Users\goals\Codex\OpenDSS\0. Codebase\app\runtime" -B "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" -G "Visual Studio 18 2026" -A x64 -D CMAKE_PREFIX_PATH="C:\Qt\6.10.1\msvc2022_64" -D ONNXRUNTIME_DIR="C:\onnxruntime-gpu" -D DCAM_SDK_DIR="C:\Users\goals\Codex\CNN for Droplet Sorting\archive\python_pipeline\Hamamatsu_DCAMSDK4_v25056964\dcamsdk4" -D NIDAQMX_INCLUDE_DIR="C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\include" -D NIDAQMX_LIBRARY="C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc\NIDAQmx.lib" -D ENABLE_NIDAQMX=ON -D BUILD_QT_GUI=ON
```

Build:

```powershell
rtk cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target desktop_app
```

## Build result

- Configure: success
- Generate: success
- Target build: success
- Built executable: `C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe`
- Runtime deploy checks:
  - `platforms\qwindows.dll`: present
  - `onnxruntime.dll`: present

## Runtime launch command

Normal hardware mode:

```powershell
& "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe"
```

Safe no-hardware smoke / camera workspace verifier:

```powershell
& "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --verify-camera-workspace
```

Safe no-hardware DAQ settings verifier:

```powershell
& "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --verify-daq-settings
```

Note: GUI launch was not executed during this audit. Per task scope and repo DAQ safety rules, this baseline records safe commands without risking hardware-side effects.

## Existing runtime flags

- Normal hardware mode: no flag required.
- `--mock-camera`
  - Sets `mockCamera = true`.
- `--no-daq`
  - Sets `noDaq = true`.
- `--test-mode`
  - Sets `testMode = true`.
  - Also forces `mockCamera = true`, `noDaq = true`, and `noStartupPrompts = true`.
- Additional observed runtime flags:
  - `--no-startup-prompts`
  - `--workspace=<name>` or `--workspace <name>`
  - `--dataset-builder-review-manifest=<path>` or `--dataset-builder-review-manifest <path>`

## Existing self-test flags

- `--verify-camera-workspace`
  - Triggers the camera workspace verifier via `QTimer::singleShot(...)`.
- `--verify-daq-settings`
  - Triggers the DAQ settings verifier via `QTimer::singleShot(...)`.

## Known warnings

- Configure warning:
  - `Could NOT find WrapVulkanHeaders (missing: Vulkan_INCLUDE_DIR)`
- Configure status:
  - `NI-DAQmx enabled: include='C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/include', library='C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/lib64/msvc/NIDAQmx.lib'`
- Build warnings:
  - None observed in the `desktop_app` Release build output captured for this audit.

## Known broken behavior

- `docs/current-state.md` records that sequence summary CSV is not created by the current sequence verifier path.
- `docs/current-state.md` also records pending interactive manual functionality testing in the historical workspace.

## Notes

- The requested branch did not exist locally at audit start and was created successfully: `refactor/maincpp-decomposition`.
- Current worktree also contains a pre-existing untracked file outside this task's write scope:
  - `docs/Refactor Doc.md`
- No source, CMake, or runtime behavior was changed during this baseline audit.
