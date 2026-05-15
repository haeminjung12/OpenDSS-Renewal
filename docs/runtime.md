# Runtime

## Active runtime layout

- Shared runtime code: `app/runtime/`
- Desktop app: `app/runtime/desktop_app/`
- Runtime models: `app/runtime/models/`
- No-hardware tests: `app/runtime/tests/`

The accepted runtime now splits shared non-GUI logic into the `opendss_runtime` static library and links that into the
desktop executable.

## Executable behavior

The primary executable is:

`C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe`

Supported runtime modes still include:

- normal camera mode
- `--mock-camera`
- `--no-daq`
- `--test-mode`
- `--verify-camera-workspace`
- `--verify-daq-settings`

## Safe no-hardware commands

Basic safe smoke:

```powershell
& "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --mock-camera --no-daq --no-startup-prompts
```

Camera workspace verifier:

```powershell
& "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --verify-camera-workspace
```

DAQ settings verifier:

```powershell
& "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --verify-daq-settings
```

CTest no-hardware target:

```powershell
ctest --test-dir "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" -C Release --output-on-failure
```

## Runtime dependencies

- Qt: `C:\Qt\6.10.1\msvc2022_64`
- ONNX Runtime: `C:\onnxruntime-gpu`
- OpenCV/vcpkg runtime DLLs: `C:\vcpkg\installed\x64-windows\bin`
- Hamamatsu DCAM SDK: currently referenced from the old repo archive path until a cleaner SDK location is configured
- NI-DAQmx: external system install; do not bundle DAQ drivers in this repo

When launching undeployed builds, put known runtime paths first in `PATH` to avoid stale DLL resolution. The accepted
desktop build also copies `onnxruntime.dll` into the executable directory after build.

## Hardware boundary

- Physical camera hardware is not required for the metadata loader test or for the documented verifier-only paths.
- Physical NI hardware must not be required in CI.
- DCAM SDK software is still required to build the accepted desktop target on Windows.

## DAQ safety

Do not fire DAQ output unless the user explicitly approves the exact action.
