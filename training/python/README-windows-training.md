# Windows Training Runtime

OpenDSS provisions one installer-owned training environment at:

```text
%LOCALAPPDATA%\OpenDSS\training-venv-gpu
```

The environment is Python 3.12.10 x64 with the exact 37-distribution lock in
`requirements/windows-py312-gpu-cu130.lock`. Its anchors are PyTorch
2.10.0+cu130, torchvision 0.25.0+cu130, ONNX Runtime GPU 1.25.1, and
droplet-trainer 0.2.0. It is used for both automatic GPU execution and factual
CPU fallback; OpenDSS does not install a duplicate CPU environment.

No system Python, source checkout, user site, `PATH`, or `PYTHONPATH`
participates in production training. The app invokes the installed package as
`python -I -m droplet_trainer`. Network access is used only by the installer
bootstrap; production training has no network fallback.

## Installer bootstrap

The installer embeds only the repository-owned, hash-locked
`droplet_trainer-0.2.0-py3-none-any.whl`. It downloads CPython and the 36
third-party wheels from the exact official HTTPS URLs in:

`requirements/windows-py312-gpu-cu130-downloads.json`

The catalog contains filenames and URLs only. SHA-256 values remain solely in
`windows-py312-gpu-cu130.lock`. The bootstrap downloads into a scoped temporary
wheelhouse, verifies all 37 wheels against that lock, then installs with
`--no-index --find-links --require-hashes`. It validates the exact distribution
inventory and sole ONNX Runtime variant, and runs:

```text
python -I -m droplet_trainer env-check --device auto --require-training --require-onnx --json
```

Candidate runtime/environment directories are verified before publication.
Download, hash, install, or environment-check failure removes the candidate and
preserves the previous accepted installation.

## Hardware behavior

A compatible NVIDIA GPU and current external NVIDIA driver are used
automatically. When CUDA is unavailable, the same environment runs on CPU; GPU
absence is not a setup error. DCAM and NI-DAQmx remain separate external
laboratory prerequisites.

## Model package contract

- Dataset metadata supplies the ordered two- or three-class schema.
- Faster and More Accurate use the qualified fixed configurations.
- Successful output is a self-contained Model Package with `metadata.json`,
  `checkpoint.pth`, and `model.onnx`.
- Save/Use registers the package and may activate it.
