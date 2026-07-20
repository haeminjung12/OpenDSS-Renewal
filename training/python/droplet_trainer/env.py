from __future__ import annotations

import importlib
import importlib.metadata
import os
import platform
import sys
import tempfile
from pathlib import Path
from typing import Any

from . import __version__


REQUIRED_TRAINING_PACKAGES = ["torch", "torchvision", "numpy", "PIL", "sklearn", "pandas"]
REQUIRED_ONNX_PACKAGES = ["onnx", "onnxruntime", "onnxscript"]


def _ort_distributions() -> dict[str, str]:
    installed: dict[str, str] = {}
    for name in ("onnxruntime", "onnxruntime-gpu"):
        try:
            installed[name] = importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError:
            pass
    return installed


def _import_status(module_name: str) -> dict[str, Any]:
    try:
        module = importlib.import_module(module_name)
    except Exception as exc:
        return {"status": "missing", "version": None, "error": str(exc)}
    version = getattr(module, "__version__", None)
    return {"status": "ok", "version": str(version) if version is not None else None}


def check_output_writable(path: str | None) -> tuple[bool | None, str | None]:
    if not path:
        return None, None
    output = Path(path).expanduser().resolve()
    try:
        output.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(prefix=".droplet_trainer_write_", dir=str(output), delete=False) as handle:
            temp_path = handle.name
            handle.write(b"ok")
        os.unlink(temp_path)
        return True, None
    except Exception as exc:
        return False, str(exc)


def run_env_check(args: Any) -> tuple[dict[str, Any], int]:
    required: list[str] = []
    if args.require_training:
        required.extend(REQUIRED_TRAINING_PACKAGES)
    if args.require_onnx:
        required.extend(REQUIRED_ONNX_PACKAGES)
    reported = list(dict.fromkeys(REQUIRED_TRAINING_PACKAGES + REQUIRED_ONNX_PACKAGES))
    if not required:
        required = list(dict.fromkeys(REQUIRED_TRAINING_PACKAGES + REQUIRED_ONNX_PACKAGES))

    packages = {module_name: _import_status(module_name) for module_name in reported}
    missing = [name for name in required if packages[name]["status"] != "ok"]

    torch_status = packages.get("torch") if "torch" in packages else _import_status("torch")
    cuda_available = False
    cuda_version = None
    gpu_names: list[str] = []
    if torch_status["status"] == "ok":
        import torch

        cuda_available = bool(torch.cuda.is_available())
        cuda_version = getattr(torch.version, "cuda", None)
        if cuda_available:
            gpu_names = [torch.cuda.get_device_name(index) for index in range(torch.cuda.device_count())]

    ort_status = packages.get("onnxruntime") if "onnxruntime" in packages else _import_status("onnxruntime")
    providers: list[str] = []
    ort_distributions = _ort_distributions()
    if ort_status["status"] == "ok":
        import onnxruntime

        providers = list(onnxruntime.get_available_providers())

    warnings: list[dict[str, Any]] = []
    selected = "cuda" if args.device == "auto" and cuda_available else args.device
    cuda_provider_available = "CUDAExecutionProvider" in providers
    if selected == "auto":
        selected = "cpu"
    exit_code = 0
    status = "ok"
    error = None
    if missing:
        status = "error"
        exit_code = 11
        error = {
            "code": "MISSING_REQUIRED_PACKAGE",
            "message": "One or more required packages are not importable.",
            "details": {"packages": missing},
        }
    elif args.device == "cuda" and args.require_onnx and (not cuda_available or not cuda_provider_available):
        status = "error"
        exit_code = 12
        selected = "unavailable"
        error = {
            "code": "CUDA_PROVIDER_UNAVAILABLE",
            "message": "CUDA was requested but the training environment cannot create CUDA ONNX Runtime sessions.",
            "details": {
                "torch_cuda_available": cuda_available,
                "onnxruntime_providers": providers,
                "installed_onnxruntime_distributions": ort_distributions,
            },
        }
    elif args.device == "cuda" and (args.require_training or not args.require_onnx) and not cuda_available:
        selected = "cpu"
        warnings.append({
            "code": "REQUESTED_DEVICE_FALLBACK_CPU",
            "message": "CUDA training was requested but PyTorch CUDA is unavailable; training will use CPU.",
        })
    elif args.device == "auto" and args.require_onnx and cuda_available and not cuda_provider_available:
        selected = "cpu"
        warnings.append({
            "code": "ONNX_CUDA_PROVIDER_FALLBACK_CPU",
            "message": "CUDA is available to PyTorch, but ONNX Runtime lacks CUDAExecutionProvider; ONNX inference will use CPU.",
        })
    writable, writable_error = check_output_writable(args.check_output)
    if writable is False:
        status = "error"
        exit_code = 30
        error = {
            "code": "OUTPUT_NOT_WRITABLE",
            "message": "Requested output path is not writable.",
            "details": {"path": str(Path(args.check_output).resolve()), "error": writable_error},
        }

    supported = (3, 10) <= sys.version_info[:2] <= (3, 12)
    if not supported and not args.allow_untested_python:
        status = "error"
        exit_code = 10
        error = {
            "code": "UNSUPPORTED_PYTHON_VERSION",
            "message": "Python version is outside the supported 3.10-3.12 range.",
            "details": {"version": platform.python_version()},
        }
    elif not supported:
        warnings.append({"code": "UNTESTED_PYTHON_VERSION", "message": "Python version is outside tested range."})

    payload = {
        "schema_version": 1,
        "command": "env-check",
        "status": status,
        "backend_version": __version__,
        "python": {
            "executable": sys.executable,
            "version": platform.python_version(),
            "supported": supported,
        },
        "platform": {"system": platform.system(), "release": platform.release(), "machine": platform.machine()},
        "packages": packages,
        "devices": {
            "selected": selected,
            "cuda_available": cuda_available,
            "cuda_version": cuda_version,
            "gpu_names": gpu_names,
            "onnxruntime_providers": providers,
            "onnxruntime_distributions": ort_distributions,
            "onnxruntime_selected_provider": (
                "CUDAExecutionProvider" if selected == "cuda" and cuda_provider_available else "CPUExecutionProvider"
            ),
        },
        "checks": {"output_writable": writable},
        "warnings": warnings,
    }
    if error:
        payload["error"] = error
    return payload, exit_code
