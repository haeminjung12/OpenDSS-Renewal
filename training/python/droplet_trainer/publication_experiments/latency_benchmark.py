from __future__ import annotations

import argparse
import ctypes
import json
import os
import platform
import statistics
import time
from pathlib import Path
from typing import Any

import numpy as np

from ..train import _build_model, _export_onnx
from .screen import sha256_file


def latency_statistics(samples_ms: list[float]) -> dict[str, float]:
    if not samples_ms:
        raise ValueError("latency samples cannot be empty")
    values = np.asarray(samples_ms, dtype=np.float64)
    return {
        "iterations": int(values.size),
        "median_ms": float(np.median(values)),
        "p95_ms": float(np.percentile(values, 95)),
        "mean_ms": float(values.mean()),
        "std_ms": float(values.std()),
        "throughput_per_second": float(1000.0 / values.mean()),
    }


def _sync(provider: str) -> None:
    if provider == "CUDAExecutionProvider":
        import torch
        torch.cuda.synchronize()


def _measure(action, provider: str, warmup: int = 20, iterations: int = 200) -> dict[str, float]:
    for _ in range(warmup):
        action()
    _sync(provider)
    samples = []
    for _ in range(iterations):
        _sync(provider)
        started = time.perf_counter_ns()
        action()
        _sync(provider)
        samples.append((time.perf_counter_ns() - started) / 1_000_000.0)
    result = latency_statistics(samples)
    result.update({"warmup_iterations": warmup, "synchronization": "torch.cuda.synchronize before/after ORT run" if provider == "CUDAExecutionProvider" else "synchronous ORT Run return"})
    return result


def _image_inputs(manifest_path: Path, count: int = 32) -> tuple[list[Path], Any]:
    from PIL import Image
    from torchvision import transforms
    doc = json.loads(manifest_path.read_text(encoding="utf-8"))
    records = doc.get("records") or doc.get("items") or []
    selected = [Path(row["source_path"]) for row in records if row.get("role") == "validation"][:count]
    if not selected:
        selected = [Path(row["source_path"]) for row in records][:count]
    transform = transforms.Compose([transforms.Resize((96, 96)), transforms.ToTensor(), transforms.Normalize([.485, .456, .406], [.229, .224, .225])])
    return selected, lambda path: transform(Image.open(path).convert("RGB")).unsqueeze(0).numpy().astype(np.float32)


def _windows_memory_info() -> dict[str, int]:
    class Counters(ctypes.Structure):
        _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong),
                    ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]
    counters = Counters()
    counters.cb = ctypes.sizeof(counters)
    get_process = ctypes.windll.kernel32.GetCurrentProcess
    get_process.restype = ctypes.c_void_p
    memory_info = ctypes.windll.psapi.GetProcessMemoryInfo
    memory_info.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong]
    memory_info.restype = ctypes.c_int
    if not memory_info(get_process(), ctypes.byref(counters), counters.cb):
        raise OSError("GetProcessMemoryInfo failed")
    return {"working_set_bytes": int(counters.WorkingSetSize), "peak_working_set_bytes": int(counters.PeakWorkingSetSize)}


def _export_reference(checkpoint: Path, output: Path) -> dict[str, Any]:
    import torch
    payload = torch.load(checkpoint, map_location="cpu", weights_only=False)
    config = payload["config"]
    model = _build_model(config, 3)
    model.load_state_dict(payload["model_state"], strict=True)
    output.mkdir(parents=True, exist_ok=False)
    onnx_path, opset = _export_onnx(model, output, config)
    disposition = {
        "reason": "Latency-compatible ONNX produced from the preserved accepted Phase-3B alpha=0.65 fold-0 best checkpoint; no retraining performed.",
        "checkpoint": str(checkpoint), "checkpoint_sha256": sha256_file(checkpoint),
        "onnx": str(onnx_path), "onnx_sha256": sha256_file(onnx_path), "opset": opset,
    }
    (output / "squeezenet_reference_export_disposition.json").write_text(json.dumps(disposition, indent=2), encoding="utf-8")
    return disposition


def _load_torch(checkpoint: Path):
    import torch
    payload = torch.load(checkpoint, map_location="cpu", weights_only=False)
    model = _build_model(payload["config"], 3)
    model.load_state_dict(payload["model_state"], strict=True)
    return model.eval(), payload["config"]


