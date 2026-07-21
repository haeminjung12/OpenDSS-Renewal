# OpenDSS

OpenDSS is a Windows application for viewing, classifying, and sorting droplets in a laboratory workflow. A complete laboratory installation uses a supported Hamamatsu camera and National Instruments DAQ hardware.

<p align="center">
  <img src="assets/branding/opendss-primary-full-color.svg" alt="OpenDSS logo" width="520">
</p>

## Install OpenDSS

1. Open the [latest OpenDSS release](https://github.com/haeminjung12/OpenDSS_clean/releases/latest).
2. Download `OpenDSSSetup.exe`.
3. Double-click the downloaded file and follow the installer.
4. Open **OpenDSS** from the Start menu.

The installer includes the Microsoft Visual C++ components needed to open OpenDSS. Before operating the complete system, install the camera and DAQ prerequisites below, then set up Python if you will train or test models.

## Required Laboratory Hardware And Drivers

These are prerequisites for the complete OpenDSS laboratory workflow. They are not bundled in `OpenDSSSetup.exe`.

### Hamamatsu camera

Connect a supported Hamamatsu camera and install Hamamatsu's DCAM runtime/driver before using Live View:

- [DCAM-API for Windows](https://www.hamamatsu.com/jp/en/product/cameras/driver-software/dcam-api-for-windows.html)
- [DCAM-SDK4](https://www.hamamatsu.com/all/en/product/cameras/software/driver-software/dcam-sdk4.html)

### National Instruments DAQ

Connect supported National Instruments DAQ hardware and install NI-DAQmx before using sorting or trigger output:

- [Download the official NI-DAQmx installer](https://www.ni.com/en/support/downloads/drivers/download.ni-daq-mx.html?srsltid=AfmBOoripP8sW1nXF0W7AAqlBqDBFPvAkvA-Otli6j6Q3Jcj7YSqsefx#590033)
- [How to install NI-DAQmx](https://download.ni.com/support/manuals/373235aa.pdf)

After installing either vendor driver, restart Windows before connecting through OpenDSS.

## Set Up Python For Models

Training and trainer-side model testing require **64-bit Python 3.12.x**. Python and the managed training environment are not inside the installer.

1. Download and install [Python 3.12 for Windows (64-bit)](https://www.python.org/downloads/windows/). Keep the Python Launcher selected. **Add Python to PATH is not required** because the setup commands use the `py` launcher.
2. Open Windows PowerShell.
3. Go to the training tools installed with OpenDSS:

```powershell
Set-Location "$env:ProgramFiles\OpenDSS\training\python"
```

4. Choose one environment:

CPU works on every supported computer:

```powershell
.\scripts\windows\create-training-venv.ps1 -Python py -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv"
.\scripts\windows\install-training-cpu.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv"
```

GPU training requires a compatible NVIDIA GPU and a current [NVIDIA driver](https://www.nvidia.com/Download/index.aspx). The current packaged GPU environment uses CUDA 13.0 PyTorch wheels:

```powershell
.\scripts\windows\create-training-venv.ps1 -Python py -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv-gpu"
.\scripts\windows\install-training-gpu-cu130.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv-gpu"
```

The package also contains `install-training-gpu-cu128.ps1` for computers that have been qualified for that CUDA 12.8 environment. Do not run both GPU installers in the same environment.

5. Verify the environment. Use `cpu` and `training-venv` for CPU, or `cuda` and `training-venv-gpu` for GPU:

```powershell
.\scripts\windows\verify-training-env.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv-gpu" -Device cuda -CheckOutput "$env:LOCALAPPDATA\OpenVisualDropletSorter\trainer_env_check"
```

The command must end with `Training environment verification passed.` The managed Python executables are:

- CPU: `%LOCALAPPDATA%\OpenVisualDropletSorter\training-venv\Scripts\python.exe`
- GPU: `%LOCALAPPDATA%\OpenVisualDropletSorter\training-venv-gpu\Scripts\python.exe`

The install scripts register the selected Python with OpenDSS. In **Models > Train**, choose CPU for the CPU environment or GPU for the CUDA environment. Packaged live model inference remains on the qualified CPU path.

## Start With Models

Open **Models** from the left side of the application.

- **MobileNet — Faster** is the best starting choice when speed matters most.
- **EfficientNet — More Accurate** is the best starting choice when accuracy matters most.
- A **Blank** model starts with general-purpose ImageNet knowledge and must be trained before it can be used.
- A **Pre-trained** model already contains OpenDSS training and can be used or trained further.

To train a model:

1. Choose a prepared dataset in **Models > Train**.
2. Select a starting model and choose CPU or GPU.
3. Start training and wait for it to finish.
4. Select **Save/Use**, then choose whether to make the new model active.
5. Restart OpenDSS and open **Models > Test** to test it.

Datasets are provided separately and are not inside the installer. A completed model contains `metadata.json`, `checkpoint.pth`, and `model.onnx` in one folder.

## Diagnostic Use Without Hardware

OpenDSS can be launched without connected laboratory hardware to inspect the interface, open diagnostics, and work with already packaged models. This no-hardware diagnostic mode does not replace the camera, DCAM, NI DAQ, or NI-DAQmx prerequisites for operating the complete droplet-sorting system.

## Troubleshooting

- **OpenDSS does not open:** restart Windows after installation, then try again. If it still fails, open **Information > Diagnostics** and record any message shown.
- **Camera or sorting hardware is unavailable:** use the no-hardware diagnostic mode only. Install and connect all required laboratory hardware and drivers before operating the full system.
- **GPU is unavailable in Train:** update the NVIDIA driver and run one bundled GPU setup script. Choose CPU to continue without GPU setup.
- **A model will not load:** confirm its folder contains all three model files. Open **Information > Diagnostics** for a short cause and expand **Detailed Log** for technical details.
- **Training is rejected:** review the class labels and balance in the selected dataset. OpenDSS rejects a model that learns to predict only one class instead of silently saving an unusable result.

## Screenshots

![Live view overview](assets/screenshots/01-live-view-overview.png)
![Hardware settings](assets/screenshots/04-hardware-settings.png)
![Model training workflow](assets/screenshots/05-model-training.png)

## For Developers

End users do not need the tools in this section. OpenDSS is a Windows C++/Qt project built with CMake.

### Source-build requirements

- CMake 3.19 or newer
- Visual Studio with the MSVC x64 toolchain
- [Qt](https://www.qt.io/development/download-open-source) 6 Widgets
- OpenCV
- [ONNX Runtime](https://onnxruntime.ai/docs/install/)
- Hamamatsu DCAM SDK and NI-DAQmx development files when building those hardware features

### Configure and build

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

NI trigger support requires `ENABLE_NIDAQMX=ON` and valid NI-DAQmx include and library paths.

## License

- [LICENSE](LICENSE)
- [Third-party notices](THIRD_PARTY_NOTICES.md)
