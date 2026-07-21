# OpenDSS

OpenDSS is a Windows application for viewing, classifying, and sorting droplets in a laboratory workflow. It brings camera viewing, trained-model testing, and optional sorting hardware controls into one desktop program.

<p align="center">
  <img src="assets/branding/opendss-primary-full-color.svg" alt="OpenDSS logo" width="520">
</p>

## Install OpenDSS

1. Open the [latest OpenDSS release](https://github.com/haeminjung12/OpenDSS_clean/releases/latest).
2. Download `OpenDSSSetup.exe`.
3. Double-click the downloaded file and follow the installer.
4. Open **OpenDSS** from the Start menu.

The installer includes the Microsoft components needed to open OpenDSS. You do not need a camera or sorting device to explore the Models Library or test a model. Model predictions use the computer's processor by default.

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

## Optional Hardware

OpenDSS can open and its Models tools can work without any of the hardware below. Install only what your laboratory uses.

### Camera

Live camera use requires a compatible Hamamatsu camera and Hamamatsu's camera software:

- [DCAM-API for Windows](https://www.hamamatsu.com/jp/en/product/cameras/driver-software/dcam-api-for-windows.html)
- [DCAM-SDK4](https://www.hamamatsu.com/all/en/product/cameras/software/driver-software/dcam-sdk4.html)

### Sorting or trigger output

Sorting output requires compatible National Instruments hardware and NI-DAQmx:

- [Download the official NI-DAQmx installer](https://www.ni.com/en/support/downloads/drivers/download.ni-daq-mx.html?srsltid=AfmBOoripP8sW1nXF0W7AAqlBqDBFPvAkvA-Otli6j6Q3Jcj7YSqsefx#590033)
- [How to install NI-DAQmx](https://download.ni.com/support/manuals/373235aa.pdf)

### Faster training with an NVIDIA GPU

GPU training requires a compatible NVIDIA graphics card and current NVIDIA driver. OpenDSS includes setup scripts under `training\python\scripts\windows` for the supported GPU environments. Use `install-training-gpu-cu130.ps1` or `install-training-gpu-cu128.ps1`, depending on the target computer's driver. CPU training remains available through `install-training-cpu.ps1`.

These scripts install the training tools; they do not change the NVIDIA driver. Packaged model predictions continue to use the qualified CPU path.

## Troubleshooting

- **OpenDSS does not open:** restart Windows after installation, then try again. If it still fails, open **Information > Diagnostics** and record any message shown.
- **Camera or sorting hardware is unavailable:** OpenDSS can still use Models. Install the matching optional driver above before using that hardware.
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
