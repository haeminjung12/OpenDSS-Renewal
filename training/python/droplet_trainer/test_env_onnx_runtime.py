from __future__ import annotations

from argparse import Namespace

import droplet_trainer.env as env


def _args(device: str) -> Namespace:
    return Namespace(
        device=device,
        require_training=False,
        require_onnx=True,
        check_output=None,
        allow_untested_python=True,
    )


def test_explicit_cuda_fails_without_cuda_execution_provider(monkeypatch):
    monkeypatch.setattr(env, "_ort_distributions", lambda: {"onnxruntime": "1.25.1"})
    monkeypatch.setattr(env, "_import_status", lambda name: {"status": "ok", "version": "test"})
    import onnxruntime
    import torch
    monkeypatch.setattr(onnxruntime, "get_available_providers", lambda: ["CPUExecutionProvider"])
    monkeypatch.setattr(torch.cuda, "is_available", lambda: True)
    monkeypatch.setattr(torch.cuda, "device_count", lambda: 1)
    monkeypatch.setattr(torch.cuda, "get_device_name", lambda index: "test gpu")
    payload, exit_code = env.run_env_check(_args("cuda"))
    assert exit_code == 12
    assert payload["error"]["code"] == "CUDA_PROVIDER_UNAVAILABLE"
    assert payload["devices"]["selected"] == "unavailable"


def test_auto_warns_and_falls_back_when_ort_cuda_provider_is_missing(monkeypatch):
    monkeypatch.setattr(env, "_ort_distributions", lambda: {"onnxruntime": "1.25.1"})
    monkeypatch.setattr(env, "_import_status", lambda name: {"status": "ok", "version": "test"})
    import onnxruntime
    import torch
    monkeypatch.setattr(onnxruntime, "get_available_providers", lambda: ["CPUExecutionProvider"])
    monkeypatch.setattr(torch.cuda, "is_available", lambda: True)
    monkeypatch.setattr(torch.cuda, "device_count", lambda: 1)
    monkeypatch.setattr(torch.cuda, "get_device_name", lambda index: "test gpu")
    payload, exit_code = env.run_env_check(_args("auto"))
    assert exit_code == 0
    assert payload["devices"]["selected"] == "cpu"
    assert payload["warnings"][0]["code"] == "ONNX_CUDA_PROVIDER_FALLBACK_CPU"