def _benchmark_model(name: str, checkpoint: Path, onnx_path: Path, manifest: Path) -> dict[str, Any]:
    import onnxruntime as ort
    import torch
    model, config = _load_torch(checkpoint)
    paths, preprocess = _image_inputs(manifest)
    tensors = [preprocess(path) for path in paths]
    fixed = tensors[0]
    with torch.no_grad():
        expected = model(torch.from_numpy(fixed)).numpy()
    model_bytes = onnx_path.stat().st_size + sum(p.stat().st_size for p in onnx_path.parent.glob(onnx_path.name + ".data"))
    result: dict[str, Any] = {
        "architecture": name, "checkpoint": str(checkpoint), "checkpoint_sha256": sha256_file(checkpoint),
        "onnx": str(onnx_path), "onnx_sha256": sha256_file(onnx_path), "onnx_size_bytes": model_bytes,
        "parameters": sum(p.numel() for p in model.parameters()), "input_shape": list(fixed.shape), "output_shape": list(expected.shape),
        "class_order": config.get("classes"), "providers": {},
    }
    available = ort.get_available_providers()
    for provider in ("CPUExecutionProvider", "CUDAExecutionProvider"):
        if provider not in available:
            result["providers"][provider] = {"available": False}
            continue
        session = ort.InferenceSession(str(onnx_path), providers=[provider])
        actual = session.run(None, {session.get_inputs()[0].name: fixed})[0]
        before = _windows_memory_info()
        inference = _measure(lambda: session.run(None, {session.get_inputs()[0].name: fixed}), provider)
        index = 0
        preprocessing = _measure(lambda: preprocess(paths[index]), "CPUExecutionProvider")
        def end_to_end():
            nonlocal index
            array = preprocess(paths[index % len(paths)])
            index += 1
            session.run(None, {session.get_inputs()[0].name: array})
        e2e = _measure(end_to_end, provider)
        after = _windows_memory_info()
        result["providers"][provider] = {
            "available": True, "session_providers": session.get_providers(), "preprocessing": preprocessing,
            "inference_only": inference, "end_to_end": e2e,
            "host_memory_before": before, "host_memory_after": after,
            "host_working_set_observed_delta_bytes": max(0, after["working_set_bytes"]-before["working_set_bytes"]),
            "gpu_peak_memory_bytes": None,
            "gpu_memory_note": "ONNX Runtime CUDA allocations are not tracked by torch; no reliable per-session peak API is exposed by this runtime.",
            "parity": {"max_abs_error": float(np.max(np.abs(expected-actual))), "mean_abs_error": float(np.mean(np.abs(expected-actual))), "finite": bool(np.isfinite(actual).all())},
        }
    return result


def execute(execution: Path, phase3b_ledger: Path) -> dict[str, Any]:
    import onnxruntime as ort
    import torch
    ledger = json.loads((execution / "run_ledger.json").read_text(encoding="utf-8"))
    phase3b = json.loads(phase3b_ledger.read_text(encoding="utf-8"))
    reference = next(row for row in phase3b["runs"] if row["run_id"] == "sampler_alpha_0.65__seed1729__fold0")
    deployment = execution / "deployment-benchmark"
    deployment.mkdir(exist_ok=False)
    squeeze_checkpoint = Path(reference["trainer_dir"]) / "checkpoints" / "best.pth"
    squeeze = _export_reference(squeeze_checkpoint, deployment / "squeezenet_reference")
    selected = []
    for architecture in ("mobilenet_v3_small", "efficientnet_b0"):
        candidates = [row for row in ledger["runs"] if row["architecture"] == architecture]
        selected.append(max(candidates, key=lambda row: (row["balanced_accuracy"], row["macro_f1"])))
    first_manifest = Path(ledger["runs"][0]["trainer_dir"]).parents[1] / "dataset" / "metadata" / "dataset_manifest.json"
    models = [_benchmark_model("squeezenet1_1_alpha_0.65_reference", squeeze_checkpoint, Path(squeeze["onnx"]), first_manifest)]
    for row in selected:
        trainer = Path(row["trainer_dir"])
        manifest = trainer.parents[1] / "dataset" / "metadata" / "dataset_manifest.json"
        models.append(_benchmark_model(row["architecture"], trainer / "checkpoints" / "best.pth", trainer / "model.onnx", manifest))
    result = {
        "protocol": {"batch": 1, "input": "96x96 RGB ImageNet-normalized", "fixed_warmed_input_count": 32, "warmup": 20, "iterations": 200,
                     "selection": "highest balanced accuracy fold per architecture; accepted SqueezeNet alpha=0.65 fold0 reference"},
        "environment": {"platform": platform.platform(), "python": platform.python_version(), "torch": torch.__version__, "cuda": torch.version.cuda,
                        "gpu": torch.cuda.get_device_name(0) if torch.cuda.is_available() else None, "onnxruntime": ort.__version__, "available_providers": ort.get_available_providers()},
        "models": models,
    }
    (deployment / "latency_results.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execution", type=Path, required=True)
    parser.add_argument("--phase3b-ledger", type=Path, required=True)
    args = parser.parse_args(argv)
    print(json.dumps(execute(args.execution.resolve(), args.phase3b_ledger.resolve()), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
