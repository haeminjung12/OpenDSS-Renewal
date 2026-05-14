# Build

Use an out-of-tree build directory.

```powershell
cmake -S "C:\Users\goals\Codex\OpenDSS\0. Codebase\app\runtime" -B "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" `
  -G "Visual Studio 17 2022" -A x64 `
  -D CMAKE_PREFIX_PATH="C:\Qt\6.10.1\msvc2022_64" `
  -D ONNXRUNTIME_DIR="C:\onnxruntime-gpu" `
  -D DCAM_SDK_DIR="C:\Users\goals\Codex\CNN for Droplet Sorting\archive\python_pipeline\Hamamatsu_DCAMSDK4_v25056964\dcamsdk4" `
  -D NIDAQMX_INCLUDE_DIR="C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\include" `
  -D NIDAQMX_LIBRARY="C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc\NIDAQmx.lib" `
  -D ENABLE_NIDAQMX=ON `
  -D BUILD_QT_GUI=ON

cmake --build "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release" --config Release --target desktop_app
```

Verify runtime files:

```powershell
Test-Path "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\platforms\qwindows.dll"
Test-Path "C:\Users\goals\Codex\OpenDSS\build-opendss-internal-release\desktop_app\Release\onnxruntime.dll"
```

## Local Verification Note

On 2026-05-14 this machine did not have the `Visual Studio 17 2022` CMake generator installed. A configure and Release build succeeded with `Visual Studio 18 2026` in:

`C:\Users\goals\Codex\OpenDSS\build-opendss-clean-verify-vs18`

Use the same dependency paths as above and replace the generator with:

```powershell
-G "Visual Studio 18 2026" -A x64
```
