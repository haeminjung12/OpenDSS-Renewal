# OpenDSS

OpenDSS is an open droplet sorting suite for live camera inspection, ONNX-based event classification, and NI hardware triggering on Windows.

<p align="center">
  <img src="assets/branding/opendss-primary-full-color.svg" alt="OpenDSS logo" width="520">
</p>

Public Windows binary releases are distributed through [GitHub Releases](https://github.com/haeminjung12/OpenDSS_clean/releases/latest). Vendor drivers and Microsoft runtime installers are treated as user-installed prerequisites.

## What It Does

OpenDSS is a desktop application for running a droplet-sorting workflow from a Windows workstation. It combines live camera viewing, detector controls, model-backed inference, hardware settings, and DAQ triggering in one Qt-based interface.

It is designed for lab setups that use Hamamatsu camera hardware and NI output hardware for trigger delivery.

## Key Features

- Live camera view with dedicated camera controls
- Detector and event-processing controls for real-time inspection workflows
- ONNX Runtime inference for model-backed classification
- NI-DAQmx trigger output for hardware-integrated sorting setups
- Hardware settings workspace for camera and DAQ configuration
- Models workspace for creating, fine-tuning, testing, saving, and activating models
- Packaged MobileNetV3-Small (faster) and EfficientNet-B0 (more accurate) starters
- Windows training helpers with CPU and optional NVIDIA CUDA training environments

## Hardware Requirements

### Application-only use

- Windows 10 or Windows 11
- Microsoft Visual C++ Redistributable x64 if it is not already installed

The Models Library, Train, and Test workflows can be installed and tested without a camera or NI DAQ device. Packaged ONNX inference uses the qualified CPU provider. CUDA is used by the optional PyTorch training environment, not by production ONNX inference.

### Hardware-integrated operation

- A supported Hamamatsu camera plus Hamamatsu DCAM-API / DCAM-SDK for live acquisition
- Supported NI DAQ hardware plus NI-DAQmx for trigger output

## Downloads

### Application

- Public installer release: [GitHub Releases](https://github.com/haeminjung12/OpenDSS_clean/releases/latest)
- Release assets are distributed through GitHub Releases.
- Release packages include Blank and Pre-trained MobileNetV3-Small and EfficientNet-B0 packages. Blank packages start training from bundled ImageNet weights; Pre-trained packages include trainable OpenDSS checkpoints and ONNX models for immediate CPU inference and Test use.
- The public installer does not bundle or run NI-DAQmx, Hamamatsu DCAM, or Microsoft Visual C++ Redistributable installers; install those prerequisites separately.

### Vendor Prerequisites

- Hamamatsu DCAM-API for Windows: [official download page](https://www.hamamatsu.com/jp/en/product/cameras/driver-software/dcam-api-for-windows.html)
- Hamamatsu DCAM-SDK4: [official product page](https://www.hamamatsu.com/all/en/product/cameras/software/driver-software/dcam-sdk4.html)
- NI-DAQmx Runtime / driver install guidance: [official NI article](https://knowledge.ni.com/KnowledgeArticleDetails?id=kA0VU0000003eH30AI&l=en-CA)
- Microsoft Visual C++ Redistributable x64: [latest supported downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)
- Qt for source builds: [Qt Online Installer](https://www.qt.io/development/download-open-source)
- ONNX Runtime for source builds: [installation guide](https://onnxruntime.ai/docs/install/)

### Python Trainer

The release includes trainer helper scripts under `training/python`, but Python, PyTorch, CUDA, datasets, checkpoints, and virtual environments are installed separately by the user. Install Python 3.12 x64, then run this from `training/python`:

```powershell
.\scripts\windows\create-training-venv.ps1 -Python py -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv"
.\scripts\windows\install-training-cpu.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv"
```

`install-training-cpu.ps1` verifies the environment and updates the app's Python trainer setting to the selected environment's `Scripts\python.exe`.

For GPU training, create a separate environment and use one CUDA-specific installer wrapper instead of the CPU installer: `install-training-gpu-cu130.ps1` or `install-training-gpu-cu128.ps1`. Use the wrapper compatible with the target computer's NVIDIA driver. Each wrapper installs its pinned PyTorch runtime, requires `torch.cuda.is_available()` to pass, and updates the app's trainer setting. It does not install or change the NVIDIA driver.

## Quick Start

1. Build from source, or use the public installer after it is posted to [GitHub Releases](https://github.com/haeminjung12/OpenDSS_clean/releases/latest).
2. Install Microsoft Visual C++ Redistributable x64 if it is not already present.
3. Launch OpenDSS. The Models workspace can be evaluated without camera or DAQ hardware.
4. In Models > Library, confirm the two Blank and two Pre-trained MobileNet/EfficientNet entries appear. Blank entries are training starters and cannot be activated until trained; Pre-trained entries can be activated and tested immediately.
5. For training, install Python 3.12 x64 and create either the CPU environment or one supported CUDA environment using the bundled `training\python` scripts.
6. Select a prepared dataset manifest. Dataset metadata determines whether the model has two or three output classes. A trained checkpoint may be continued when its output count matches the dataset.
7. Use Models > Train, then Save/Use to register the completed package. Confirm activation when prompted. Models > Test lists inference-capable packages and evaluates the selected model.
8. Install DCAM and NI-DAQmx only before testing the corresponding camera or DAQ workflow.

Training writes a self-contained model package containing `metadata.json`, `checkpoint.pth`, and `model.onnx`. If training reports class collapse or missing prediction classes, the run completed computationally but its model was rejected as scientifically unusable; review the dataset balance and class labels rather than bypassing the safeguard.

## Screenshots

![Live view overview](assets/screenshots/01-live-view-overview.png)
![Hardware settings](assets/screenshots/04-hardware-settings.png)
![Model training workflow](assets/screenshots/05-model-training.png)

## Build From Source

OpenDSS is a Windows C++/Qt project built with CMake.

### Requirements

- CMake 3.19 or newer
- Visual Studio with the MSVC x64 toolchain
- Qt 6 Widgets
- OpenCV
- ONNX Runtime
- Hamamatsu DCAM SDK
- NI-DAQmx headers and libraries

### Configure And Build

```powershell
cmake -S app/runtime -B build-opendss-release `
  -G "Visual Studio 17 2022" -A x64 `
  -D CMAKE_PREFIX_PATH="C:\Qt\6.10.1\msvc2022_64" `
  -D ONNXRUNTIME_DIR="C:\onnxruntime-gpu" `
  -D NIDAQMX_INCLUDE_DIR="C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\include" `
  -D NIDAQMX_LIBRARY="C:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc\NIDAQmx.lib" `
  -D ENABLE_NIDAQMX=ON `
  -D BUILD_QT_GUI=ON

cmake --build build-opendss-release --config Release --target desktop_app
```

NI trigger support requires `-D ENABLE_NIDAQMX=ON` with valid NI-DAQmx include and library paths.

### Optional Model Training Helpers

The same trainer setup commands are included in the Downloads section. A full instruction site with step-by-step installation, operation, trainer, and troubleshooting guidance is planned separately.

## License

- [LICENSE](LICENSE)
- [Third-Party Notices](THIRD_PARTY_NOTICES.md)
