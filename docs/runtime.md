# Runtime

## Safe No-DAQ Smoke

```powershell
& "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\OpenVisualDropletSorter.exe" --test-mode --mock-camera --no-daq --no-startup-prompts
```

## Runtime Dependencies

- Qt: `C:\Qt\6.10.1\msvc2022_64`
- ONNX Runtime: `C:\onnxruntime-gpu`
- OpenCV/vcpkg runtime DLLs: `C:\vcpkg\installed\x64-windows\bin`
- Hamamatsu DCAM SDK: currently referenced from the old repo archive path until a cleaner SDK location is configured.
- NI-DAQmx: external system install; do not bundle DAQ drivers in this repo.

When launching undeployed builds, put known runtime paths first in `PATH` to avoid stale DLL resolution.

## DAQ Safety

Do not fire DAQ output unless the user explicitly approves the exact action.
