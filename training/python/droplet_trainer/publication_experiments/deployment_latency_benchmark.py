from __future__ import annotations

import argparse
import json
import statistics
import time
from pathlib import Path

import numpy as np
import onnxruntime as ort


def _stats(values: list[float]) -> dict[str, float]:
    data = np.asarray(values, dtype=np.float64)
    return {"median_ms": float(np.median(data)), "p95_ms": float(np.percentile(data, 95)),
            "mean_ms": float(data.mean()), "std_ms": float(data.std()),
            "throughput_per_second": float(1000.0 / data.mean())}


def _measure(action, warmups: int, iterations: int) -> dict[str, float]:
    for _ in range(warmups):
        action()
    samples = []
    for _ in range(iterations):
        started = time.perf_counter_ns()
        action()
        samples.append((time.perf_counter_ns() - started) / 1e6)
    return {**_stats(samples), "warmups": warmups, "iterations": iterations}


def run(models_root: Path, output: Path, warmups: int = 20, iterations: int = 200) -> dict:
    rng = np.random.default_rng(1729)
    images = rng.integers(0, 256, size=(32, 96, 96, 3), dtype=np.uint8)
    mean = np.asarray([.485, .456, .406], dtype=np.float32)[:, None, None]
    std = np.asarray([.229, .224, .225], dtype=np.float32)[:, None, None]
    def preprocess(index: int) -> np.ndarray:
        return ((images[index].transpose(2, 0, 1).astype(np.float32) / 255.0 - mean) / std)[None]
    result = {"runtime": ort.__version__, "available_providers": ort.get_available_providers(),
              "protocol": {"batch": 1, "input": "96x96 RGB ImageNet-normalized", "warmups": warmups,
                           "iterations": iterations, "seed": 1729,
                           "synchronization": "Ort::Run/session.run returns host output and is synchronous for measured output"},
              "models": []}
    for architecture in ("mobilenet_v3_small", "efficientnet_b0"):
        model_path = models_root / architecture / "model.onnx"
        row = {"architecture": architecture, "providers": {}}
        cpu_output = None
        for provider in ("CPUExecutionProvider", "CUDAExecutionProvider"):
            session = ort.InferenceSession(str(model_path), providers=[provider])
            name = session.get_inputs()[0].name
            fixed = preprocess(0)
            output_value = session.run(None, {name: fixed})[0]
            if provider == "CPUExecutionProvider":
                cpu_output = output_value
            index = 0
            def infer(): session.run(None, {name: fixed})
            def prep(): preprocess(0)
            def end_to_end():
                nonlocal index
                value = preprocess(index % len(images)); index += 1
                session.run(None, {name: value})
            row["providers"][provider] = {"session_providers": session.get_providers(),
                "preprocessing": _measure(prep, warmups, iterations),
                "inference_only": _measure(infer, warmups, iterations),
                "end_to_end": _measure(end_to_end, warmups, iterations),
                "finite": bool(np.isfinite(output_value).all()), "output_shape": list(output_value.shape),
                "cpu_cuda_max_abs_difference": None if cpu_output is None else float(np.max(np.abs(cpu_output-output_value)))}
        result["models"].append(row)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--models-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.models_root, args.output), indent=2))
