# Third-Party Notices

This file summarizes third-party components and vendor prerequisites referenced by the current OpenDSS repository and packaging scripts. It is an engineering redistribution note, not legal advice.

## Project License

The top-level license for first-party project code in this repository is Apache-2.0; see [LICENSE](LICENSE). The Python trainer package metadata is aligned with that top-level first-party license.

## Bundled With The Repository Or Current Package Flow

### Qt 6 runtime libraries and plugins

- Evidence in repo:
  - `app/runtime/scripts/package_portable.ps1` runs `windeployqt`.
  - `app/runtime/scripts/check_package.ps1` requires `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, and `platforms\\qwindows.dll`.
- Public redistribution note:
  - The package flow clearly bundles Qt runtime files.
  - Qt is dual-licensed. Public binary distribution should include the applicable Qt notices and comply with the license path chosen for the shipped Qt build.
- Official pages:
  - https://www.qt.io/development/download-open-source
  - https://doc.qt.io/qt-6/licensing.html
  - https://www.qt.io/development/open-source-lgpl-obligations

### ONNX Runtime

- Evidence in repo:
  - `app/runtime/CMakeLists.txt` and `app/runtime/desktop_app/CMakeLists.txt` require ONNX Runtime.
  - `app/runtime/desktop_app/CMakeLists.txt` copies `onnxruntime.dll` and may copy provider DLLs.
  - `app/runtime/scripts/check_package.ps1` requires `onnxruntime.dll`.
- Public redistribution note:
  - The current package flow bundles ONNX Runtime runtime DLLs.
  - The official ONNX Runtime repository identifies the project as MIT-licensed.
- Official pages:
  - https://onnxruntime.ai/
  - https://github.com/microsoft/onnxruntime/blob/main/LICENSE

### OpenCV runtime DLLs

- Evidence in repo:
  - `app/runtime/CMakeLists.txt` and `app/runtime/desktop_app/CMakeLists.txt` require OpenCV components `core`, `imgproc`, and `imgcodecs`.
  - `app/runtime/scripts/package_portable.ps1` copies `opencv_*4.dll` from the local vcpkg bin directory when available.
  - `app/runtime/scripts/check_package.ps1` requires `opencv_core4.dll`, `opencv_imgproc4.dll`, and `opencv_imgcodecs4.dll`.
  - The current portable artifact manifest at `C:\Users\goals\Codex\OpenDSS\artifacts\internal-release\portable\OpenVisualDropletSorterSuite_20260709_230805\package_manifest.json` records `opencv_core4.dll`, `opencv_imgproc4.dll`, and `opencv_imgcodecs4.dll` as present.
  - The packaged DLL metadata for `opencv_core4.dll`, `opencv_imgproc4.dll`, and `opencv_imgcodecs4.dll` reports `FileVersion=4.12.0` and `ProductVersion=4.12.0`.
- Public redistribution note:
  - The package flow bundles OpenCV runtime DLLs.
  - The currently inspected packaged OpenCV runtime is version `4.12.0`.
  - The official OpenCV license page states OpenCV `4.5.0` and higher use Apache-2.0, so the inspected `4.12.0` runtime falls under Apache-2.0.
  - If a future package ships different OpenCV DLLs, re-verify the DLL metadata before reusing this notice.
- Official pages:
  - https://opencv.org/license/

### Bundled model assets

- Evidence in repo:
  - `app/runtime/desktop_app/CMakeLists.txt` copies `app/runtime/models` into the output package.
  - `app/runtime/scripts/package_portable.ps1` and `app/runtime/scripts/check_package.ps1` require the model registry and referenced model assets.
  - `app/runtime/models/model_registry.json` is the active manifest.
- Files currently referenced by the registry/package flow:
  - `app/runtime/models/model_registry.json`
  - `app/runtime/models/squeezenet_final_new_condition.onnx`
  - `app/runtime/models/metadata.json`
  - `app/runtime/models/model.onnx.data`
  - `app/runtime/models/pre_binary_promotion_backup.onnx`
  - `app/runtime/models/pre_binary_promotion_backup_metadata.json`
- Public redistribution note:
  - The repository owner approved public redistribution of the bundled model files for this release on 2026-07-10.

### Packaged trainer source and dependency manifests

- Evidence in repo:
  - `app/runtime/scripts/package_portable.ps1` copies `training/python/` content into the package.
  - `app/runtime/scripts/check_package.ps1` requires trainer scripts and requirements files in the packaged layout.
- What is bundled:
  - `training/python/pyproject.toml`
  - `training/python/README-windows-training.md`
  - `training/python/droplet_trainer/`
  - `training/python/scripts/windows/`
  - `training/python/requirements/`
- What is not bundled by the current package flow:
  - Python itself
  - user virtual environments
  - PyTorch wheels
  - CUDA toolkits/runtime
  - datasets
  - checkpoints
  - generated outputs

## Required But Installed Separately By The User

### Hamamatsu DCAM SDK/runtime

- Evidence in repo:
  - `app/runtime/CMakeLists.txt` and `app/runtime/desktop_app/CMakeLists.txt` use `DCAM_SDK_DIR` and link `dcamapi`.
  - The build configuration links against a system DCAM SDK/runtime.
  - `app/runtime/scripts/check_package.ps1` checks for `dcamapi.dll` as an external prerequisite.
- Redistribution note:
  - Treat DCAM SDK/runtime as a separately installed vendor prerequisite unless separate redistribution approval is documented.
- Official pages:
  - https://www.hamamatsu.com/eu/en/product/cameras/software/driver-software.html
  - https://www.hamamatsu.com/all/en/product/cameras/software/driver-software/dcam-sdk4.html
  - https://www.hamamatsu.com/jp/en/product/cameras/driver-software/dcam-api-for-windows.html

### NI-DAQmx runtime/driver

- Evidence in repo:
  - `ENABLE_NIDAQMX` is optional in CMake and a stub build exists when it is off.
  - `app/runtime/installer/preinstall_note.txt` says NI-DAQmx is required for the accepted NI-enabled build because the app imports `nicaiu.dll`.
  - `app/runtime/scripts/check_package.ps1` checks for `nicaiu.dll` as an external prerequisite.
  - `app/runtime/scripts/package_portable.ps1` can copy `NIDAQmx*.dll` only when requested for internal validation.
  - `app/runtime/installer/installer.iss` does not bundle or run NI-DAQmx installer payloads in the public prerequisite-only installer flow.
- Redistribution note:
  - For public release, treat NI-DAQmx as a user-installed prerequisite unless redistribution rights are separately confirmed and documented.
  - The public installer flow does not bundle NI-DAQmx installer payloads. Treat any internal NI runtime DLL copying as blocked for public redistribution unless separate rights are confirmed and documented.
- Official pages:
  - https://www.ni.com/
  - https://knowledge.ni.com/KnowledgeArticleDetails?id=kA0VU0000003eH30AI&l=en-CA

### Microsoft Visual C++ Redistributable x64

- Evidence in repo:
  - `app/runtime/installer/preinstall_note.txt` says the Microsoft Visual C++ Redistributable x64 is required on clean target machines.
  - `app/runtime/installer/build_installer.ps1` ignores any legacy `-VcRedist` argument for the public prerequisite-only installer flow.
  - `app/runtime/installer/installer.iss` does not bundle or run Microsoft Visual C++ Redistributable installer payloads.
- Redistribution note:
  - The public installer flow treats the redistributable as a user-installed prerequisite and points users to the official Microsoft download page.
- Official pages:
  - https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170
  - https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170

## Referenced By Bundled Trainer Manifests But Not Bundled By Current Package Flow

These packages appear in `training/python/pyproject.toml` and `training/python/requirements/*.txt`, but the current package flow copies requirement manifests and helper scripts only. Users install these separately into their own Python environment.

- Python `>=3.10,<3.13`
- `torch`
- `torchvision`
- `numpy`
- `pillow`
- `scikit-learn`
- `pandas`
- `onnx`
- `onnxruntime`
- `onnxscript`

Representative official pages:

- https://pytorch.org/
- https://numpy.org/
- https://scikit-learn.org/
- https://onnxruntime.ai/

## Components Requiring Redistribution Confirmation Before Public Bundling

- Hamamatsu DCAM SDK/runtime
  - Reason: repo treats it as an external prerequisite and includes no bundled redistribution permission.
- NI-DAQmx DLLs or installer payloads
  - Reason: public installer flow is prerequisite-only, and the repo contains no redistribution confirmation for bundling NI artifacts.
- Microsoft Visual C++ Redistributable installer payloads
  - Reason: public installer flow is prerequisite-only, and the repo contains no checked-in redistribution terms for bundling the installer payload.
## Repository Evidence Checked

- `README.md`
- `app/runtime/CMakeLists.txt`
- `app/runtime/desktop_app/CMakeLists.txt`
- `app/runtime/installer/build_installer.ps1`
- `app/runtime/installer/installer.iss`
- `app/runtime/installer/preinstall_note.txt`
- `app/runtime/scripts/package_portable.ps1`
- `app/runtime/scripts/check_package.ps1`
- `C:\Users\goals\Codex\OpenDSS\artifacts\internal-release\portable\OpenVisualDropletSorterSuite_20260709_230805\package_manifest.json`
- Packaged OpenCV DLL metadata from:
  - `C:\Users\goals\Codex\OpenDSS\artifacts\internal-release\portable\OpenVisualDropletSorterSuite_20260709_230805\opencv_core4.dll`
  - `C:\Users\goals\Codex\OpenDSS\artifacts\internal-release\portable\OpenVisualDropletSorterSuite_20260709_230805\opencv_imgproc4.dll`
  - `C:\Users\goals\Codex\OpenDSS\artifacts\internal-release\portable\OpenVisualDropletSorterSuite_20260709_230805\opencv_imgcodecs4.dll`
- `app/runtime/models/model_registry.json`
- `training/python/pyproject.toml`
- `training/python/requirements/windows-py312-common-constraints.txt`
- `training/python/requirements/windows-py312-cpu.txt`
- `training/python/requirements/windows-py312-gpu-cu128.txt`
- `training/python/requirements/windows-py312-gpu-cu130.txt`
