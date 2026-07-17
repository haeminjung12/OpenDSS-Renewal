# Windows Training Environment

This package is the external Python trainer backend for OpenDSS. The C++/Qt app does not bundle Python, PyTorch, CUDA, MATLAB, or a prebuilt virtual environment.

Supported internal-release setup:

- Python 3.12 x64 on Windows.
- User-managed `venv`.
- CPU setup first.
- Optional NVIDIA GPU setup through separate CUDA PyTorch wheel-index scripts.

Recommended CPU setup from `training/python`:

```powershell
.\scripts\windows\create-training-venv.ps1 -Python py -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv"
.\scripts\windows\install-training-cpu.ps1 -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv"
```

The install script:

- installs the local `droplet_trainer` package into the selected `venv`
- runs `env-check`
- updates the OpenDSS app setting to use `%LOCALAPPDATA%\OpenDSS\training-venv\Scripts\python.exe`

Run this again any time you want to re-check the environment without reinstalling:

```powershell
.\scripts\windows\verify-training-env.ps1 -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv" -Device cpu -CheckOutput "$PWD\..\..\outputs\trainer_env_check"
```

Dataset readiness wrappers:

```powershell
.\scripts\windows\inspect-dataset.ps1 -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv" -Dataset "C:\path\to\dataset_manifest.json"
.\scripts\windows\validate-dataset.ps1 -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv" -Dataset "C:\path\to\dataset_manifest.json" -Output "C:\path\to\readiness"
```

Training wrapper:

```powershell
.\scripts\windows\train-model.ps1 -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv" -Dataset "C:\path\to\reviewed_dataset" -Output "C:\path\to\models" -Device cpu
```

The wrappers print the underlying `python -m droplet_trainer ...` command and preserve its process exit code. They do not hide or replace the trainer CLI contract.

Optional GPU setup:

```powershell
.\scripts\windows\install-training-gpu-cu130.ps1 -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv-gpu"
.\scripts\windows\install-training-gpu-cu128.ps1 -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv-gpu"
```

Use only one GPU install script per environment. GPU verification requires `torch.cuda.is_available()` to pass. These scripts install PyTorch CUDA runtime wheels, but they do not install or manage the NVIDIA driver.

If the app was previously pointed at a different Python, you can re-point it manually:

```powershell
.\scripts\windows\set-app-trainer-python.ps1 -VenvPath "$env:LOCALAPPDATA\OpenDSS\training-venv"
```
