# Local Build Notes: Haemin

This file is intentionally machine-specific. Keep personal path notes here instead of in the shared build guide.

## Recorded local setup

- Build directory: `C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release`
- Preferred fallback build directory for verification: `C:\Users\goals\Codex\OpenDSS\build-opendss-clean-verify-vs18`
- Qt: `C:\Qt\6.10.1\msvc2022_64`
- ONNX Runtime: `C:\onnxruntime-gpu`
- DCAM SDK: `C:\Users\goals\Codex\CNN for Droplet Sorting\archive\python_pipeline\Hamamatsu_DCAMSDK4_v25056964\dcamsdk4`
- NI-DAQmx include: `C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\include`
- NI-DAQmx library: `C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc\NIDAQmx.lib`

## Generator note

`Visual Studio 17 2022` was not available during the 2026-05-14 local verification. `Visual Studio 18 2026` worked
with the same dependency paths.

## Separation rule

Keep shared instructions in `docs/build.md`. Keep user-specific paths, generators, and workstation notes here.
