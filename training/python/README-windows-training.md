# Windows Training Environment

This package is the external Python trainer backend for OpenDSS. The C++/Qt app does not bundle Python, PyTorch, CUDA, MATLAB, or a prebuilt virtual environment.

Supported internal-release setup:

- Python 3.12 x64 on Windows.
- User-managed `venv`.
- CPU setup for the broadest compatibility, or a separate optional CUDA environment.
- Optional NVIDIA GPU setup through separate CUDA PyTorch wheel-index scripts.

If Python is not installed, download [Python 3.12 for Windows (64-bit)](https://www.python.org/downloads/windows/) and keep the Python Launcher selected. Adding Python to PATH is not required because these commands use `py -3.12`.

For an installed copy of OpenDSS, first open PowerShell and run:

```powershell
Set-Location "$env:ProgramFiles\OpenDSS\training\python"
```

Recommended CPU setup from `training/python`:

```powershell
.\scripts\windows\create-training-venv.ps1 -Python py -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv"
.\scripts\windows\install-training-cpu.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv"
```

The install script:

- installs the local `droplet_trainer` package into the selected `venv`
- runs `env-check`
- updates the OpenDSS app setting to use the selected environment's `Scripts\python.exe`

Run this again any time you want to re-check the environment without reinstalling:

```powershell
.\scripts\windows\verify-training-env.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -Device cpu -CheckOutput "$PWD\..\..\outputs\trainer_env_check"
```

Dataset readiness wrappers:

```powershell
.\scripts\windows\inspect-dataset.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -Dataset "C:\path\to\dataset_manifest.json"
.\scripts\windows\validate-dataset.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -Dataset "C:\path\to\dataset_manifest.json" -Output "C:\path\to\readiness"
```

Training wrapper:

```powershell
.\scripts\windows\train-model.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -Dataset "C:\path\to\reviewed_dataset" -Output "C:\path\to\models" -Device cpu
```

The wrappers print the underlying `python -m droplet_trainer ...` command and preserve its process exit code. They do not hide or replace the trainer CLI contract.

GPU setup (requires a compatible NVIDIA GPU and current NVIDIA driver):

```powershell
.\scripts\windows\create-training-venv.ps1 -Python py -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv-gpu"
.\scripts\windows\install-training-gpu-cu130.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv-gpu"
# Or, in a separate environment, use install-training-gpu-cu128.ps1.
```

Use only one GPU install script per environment. GPU verification requires `torch.cuda.is_available()` to pass. These scripts install pinned PyTorch CUDA runtime wheels, but they do not install or manage the NVIDIA driver. OpenDSS production ONNX inference remains on the qualified CPU provider; this CUDA environment accelerates training and trainer-side model validation.

Verify GPU setup with:

```powershell
.\scripts\windows\verify-training-env.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv-gpu" -Device cuda -CheckOutput "$env:LOCALAPPDATA\OpenVisualDropletSorter\trainer_env_check"
```

The final line must say `Training environment verification passed.` The expected Python executables are `%LOCALAPPDATA%\OpenVisualDropletSorter\training-venv\Scripts\python.exe` for CPU and `%LOCALAPPDATA%\OpenVisualDropletSorter\training-venv-gpu\Scripts\python.exe` for GPU.

If the app was previously pointed at a different Python, you can re-point it manually:

```powershell
.\scripts\windows\set-app-trainer-python.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv"
```

## Models workspace contract

- Dataset metadata supplies the ordered two- or three-class schema.
- Blank MobileNetV3-Small and EfficientNet-B0 starters load bundled ImageNet weights and become activatable only after successful training.
- Pre-trained packages include a PyTorch checkpoint for continuation and an ONNX model for inference/Test. Continuation requires the checkpoint output count to match the selected dataset.
- Successful Train output is a self-contained package with `metadata.json`, `checkpoint.pth`, and `model.onnx`; Save/Use registers it and optionally activates it.
- A `MODEL_COLLAPSE_DETECTED` result means the training process ran, but the result omitted one or more classes or otherwise failed the scientific promotion gate. Check dataset labels, imbalance, and per-class metrics.
