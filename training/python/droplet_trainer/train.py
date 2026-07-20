from __future__ import annotations

import copy
import csv
import hashlib
import json
import os
import platform
import random
import sys
import time
from collections import Counter
from contextlib import nullcontext
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from . import __version__
from .dataset import apply_waste_source_mode, assign_splits, class_balance_warnings, parse_split, scan_dataset, schema_payload, sha256_file, source_warnings, split_support_errors, utc_now, write_csv
from .errors import CliError, EXIT_MISSING_PACKAGE, EXIT_ONNX_EXPORT_FAILED, EXIT_OUTPUT_INVALID, EXIT_SCHEMA_MISMATCH, EXIT_TRAINING_FAILED
from .metadata import sorting_policy_for_classes
from .schema import ClassSchema, compute_inverse_count_weights


WSL_SOURCE_LINEAGE = {
    "project": r"\\wsl.localhost\Ubuntu\home\haeminjung\CNN_Droplet_Sorting",
    "primary_scripts": [
        "python_env/finetune_squeezenet_new_condition.py",
        "python_env/train_best_sweep.py",
        "python_env/train_droplet.py",
    ],
}

MIN_ONNX_OPSET = 18


DEFAULT_CONFIG: dict[str, Any] = {
    "schema_version": 2,
    "architecture": "mobilenet_v3_small",
    "input_size": [96, 96, 3],
    "normalization": {
        "mean": [0.485, 0.456, 0.406],
        "std": [0.229, 0.224, 0.225],
    },
    "split": {"train": 0.70, "val": 0.15, "test": 0.15},
    "split_mode": "auto",
    "seed": 1729,
    "batch_size": 64,
    "num_workers": 0,
    "epochs": 35,
    "stages": [
        {"name": "head_and_late_blocks", "epochs": 20, "learning_rate": 0.0001, "trainable": "classifier_and_last_blocks"},
        {"name": "controlled_fine_tune", "epochs": 15, "learning_rate": 0.00001, "trainable": "controlled_fine_tune"},
    ],
    "weight_decay": 0.0001,
    "patience": 5,
    "min_delta": 0.000001,
    "use_amp": True,
    "classifier_output": "signed_logits",
    "optimizer": {"name": "adam"},
    "scheduler": {"name": "reduce_on_plateau", "factor": 0.5, "patience": 2, "min_lr": 1e-7},
    "augmentation": {"random_resized_crop": True, "affine": True, "color_jitter": True, "horizontal_flip": False},
    "imbalance": {
        "mode": "balanced_sampler",
        "sampler_alpha": 0.65,
        "class_weight_formula": "none",
        "computed_from": "training_split_only",
        "class_weights": {},
    },
    "initialization": {"mode": "imagenet"},
    "source_model_path": None,
    "export_onnx": True,
    "onnx_opset": MIN_ONNX_OPSET,
    "optional_logging": {"wandb": False},
}

SUPPORTED_IMBALANCE_MODES = {
    "none", "class_weighted_loss", "effective_number", "balanced_sampler",
    "balanced_sampler_effective_number", "focal_loss",
}


class StageEarlyStopping:
    """Stage-local patience tracker; instantiate once per stage."""
    def __init__(self, patience: int, min_delta: float) -> None:
        self.patience = patience
        self.min_delta = min_delta
        self.best = -1.0
        self.best_epoch = 0
        self.stale = 0

    def update(self, metric: float, epoch: int) -> tuple[bool, bool]:
        improved = metric > self.best + self.min_delta
        if improved:
            self.best, self.best_epoch, self.stale = metric, epoch, 0
        else:
            self.stale += 1
        return improved, self.stale >= self.patience


def _validate_training_config(config: dict[str, Any]) -> None:
    if int(config.get("batch_size", 0)) <= 0 or int(config.get("num_workers", -1)) < 0:
        raise CliError("INVALID_TRAINING_CONFIG", "Batch size must be positive and worker count non-negative.", EXIT_TRAINING_FAILED, {})
    if int(config.get("patience", 0)) <= 0 or float(config.get("min_delta", 0)) < 0:
        raise CliError("INVALID_TRAINING_CONFIG", "Early-stopping patience must be positive and min_delta non-negative.", EXIT_TRAINING_FAILED, {})
    if config.get("classifier_output") not in {"signed_logits", "legacy_terminal_relu"}:
        raise CliError("INVALID_TRAINING_CONFIG", "Classifier output must be signed_logits or legacy_terminal_relu.", EXIT_TRAINING_FAILED, {})
    initialization = config.get("initialization", {})
    if not isinstance(initialization, dict) or initialization.get("mode") not in {"imagenet", "checkpoint"}:
        raise CliError("INVALID_TRAINING_CONFIG", "Initialization mode must be imagenet or checkpoint.", EXIT_TRAINING_FAILED, {})
    if initialization.get("mode") == "checkpoint" and not str(initialization.get("checkpoint_path", "")).strip():
        raise CliError("INVALID_TRAINING_CONFIG", "Checkpoint initialization requires checkpoint_path.", EXIT_TRAINING_FAILED, {})
    stages = config.get("stages")
    if not isinstance(stages, list) or not stages:
        raise CliError("INVALID_TRAINING_CONFIG", "At least one training stage is required.", EXIT_TRAINING_FAILED, {})
    names: set[str] = set()
    for stage in stages:
        name = str(stage.get("name", "")).strip()
        if not name or name in names or int(stage.get("epochs", 0)) <= 0 or float(stage.get("learning_rate", 0)) <= 0:
            raise CliError("INVALID_TRAINING_CONFIG", "Each stage needs a unique name, positive epochs, and positive learning rate.", EXIT_TRAINING_FAILED, {"stage": stage})
        names.add(name)
    imbalance = config.get("imbalance", {})
    if imbalance.get("mode") not in SUPPORTED_IMBALANCE_MODES:
        raise CliError("INVALID_TRAINING_CONFIG", "Unsupported imbalance mode.", EXIT_TRAINING_FAILED, {"mode": imbalance.get("mode")})
    sampler_alpha = float(imbalance.get("sampler_alpha", 1.0))
    if not 0.0 <= sampler_alpha <= 1.0:
        raise CliError("INVALID_TRAINING_CONFIG", "Sampler exponent must be between 0 and 1.", EXIT_TRAINING_FAILED, {"sampler_alpha": sampler_alpha})
    if config.get("optimizer", {}).get("name") not in {"adam", "adamw", "sgd"}:
        raise CliError("INVALID_TRAINING_CONFIG", "Unsupported optimizer.", EXIT_TRAINING_FAILED, {})
    if config.get("scheduler", {}).get("name") not in {"none", "step", "reduce_on_plateau"}:
        raise CliError("INVALID_TRAINING_CONFIG", "Unsupported scheduler.", EXIT_TRAINING_FAILED, {})
    if config.get("split_mode") not in {"auto", "frozen_external"}:
        raise CliError("INVALID_TRAINING_CONFIG", "Split mode must be auto or frozen_external.", EXIT_TRAINING_FAILED, {})
    if config.get("split_mode") == "frozen_external" and not config.get("frozen_split_contract", {}).get("authorized"):
        raise CliError("INVALID_FROZEN_SPLIT_CONTRACT", "Frozen split mode requires an explicit authorized contract.", EXIT_TRAINING_FAILED, {})


SMOKE_CONFIG_OVERRIDE: dict[str, Any] = {
    "epochs": 1,
    "batch_size": 8,
    "num_workers": 0,
    "patience": 1,
    "export_onnx": False,
    "classifier_output": "signed_logits",
    "imbalance": {"mode": "balanced_sampler_effective_number", "beta": 0.999, "cap": 5.0},
    "stages": [
        {"name": "smoke", "epochs": 1, "learning_rate": 0.0001, "trainable": "classifier_and_last_fire_modules"},
    ],
}


class JsonlEmitter:
    def __init__(self, command: str, run_id: str) -> None:
        self.command = command
        self.run_id = run_id
        self.sequence = 0

    def emit(self, event: str, level: str = "info", **fields: Any) -> None:
        self.sequence += 1
        payload = {
            "schema_version": 1,
            "event": event,
            "command": self.command,
            "run_id": self.run_id,
            "timestamp": utc_now(),
            "level": level,
            "sequence": self.sequence,
        }
        payload.update(fields)
        print(json.dumps(payload, sort_keys=False), flush=True)


def _deep_update(base: dict[str, Any], override: dict[str, Any]) -> dict[str, Any]:
    result = copy.deepcopy(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = _deep_update(result[key], value)
        else:
            result[key] = value
    return result


def _resolved_onnx_opset(value: Any) -> int:
    return max(int(value if value is not None else MIN_ONNX_OPSET), MIN_ONNX_OPSET)


def load_training_config(path: str | None, schema: ClassSchema, smoke: bool = False) -> dict[str, Any]:
    config = copy.deepcopy(DEFAULT_CONFIG)
    loaded: dict[str, Any] = {}
    config["classes"] = list(schema.classes)
    config["display_labels"] = dict(schema.display_labels)
    if path:
        with Path(path).expanduser().resolve().open("r", encoding="utf-8-sig") as handle:
            loaded = json.load(handle)
        config = _deep_update(config, loaded)
    if "initialization" not in loaded:
        legacy_checkpoint = loaded.get("source_checkpoint_path") or loaded.get("source_checkpoint")
        if legacy_checkpoint:
            config["initialization"] = {"mode": "checkpoint", "checkpoint_path": str(legacy_checkpoint)}
        else:
            config["initialization"] = {"mode": "imagenet"}
    for legacy_key in (
        "pretrained", "source_checkpoint", "source_checkpoint_path", "source_model_path",
        "pretrained_weight_id", "pretrained_weight_path", "pretrained_weight_sha256",
        "classifier_initialization",
    ):
        config.pop(legacy_key, None)
    if smoke:
        config = _deep_update(config, SMOKE_CONFIG_OVERRIDE)
    # Normalize legacy GUI strings without pretending they were already structured.
    if isinstance(config.get("scheduler"), str):
        legacy_scheduler = str(config["scheduler"]).strip().lower()
        config["scheduler"] = {"name": {"none": "none", "steplr": "step", "step": "step", "reducelronplateau": "reduce_on_plateau"}.get(legacy_scheduler, legacy_scheduler)}
    if isinstance(config.get("optimizer"), str):
        config["optimizer"] = {"name": str(config["optimizer"]).strip().lower()}
    augmentation = config.get("augmentation", {})
    if isinstance(augmentation, dict):
        if "random_flip" in augmentation:
            augmentation["horizontal_flip"] = bool(augmentation["random_flip"])
        if "random_crop" in augmentation:
            augmentation["random_resized_crop"] = bool(augmentation["random_crop"])
        if "random_rotation" in augmentation:
            augmentation["affine"] = bool(augmentation["random_rotation"])
    raw_classes = config.get("classes", schema.classes)
    config["classes"] = [
        str(value.get("id")) if isinstance(value, dict) and value.get("id") is not None else str(value)
        for value in raw_classes
    ]
    if config["classes"] != schema.classes:
        raise CliError(
            "CLASS_SCHEMA_MISMATCH",
            "Training config classes must match the selected ordered class schema.",
            EXIT_SCHEMA_MISMATCH,
            {"config_classes": config["classes"], "schema_classes": schema.classes},
        )
    config["display_labels"] = {class_id: str(config.get("display_labels", {}).get(class_id, schema.display_labels.get(class_id, class_id))) for class_id in schema.classes}
    imbalance = config.get("imbalance")
    if not isinstance(imbalance, dict):
        imbalance = {}
    if config.get("class_weights") == "none" and "mode" not in imbalance:
        imbalance["mode"] = "none"
    imbalance.setdefault("mode", "balanced_sampler")
    imbalance.setdefault("sampler_alpha", 0.65)
    imbalance.setdefault("class_weight_formula", "none")
    imbalance.setdefault("computed_from", "training_split_only")
    imbalance.setdefault("class_weights", {})
    config["imbalance"] = imbalance
    requested_onnx_opset = int(config.get("onnx_opset", MIN_ONNX_OPSET))
    if requested_onnx_opset < MIN_ONNX_OPSET:
        config["onnx_requested_opset"] = requested_onnx_opset
        config["onnx_opset"] = _resolved_onnx_opset(requested_onnx_opset)
    else:
        config["onnx_opset"] = _resolved_onnx_opset(requested_onnx_opset)
        config.pop("onnx_requested_opset", None)
    _validate_training_config(config)
    return config


def _run_folder(output: str, run_name: str | None) -> tuple[str, Path]:
    parent = Path(output).expanduser().resolve()
    run_id = run_name or f"run_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    run_dir = parent / run_id
    if run_dir.exists() and any(run_dir.iterdir()):
        suffix = datetime.now().strftime("%f")
        run_id = f"{run_id}_{suffix}"
        run_dir = parent / run_id
    try:
        (run_dir / "checkpoints").mkdir(parents=True, exist_ok=False)
        (run_dir / "logs").mkdir(parents=True, exist_ok=True)
    except Exception as exc:
        raise CliError("OUTPUT_NOT_WRITABLE", "Training run output path is not writable or safe.", EXIT_OUTPUT_INVALID, {"path": str(run_dir), "error": str(exc)})
    return run_id, run_dir


def _require_training_imports() -> dict[str, Any]:
    status: dict[str, Any] = {}
    missing: list[str] = []
    for module_name in ["torch", "torchvision", "numpy", "PIL", "sklearn"]:
        try:
            module = __import__(module_name)
            status[module_name] = {"status": "ok", "version": str(getattr(module, "__version__", ""))}
        except Exception as exc:
            status[module_name] = {"status": "missing", "error": str(exc)}
            missing.append(module_name)
    if missing:
        raise CliError("MISSING_REQUIRED_PACKAGE", "Required training packages are not importable.", EXIT_MISSING_PACKAGE, {"packages": missing, "package_status": status})
    return status


def _seed_everything(seed: int) -> None:
    import numpy as np
    import torch

    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


def _device(requested: str):
    import torch

    if requested == "auto":
        return torch.device("cuda" if torch.cuda.is_available() else "cpu")
    if requested == "cuda" and not torch.cuda.is_available():
        return torch.device("cpu")
    return torch.device(requested)


def _device_warnings(requested: str, device: Any) -> list[dict[str, Any]]:
    if requested == "cuda" and getattr(device, "type", "") == "cpu":
        return [
            {
                "code": "REQUESTED_DEVICE_FALLBACK_CPU",
                "message": "CUDA was requested but is unavailable; falling back to CPU.",
            }
        ]
    return []


def _input_size(config: dict[str, Any]) -> int:
    raw = config.get("input_size", [96, 96, 3])
    if isinstance(raw, int):
        return int(raw)
    if isinstance(raw, list) and raw:
        return int(raw[0])
    return 96


def _build_transforms(config: dict[str, Any]):
    from torchvision import transforms

    size = _input_size(config)
    mean = config["normalization"]["mean"]
    std = config["normalization"]["std"]
    aug = config.get("augmentation", {})
    train_steps: list[Any] = [transforms.Resize(int(size * 1.1))]
    train_steps.append(transforms.RandomResizedCrop(size, scale=(0.9, 1.0), ratio=(0.95, 1.05)) if aug.get("random_resized_crop", True) else transforms.CenterCrop(size))
    if aug.get("affine", True): train_steps.append(transforms.RandomAffine(degrees=5, translate=(0.03, 0.03)))
    if aug.get("horizontal_flip", False): train_steps.append(transforms.RandomHorizontalFlip())
    if aug.get("color_jitter", True): train_steps.append(transforms.ColorJitter(brightness=0.1, contrast=0.1))
    train_steps.extend([transforms.ToTensor(), transforms.Normalize(mean=mean, std=std)])
    train_tf = transforms.Compose(train_steps)
    eval_tf = transforms.Compose(
        [
            transforms.Resize(size),
            transforms.CenterCrop(size),
            transforms.ToTensor(),
            transforms.Normalize(mean=mean, std=std),
        ]
    )
    return train_tf, eval_tf


class ManifestImageDataset:
    def __init__(self, items: list[dict[str, Any]], class_to_idx: dict[str, int], transform: Any) -> None:
        self.items = items
        self.class_to_idx = class_to_idx
        self.transform = transform

    def __len__(self) -> int:
        return len(self.items)

    def __getitem__(self, index: int):
        from PIL import Image

        item = self.items[index]
        with Image.open(item["source_path"]) as image:
            image = image.convert("RGB")
            if self.transform is not None:
                image = self.transform(image)
        return image, self.class_to_idx[item["class_id"]]


def _sampler_weights(items: list[dict[str, Any]], alpha: float) -> list[float]:
    if not 0.0 <= float(alpha) <= 1.0:
        raise ValueError("Sampler exponent must be between 0 and 1")
    counts = Counter(item["class_id"] for item in items)
    return [float(counts[item["class_id"]]) ** (-float(alpha)) for item in items]


def _make_loaders(items: list[dict[str, Any]], schema: ClassSchema, config: dict[str, Any], device: Any):
    import torch
    from torch.utils.data import DataLoader, WeightedRandomSampler

    train_tf, eval_tf = _build_transforms(config)
    class_to_idx = schema.class_to_idx
    train_items = [item for item in items if item.get("role") == "train"]
    val_items = [item for item in items if item.get("role") in {"val", "validation"}]
    test_items = [item for item in items if item.get("role") == "test"]
    train_ds = ManifestImageDataset(train_items, class_to_idx, train_tf)
    val_ds = ManifestImageDataset(val_items, class_to_idx, eval_tf)
    test_ds = ManifestImageDataset(test_items, class_to_idx, eval_tf)
    common = {"batch_size": int(config["batch_size"]), "num_workers": int(config["num_workers"]), "pin_memory": device.type == "cuda"}
    mode = config.get("imbalance", {}).get("mode", "balanced_sampler")
    sampler = None
    if mode in {"balanced_sampler", "balanced_sampler_effective_number"}:
        alpha = float(config.get("imbalance", {}).get("sampler_alpha", 1.0))
        sample_weights = _sampler_weights(train_items, alpha)
        generator = torch.Generator().manual_seed(int(config["seed"]))
        sampler = WeightedRandomSampler(sample_weights, len(sample_weights), replacement=True, generator=generator)
    return (
        DataLoader(train_ds, shuffle=sampler is None, sampler=sampler, **common),
        DataLoader(val_ds, shuffle=False, **common),
        DataLoader(test_ds, shuffle=False, **common) if test_items else None,
    )


def _build_model(config: dict[str, Any], num_classes: int):
    import torch
    import torch.nn as nn
    from torchvision import models

    arch = config.get("architecture", "mobilenet_v3_small")
    initialization = config.get("initialization", {})
    pretrained = initialization.get("mode") == "imagenet"
    packaged_weight_path = str(initialization.get("weight_path", "")).strip()
    packaged_state = None
    if pretrained and packaged_weight_path:
        weight_file = Path(packaged_weight_path).expanduser().resolve()
        if not weight_file.is_file():
            raise CliError("IMAGENET_WEIGHTS_MISSING", "Packaged ImageNet weights are missing.", EXIT_TRAINING_FAILED,
                           {"path": str(weight_file)})
        packaged_state = torch.load(weight_file, map_location="cpu", weights_only=True)
    if arch == "squeezenet1_0":
        weights = models.SqueezeNet1_0_Weights.DEFAULT if pretrained else None
        model = models.squeezenet1_0(weights=weights)
    elif arch == "squeezenet1_1":
        weights = models.SqueezeNet1_1_Weights.DEFAULT if pretrained else None
        model = models.squeezenet1_1(weights=weights)
    elif arch == "mobilenet_v3_small":
        weights = models.MobileNet_V3_Small_Weights.DEFAULT if pretrained and packaged_state is None else None
        model = models.mobilenet_v3_small(weights=weights)
        if packaged_state is not None:
            model.load_state_dict(packaged_state, strict=True)
        torch.manual_seed(int(config.get("seed", 42)))
        model.classifier[-1] = nn.Linear(model.classifier[-1].in_features, num_classes)
        model.num_classes = num_classes
        return model
    elif arch == "efficientnet_b0":
        weights = models.EfficientNet_B0_Weights.DEFAULT if pretrained and packaged_state is None else None
        model = models.efficientnet_b0(weights=weights)
        if packaged_state is not None:
            model.load_state_dict(packaged_state, strict=True)
        torch.manual_seed(int(config.get("seed", 42)))
        model.classifier[-1] = nn.Linear(model.classifier[-1].in_features, num_classes)
        model.num_classes = num_classes
        return model
    else:
        raise CliError("UNSUPPORTED_ARCHITECTURE", "Unsupported training architecture.", EXIT_TRAINING_FAILED, {"architecture": arch})
    model.classifier[1] = nn.Conv2d(512, num_classes, kernel_size=1)
    if config.get("classifier_output", "signed_logits") == "signed_logits":
        model.classifier[2] = nn.Identity()
    model.num_classes = num_classes
    return model


def _load_source_checkpoint(model: Any, path: str | None) -> list[str]:
    if not path:
        return []
    import torch

    ckpt_path = Path(path).expanduser().resolve()
    if not ckpt_path.is_file():
        raise CliError("SOURCE_CHECKPOINT_MISSING", "Configured source checkpoint does not exist.", EXIT_TRAINING_FAILED, {"path": str(ckpt_path)})
    try:
        ckpt = torch.load(ckpt_path, map_location="cpu", weights_only=False)
    except Exception as exc:
        raise CliError("SOURCE_CHECKPOINT_INVALID", "The selected training checkpoint could not be loaded.", EXIT_TRAINING_FAILED, {"path": str(ckpt_path), "error": str(exc)}) from exc
    state = ckpt.get("model_state", ckpt.get("state_dict", ckpt)) if isinstance(ckpt, dict) else ckpt
    if not isinstance(state, dict):
        raise CliError("SOURCE_CHECKPOINT_INVALID", "The selected training checkpoint does not contain model weights.", EXIT_TRAINING_FAILED, {"path": str(ckpt_path)})
    cleaned = {}
    for key, value in state.items():
        for prefix in ["module.", "model.", "net.", "1."]:
            if key.startswith(prefix):
                key = key[len(prefix) :]
        if key.startswith("0."):
            continue
        cleaned[key] = value
    expected_count = int(getattr(model, "num_classes", 0))
    source_count = _checkpoint_output_count(cleaned)
    if source_count is None:
        raise CliError("SOURCE_CHECKPOINT_INVALID", "The selected training checkpoint has no recognizable classifier head.", EXIT_TRAINING_FAILED, {"path": str(ckpt_path)})
    if source_count != expected_count:
        raise CliError(
            "SOURCE_CHECKPOINT_CLASS_COUNT_MISMATCH",
            f"This model has {source_count} outputs, but the selected dataset defines {expected_count} classes. Select a matching dataset or start from ImageNet weights.",
            EXIT_SCHEMA_MISMATCH,
            {"checkpoint_class_count": source_count, "dataset_class_count": expected_count, "path": str(ckpt_path)},
        )
    try:
        model.load_state_dict(cleaned, strict=True)
    except Exception as exc:
        raise CliError("SOURCE_CHECKPOINT_INVALID", "The selected training checkpoint is not compatible with its model architecture.", EXIT_TRAINING_FAILED, {"path": str(ckpt_path), "error": str(exc)}) from exc
    return [f"continued_from_checkpoint={ckpt_path}"]


def _checkpoint_output_count(state: dict[str, Any]) -> int | None:
    """Return the output count from a supported MobileNet/EfficientNet state dict."""
    for suffix in ("classifier.3.weight", "classifier.1.weight"):
        matches = [value for key, value in state.items() if key == suffix or key.endswith(f".{suffix}")]
        if len(matches) == 1 and getattr(matches[0], "ndim", 0) >= 1:
            return int(matches[0].shape[0])
    return None


def _onnx_state_key_candidates(name: str) -> list[str]:
    clean = name.strip()
    if not clean:
        return []

    candidates: list[str] = []
    if ": PARAMETER target='" in clean:
        clean = clean.split(": PARAMETER target='", 1)[0].strip()
    candidates.append(clean)

    if "target='" in name:
        target = name.split("target='", 1)[1].split("'", 1)[0].strip()
        if target:
            candidates.append(target)

    if clean.startswith("p_"):
        compact = clean[2:]
        if compact.endswith("_weight"):
            candidates.append(".".join(compact[:-7].split("_")) + ".weight")
        elif compact.endswith("_bias"):
            candidates.append(".".join(compact[:-5].split("_")) + ".bias")
        else:
            candidates.append(".".join(compact.split("_")))

    deduped: list[str] = []
    for candidate in candidates:
        if candidate and candidate not in deduped:
            deduped.append(candidate)
    return deduped


def _merge_partial_classifier_tensor(expected: Any, source: Any, policy: str) -> Any:
    merged = expected.detach().clone()
    merged[: source.shape[0]] = source.to(dtype=expected.dtype)
    if policy == "partial_mean_existing":
        merged[source.shape[0]:] = source.to(dtype=expected.dtype).mean(dim=0, keepdim=True)
    elif policy == "partial_copy_class1":
        merged[source.shape[0]:] = source[min(1, source.shape[0] - 1)].to(dtype=expected.dtype)
    return merged


def _load_source_onnx(model: Any, path: str, classifier_initialization: str = "preserve_random") -> list[str]:
    import numpy as np
    import onnx
    import torch
    from onnx import numpy_helper

    onnx_path = Path(path).expanduser().resolve()
    if not onnx_path.is_file():
        raise CliError("SOURCE_MODEL_MISSING", "Selected source model does not exist.", EXIT_TRAINING_FAILED, {"path": str(onnx_path)})

    doc = onnx.load(str(onnx_path), load_external_data=True)
    state_template = model.state_dict()
    translated: dict[str, Any] = {}
    skipped: list[str] = []
    partial_imports: list[str] = []
    shape_mismatches: list[dict[str, Any]] = []

    for initializer in doc.graph.initializer:
        target_key = None
        for candidate in _onnx_state_key_candidates(initializer.name):
            if candidate in state_template:
                target_key = candidate
                break
        if target_key is None:
            skipped.append(initializer.name)
            continue

        array = numpy_helper.to_array(initializer)
        tensor = torch.from_numpy(np.array(array, copy=True))
        expected = state_template[target_key]
        if tuple(tensor.shape) != tuple(expected.shape):
            if classifier_initialization != "preserve_random" and target_key in {"classifier.1.weight", "classifier.1.bias"} and tensor.ndim == expected.ndim and tensor.shape[0] < expected.shape[0] and tuple(tensor.shape[1:]) == tuple(expected.shape[1:]):
                translated[target_key] = _merge_partial_classifier_tensor(expected, tensor, classifier_initialization)
                partial_imports.append(f"{target_key}:{tensor.shape[0]}->{expected.shape[0]}:{classifier_initialization}")
                continue
            shape_mismatches.append(
                {
                    "initializer": initializer.name,
                    "target": target_key,
                    "onnx_shape": list(tensor.shape),
                    "model_shape": list(expected.shape),
                }
            )
            continue
        translated[target_key] = tensor.to(dtype=expected.dtype)

    if not translated:
        raise CliError(
            "SOURCE_MODEL_UNSUPPORTED",
            "Selected source model could not be mapped into the current trainer architecture.",
            EXIT_TRAINING_FAILED,
            {"path": str(onnx_path)},
        )

    missing, unexpected = model.load_state_dict(translated, strict=False)
    warnings = [
        f"loaded_onnx_parameters={len(translated)}",
        f"missing_keys={list(missing)}",
        f"unexpected_keys={list(unexpected)}",
    ]
    if skipped:
        warnings.append(f"skipped_initializers={len(skipped)}")
    if partial_imports:
        warnings.append(f"partial_classifier_imports={partial_imports}")
    if shape_mismatches:
        warnings.append(f"shape_mismatches={shape_mismatches[:5]}")
    return warnings


def _load_source_artifact(model: Any, source_model_path: str | None, source_checkpoint: str | None, classifier_initialization: str = "preserve_random") -> list[str]:
    if source_model_path:
        raise CliError(
            "ONNX_NOT_TRAINABLE",
            "ONNX files are used for inference only. Select a model package containing checkpoint.pth to continue training.",
            EXIT_TRAINING_FAILED,
            {"path": str(Path(source_model_path).expanduser().resolve())},
        )
    return _load_source_checkpoint(model, source_checkpoint)


def _freeze_for_stage(model: Any, trainable: str) -> None:
    if trainable == "classifier_and_last_fire_modules":
        for param in model.parameters():
            param.requires_grad = False
        for param in model.classifier.parameters():
            param.requires_grad = True
        fire_indices = [index for index, module in enumerate(model.features) if module.__class__.__name__ == "Fire"]
        for index in fire_indices[-2:]:
            for param in model.features[index].parameters():
                param.requires_grad = True
    elif trainable == "fine_tune":
        for param in model.parameters():
            param.requires_grad = True
        if hasattr(model, "features") and len(model.features) > 0:
            for param in model.features[0].parameters():
                param.requires_grad = False
    elif trainable == "all":
        for param in model.parameters():
            param.requires_grad = True
    elif trainable == "classifier_and_last_blocks":
        for param in model.parameters(): param.requires_grad = False
        for param in model.classifier.parameters(): param.requires_grad = True
        for block in list(model.features.children())[-2:]:
            for param in block.parameters(): param.requires_grad = True
    elif trainable == "controlled_fine_tune":
        for param in model.parameters(): param.requires_grad = True
        first = next(iter(model.features.children()))
        for param in first.parameters(): param.requires_grad = False
    else:
        raise CliError("UNSUPPORTED_TRAINABLE_STAGE", "Unsupported stage trainable setting.", EXIT_TRAINING_FAILED, {"trainable": trainable})


def _classification_metrics(targets: list[int], preds: list[int], classes: list[str]) -> dict[str, Any]:
    import numpy as np
    from sklearn.metrics import accuracy_score, balanced_accuracy_score, confusion_matrix, precision_recall_fscore_support

    labels = list(range(len(classes)))
    cm = confusion_matrix(targets, preds, labels=labels)
    precision, recall, f1, support = precision_recall_fscore_support(targets, preds, labels=labels, zero_division=0)
    return {
        "accuracy": float(accuracy_score(targets, preds)) if targets else 0.0,
        "balanced_accuracy": float(balanced_accuracy_score(targets, preds)) if targets else 0.0,
        "macro_f1": float(np.mean(f1)) if len(f1) else 0.0,
        "confusion_matrix": cm.tolist(),
        "class_metrics": [
            {"class": classes[index], "precision": float(precision[index]), "recall": float(recall[index]), "f1": float(f1[index]), "support": int(support[index])}
            for index in labels
        ],
    }


def _effective_number_weights(counts: dict[str, int], classes: list[str], beta: float = 0.999, cap: float = 10.0) -> dict[str, float]:
    raw = {c: (1.0 - beta) / max(1.0 - beta ** max(int(counts.get(c, 0)), 1), 1e-12) for c in classes}
    floor = min(raw.values())
    return {c: min(raw[c] / floor, cap) for c in classes}


def _logit_diagnostics(logits: list[list[float]], preds: list[int], class_count: int) -> dict[str, Any]:
    import numpy as np
    values = np.asarray(logits, dtype=np.float64)
    finite = bool(values.size and np.isfinite(values).all())
    variance = float(np.var(values)) if finite else float("nan")
    distribution = {str(i): int(preds.count(i)) for i in range(class_count)}
    reasons = []
    if not finite: reasons.append("NON_FINITE_LOGITS")
    if finite and variance <= 1e-12: reasons.append("EFFECTIVELY_CONSTANT_LOGITS")
    if len([v for v in distribution.values() if v]) < class_count: reasons.append("MISSING_PREDICTION_CLASSES")
    return {"finite": finite, "min": float(values.min()) if finite else None, "max": float(values.max()) if finite else None, "variance": variance if finite else None, "prediction_distribution": distribution, "failure_reasons": reasons}


def _run_epoch(model: Any, loader: Any, criterion: Any, optimizer: Any, device: Any, scaler: Any | None = None) -> dict[str, Any]:
    import torch

    model.train()
    total_loss = 0.0
    targets_all: list[int] = []
    preds_all: list[int] = []
    for images, targets in loader:
        images = images.to(device)
        targets = targets.to(device)
        optimizer.zero_grad(set_to_none=True)
        context = torch.amp.autocast(device_type="cuda", enabled=True) if scaler is not None else nullcontext()
        with context:
            outputs = model(images)
            loss = criterion(outputs, targets)
        if scaler is not None:
            scaler.scale(loss).backward()
            scaler.step(optimizer)
            scaler.update()
        else:
            loss.backward()
            optimizer.step()
        total_loss += float(loss.item()) * images.size(0)
        preds_all.extend(outputs.argmax(dim=1).detach().cpu().tolist())
        targets_all.extend(targets.detach().cpu().tolist())
    return {"loss": total_loss / max(len(loader.dataset), 1), "targets": targets_all, "preds": preds_all}


def _evaluate(model: Any, loader: Any, criterion: Any, device: Any) -> dict[str, Any]:
    import torch

    model.eval()
    total_loss = 0.0
    targets_all: list[int] = []
    preds_all: list[int] = []
    logits_all: list[list[float]] = []
    with torch.no_grad():
        for images, targets in loader:
            images = images.to(device)
            targets = targets.to(device)
            outputs = model(images)
            loss = criterion(outputs, targets)
            total_loss += float(loss.item()) * images.size(0)
            preds_all.extend(outputs.argmax(dim=1).detach().cpu().tolist())
            logits_all.extend(outputs.detach().cpu().tolist())
            targets_all.extend(targets.detach().cpu().tolist())
    return {"loss": total_loss / max(len(loader.dataset), 1), "targets": targets_all, "preds": preds_all, "logits": logits_all}


class FocalLoss:
    def __init__(self, weight: Any = None, gamma: float = 2.0) -> None:
        self.weight = weight
        self.gamma = gamma

    def __call__(self, logits: Any, targets: Any) -> Any:
        import torch.nn.functional as functional
        ce = functional.cross_entropy(logits, targets, weight=self.weight, reduction="none")
        return (((1.0 - (-ce).exp()) ** self.gamma) * ce).mean()


def _make_optimizer(model: Any, config: dict[str, Any], learning_rate: float) -> Any:
    import torch.optim as optim
    params = filter(lambda p: p.requires_grad, model.parameters())
    name = config.get("optimizer", {}).get("name", "adam")
    kwargs = {"lr": learning_rate, "weight_decay": float(config.get("weight_decay", 0.0))}
    if name == "adamw": return optim.AdamW(params, **kwargs)
    if name == "sgd": return optim.SGD(params, momentum=float(config.get("optimizer", {}).get("momentum", 0.9)), **kwargs)
    return optim.Adam(params, **kwargs)


def _make_scheduler(optimizer: Any, config: dict[str, Any]) -> Any | None:
    import torch.optim.lr_scheduler as schedulers
    cfg = config.get("scheduler", {})
    if cfg.get("name") == "step":
        return schedulers.StepLR(optimizer, step_size=int(cfg.get("step_size", 5)), gamma=float(cfg.get("gamma", 0.5)))
    if cfg.get("name") == "reduce_on_plateau":
        return schedulers.ReduceLROnPlateau(optimizer, factor=float(cfg.get("factor", 0.5)), patience=int(cfg.get("patience", 2)), min_lr=float(cfg.get("min_lr", 1e-7)))
    return None


def _step_scheduler(scheduler: Any | None, config: dict[str, Any], metric: float) -> None:
    if scheduler is None: return
    if config.get("scheduler", {}).get("name") == "reduce_on_plateau": scheduler.step(metric)
    else: scheduler.step()


def _parameter_delta_l2(before: dict[str, Any], model: Any) -> float:
    import torch
    total = 0.0
    with torch.no_grad():
        for name, value in model.state_dict().items():
            if name in before and value.is_floating_point():
                total += float((value.detach().cpu() - before[name]).pow(2).sum().item())
    return total ** 0.5


def _trainable_parameter_summary(model: Any) -> dict[str, Any]:
    names = [name for name, value in model.named_parameters() if value.requires_grad]
    return {"trainable_parameter_count": sum(value.numel() for value in model.parameters() if value.requires_grad), "total_parameter_count": sum(value.numel() for value in model.parameters()), "trainable_tensor_count": len(names), "trainable_names_sha256": hashlib.sha256("\n".join(names).encode()).hexdigest(), "trainable_name_preview": names[:20]}


def _make_cuda_amp_scaler(torch_module: Any) -> Any:
    grad_scaler = getattr(getattr(torch_module, "amp", None), "GradScaler", None)
    if grad_scaler is not None:
        try:
            return grad_scaler("cuda")
        except TypeError:
            pass
    return torch_module.cuda.amp.GradScaler()


def _write_metrics_artifacts(run_dir: Path, classes: list[str], history: list[dict[str, Any]], test_metrics: dict[str, Any]) -> dict[str, str]:
    metrics_json = run_dir / "metrics.json"
    metrics_csv = run_dir / "metrics.csv"
    cm_csv = run_dir / "confusion_matrix.csv"
    class_csv = run_dir / "class_metrics.csv"
    metrics_json.write_text(json.dumps({"schema_version": 1, "history": history, "test": test_metrics}, indent=2), encoding="utf-8")
    write_csv(metrics_csv, history, ["stage", "epoch", "global_epoch", "train_loss", "train_accuracy", "train_macro_f1", "val_loss", "val_accuracy", "val_macro_f1", "val_balanced_accuracy", "learning_rate", "optimizer_updates", "parameter_delta_l2", "elapsed_seconds"])
    cm_rows = []
    for class_id, row in zip(classes, test_metrics["confusion_matrix"]):
        payload = {"true_label": class_id}
        payload.update({classes[index]: row[index] for index in range(len(classes))})
        cm_rows.append(payload)
    write_csv(cm_csv, cm_rows, ["true_label", *classes])
    write_csv(class_csv, test_metrics["class_metrics"], ["class", "precision", "recall", "f1", "support"])
    return {"metrics_json": str(metrics_json), "metrics_csv": str(metrics_csv), "confusion_matrix_csv": str(cm_csv), "class_metrics_csv": str(class_csv)}


def _primary_onnx_opset(doc: Any) -> int | None:
    versions = [int(entry.version) for entry in doc.opset_import if entry.domain in ("", "ai.onnx")]
    return max(versions) if versions else None


def _export_onnx(model: Any, run_dir: Path, config: dict[str, Any]) -> tuple[Path, int | None]:
    import torch

    onnx_path = run_dir / "model.onnx"
    dummy = torch.zeros(1, 3, _input_size(config), _input_size(config))
    torch.onnx.export(
        copy.deepcopy(model).cpu().eval(),
        dummy,
        onnx_path,
        input_names=["input"],
        output_names=["logits"],
        dynamic_axes={"input": {0: "batch"}, "logits": {0: "batch"}},
        opset_version=_resolved_onnx_opset(config.get("onnx_opset")),
        external_data=False,
    )
    try:
        import onnx

        onnx.checker.check_model(str(onnx_path))
        exported = onnx.load(str(onnx_path), load_external_data=False)
        external = [initializer.name for initializer in exported.graph.initializer if initializer.data_location == onnx.TensorProto.EXTERNAL]
        if external:
            raise ValueError(f"export unexpectedly contains external tensors: {external[:5]}")
    except Exception as exc:
        raise CliError("ONNX_EXPORT_FAILED", "ONNX export or checker validation failed.", EXIT_ONNX_EXPORT_FAILED, {"error": str(exc), "path": str(onnx_path)})
    return onnx_path, _primary_onnx_opset(exported)


def _environment_payload(args: Any, package_status: dict[str, Any], device: Any) -> dict[str, Any]:
    import torch

    return {
        "backend_version": __version__,
        "trainer_contract_version": "initialization-v1",
        "module_file": str(Path(__file__).resolve()),
        "python": {"executable": sys.executable, "version": platform.python_version()},
        "platform": {"system": platform.system(), "release": platform.release(), "machine": platform.machine()},
        "packages": package_status,
        "device": {
            "requested": args.device,
            "selected": device.type,
            "cuda_available": bool(torch.cuda.is_available()),
            "cuda_version": getattr(torch.version, "cuda", None),
            "gpu_names": [torch.cuda.get_device_name(index) for index in range(torch.cuda.device_count())] if torch.cuda.is_available() else [],
        },
        "warnings": _device_warnings(args.device, device),
        "argv": sys.argv,
        "cli_args": {key: str(value) for key, value in vars(args).items()},
    }


def _artifact_hashes(paths: dict[str, str]) -> dict[str, str]:
    result = {}
    for name, value in paths.items():
        path = Path(value)
        if path.is_file():
            result[name] = sha256_file(path)
    return result


def _dataset_display_labels(manifest_path: Path | None, schema: ClassSchema) -> dict[str, str]:
    if manifest_path is None or not manifest_path.is_file():
        return {}
    try:
        with manifest_path.open("r", encoding="utf-8-sig") as handle:
            manifest = json.load(handle)
    except Exception:
        return {}

    display_labels: dict[str, str] = {}
    classes = manifest.get("classes", [])
    if isinstance(classes, list):
        for entry in classes:
            if not isinstance(entry, dict):
                continue
            class_id = str(entry.get("id", "")).strip()
            display_name = str(entry.get("display_name", entry.get("label", entry.get("dataset_label", "")))).strip()
            if class_id in schema.classes and display_name:
                display_labels[class_id] = display_name

    class_schema = manifest.get("class_schema", {})
    if isinstance(class_schema, dict):
        raw_display_labels = class_schema.get("display_labels", {})
        if isinstance(raw_display_labels, dict):
            for class_id in schema.classes:
                value = raw_display_labels.get(class_id)
                if value is not None and str(value).strip():
                    display_labels[class_id] = str(value).strip()

    return display_labels


def _identity_set_hash(values: list[str]) -> str:
    return hashlib.sha256("\n".join(sorted(values)).encode("utf-8")).hexdigest()


def _config_binding_hash(config: dict[str, Any]) -> str:
    keys = ["schema_version", "architecture", "classes", "display_labels", "seed", "input_size", "normalization", "batch_size", "num_workers", "weight_decay", "patience", "min_delta", "use_amp", "classifier_output", "initialization", "export_onnx", "optimizer", "scheduler", "augmentation", "imbalance", "stages", "split_mode", "frozen_split_contract"]
    payload = {key: copy.deepcopy(config.get(key)) for key in keys}
    if isinstance(payload.get("frozen_split_contract"), dict):
        payload["frozen_split_contract"].pop("config_binding_sha256", None)
    return hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()


def _validate_frozen_split(scan: dict[str, Any], config: dict[str, Any], schema: ClassSchema) -> dict[str, Any]:
    contract = config.get("frozen_split_contract", {})
    if _config_binding_hash(config) != contract.get("config_binding_sha256"):
        raise CliError("FROZEN_CONFIG_BINDING_MISMATCH", "Immutable run configuration differs from its frozen contract.", EXIT_SCHEMA_MISMATCH, {})
    initialization = config.get("initialization", {})
    source_path = initialization.get("checkpoint_path") or initialization.get("weight_path")
    expected_hash = initialization.get("checkpoint_sha256") or initialization.get("weight_sha256")
    if not source_path or sha256_file(Path(source_path).expanduser().resolve()) != expected_hash:
        raise CliError("SOURCE_MODEL_HASH_MISMATCH", "Frozen run source model is missing or differs from its immutable hash.", EXIT_SCHEMA_MISMATCH, {"path": source_path})
    items = scan["items"]
    allowed_roles = {"train", "validation"}
    observed_roles = {str(item.get("role", "")) for item in items}
    if not observed_roles or not observed_roles <= allowed_roles:
        raise CliError("FROZEN_SPLIT_ROLE_INVALID", "Frozen screening accepts only train and validation roles.", EXIT_SCHEMA_MISMATCH, {"roles": sorted(observed_roles)})
    expected_items = contract.get("items")
    if not isinstance(expected_items, dict) or not expected_items:
        raise CliError("INVALID_FROZEN_SPLIT_CONTRACT", "Frozen contract must contain the expected item map.", EXIT_SCHEMA_MISMATCH, {})
    seen: dict[str, str] = {}
    seen_sources: dict[str, str] = {}
    seen_hashes: dict[str, str] = {}
    counts = {"train": Counter(), "validation": Counter()}
    actual: dict[str, dict[str, str]] = {}
    for item in items:
        identity = str(item.get("record_id") or item.get("image_id") or "")
        role = str(item.get("role"))
        if not identity or identity in seen:
            raise CliError("FROZEN_SPLIT_DUPLICATE_IDENTITY", "Frozen split contains a missing or duplicate identity.", EXIT_SCHEMA_MISMATCH, {"identity": identity})
        source = str(Path(item["source_path"]).resolve())
        content_hash = sha256_file(Path(source))
        if source in seen_sources or content_hash in seen_hashes:
            raise CliError("FROZEN_SPLIT_CROSS_PARTITION_DUPLICATE", "A source path or content hash repeats in the frozen split.", EXIT_SCHEMA_MISMATCH, {"identity": identity, "source": source, "sha256": content_hash})
        expected = expected_items.get(identity)
        observed = {"role": role, "source_path": source, "class_id": str(item["class_id"]), "sha256": content_hash}
        if not isinstance(expected, dict) or any(str(expected.get(key)) != value for key, value in observed.items()):
            raise CliError("FROZEN_SPLIT_ITEM_MISMATCH", "Frozen split item identity, path, label, role, or hash differs from its contract.", EXIT_SCHEMA_MISMATCH, {"identity": identity, "observed": observed, "expected": expected})
        seen[identity], seen_sources[source], seen_hashes[content_hash] = role, role, role
        counts[role][str(item["class_id"])] += 1
        actual[identity] = observed
    if set(actual) != set(expected_items):
        raise CliError("FROZEN_SPLIT_UNION_MISMATCH", "Frozen split identities do not exactly match the expected union.", EXIT_SCHEMA_MISMATCH, {"missing": sorted(set(expected_items) - set(actual))[:20], "unknown": sorted(set(actual) - set(expected_items))[:20]})
    for role in allowed_roles:
        ids = [identity for identity, observed in actual.items() if observed["role"] == role]
        if _identity_set_hash(ids) != contract.get(f"{role}_identities_sha256"):
            raise CliError("FROZEN_SPLIT_IDENTITY_HASH_MISMATCH", "Frozen partition identity hash differs from its contract.", EXIT_SCHEMA_MISMATCH, {"role": role})
        if len(ids) != int(contract.get("counts", {}).get(role, -1)):
            raise CliError("FROZEN_SPLIT_COUNT_MISMATCH", "Frozen partition count differs from its contract.", EXIT_SCHEMA_MISMATCH, {"role": role, "count": len(ids)})
    if [str(value) for value in contract.get("classes", [])] != schema.classes:
        raise CliError("CLASS_SCHEMA_MISMATCH", "Frozen contract class order differs from the trainer schema.", EXIT_SCHEMA_MISMATCH, {})
    manifest_path = scan.get("manifest_path")
    manifest_hash = sha256_file(manifest_path) if manifest_path else None
    if manifest_hash != contract.get("manifest_sha256"):
        raise CliError("FROZEN_SPLIT_MANIFEST_HASH_MISMATCH", "Frozen trainer manifest hash differs from its contract.", EXIT_SCHEMA_MISMATCH, {"observed": manifest_hash, "expected": contract.get("manifest_sha256")})
    return {"mode": "frozen_external", "authorized": True, "manifest_sha256": manifest_hash, "fold_manifest_sha256": contract.get("fold_manifest_sha256"), "derived_manifest_sha256": contract.get("derived_manifest_sha256"), "config_binding_sha256": contract.get("config_binding_sha256"), "counts": {role: dict(counts[role]) for role in sorted(allowed_roles)}, "identity_counts": {role: sum(counts[role].values()) for role in sorted(allowed_roles)}, "identity_hashes": {role: contract[f"{role}_identities_sha256"] for role in sorted(allowed_roles)}, "union_sha256": _identity_set_hash(list(actual))}


def run_train(args: Any, schema: ClassSchema) -> int:
    config = load_training_config(args.config, schema, smoke=bool(getattr(args, "smoke", False)))
    run_id, run_dir = _run_folder(args.output, args.run_name)
    emitter = JsonlEmitter("train", run_id)
    emitter.emit("run_started", run_dir=str(run_dir), class_schema=schema_payload(schema))
    try:
        package_status = _require_training_imports()
        import torch
        import torch.nn as nn

        device = _device(args.device)
        if device.type == "cuda":
            torch.cuda.reset_peak_memory_stats(device)
        for warning in _device_warnings(args.device, device):
            emitter.emit("warning", level="warning", warning=warning)
        _seed_everything(int(config["seed"]))
        scan = apply_waste_source_mode(scan_dataset(args.dataset, schema), schema, getattr(args, "waste_source_mode", "new-reviewed-only"))
        dataset_display_labels = _dataset_display_labels(scan.get("manifest_path"), schema)
        for class_id in schema.classes:
            current_label = str(config["display_labels"].get(class_id, schema.display_labels.get(class_id, class_id)))
            default_label = str(schema.display_labels.get(class_id, class_id))
            if class_id in dataset_display_labels and current_label == default_label:
                config["display_labels"][class_id] = dataset_display_labels[class_id]
        counts = dict(scan["counts"])
        errors = [{"class": class_id, "count": counts.get(class_id, 0)} for class_id in schema.classes if counts.get(class_id, 0) <= 0]
        if errors:
            raise CliError("INSUFFICIENT_CLASS_EXAMPLES", "Every configured class needs at least one training example.", EXIT_SCHEMA_MISMATCH, {"classes": errors})
        invalid_eligible = scan["summary"].get("invalid_eligible_items", [])
        if invalid_eligible:
            raise CliError("UNREVIEWED_ITEMS_NOT_ELIGIBLE", "Dataset Builder contains trainer-eligible items that are not manually reviewed.", EXIT_SCHEMA_MISMATCH, {"items": invalid_eligible[:20], "count": len(invalid_eligible)})
        frozen_summary = None
        if config.get("split_mode") == "frozen_external":
            frozen_summary = _validate_frozen_split(scan, config, schema)
            split_counts = {role: dict(Counter(item["class_id"] for item in scan["items"] if item.get("role") == role)) for role in ["train", "validation"]}
            support_errors = [{"code": "INVALID_SPLIT_SUPPORT", "message": "Frozen partition must include every class.", "details": {"split": role, "class": class_id, "count": split_counts.get(role, {}).get(class_id, 0)}} for role in ["train", "validation"] for class_id in schema.classes if split_counts.get(role, {}).get(class_id, 0) <= 0]
        else:
            split = config.get("split", DEFAULT_CONFIG["split"])
            split_text = ",".join(f"{key}={split[key]}" for key in ["train", "val", "test"])
            assign_splits(scan["items"], schema, parse_split(split_text), int(config["seed"]))
            split_counts = {role: dict(Counter(item["class_id"] for item in scan["items"] if item.get("role") == role)) for role in ["train", "val", "test"]}
            support_errors = split_support_errors(split_counts, schema)
        if support_errors:
            raise CliError("INVALID_SPLIT_SUPPORT", "Default train/validation/test split does not have enough per-class support.", EXIT_SCHEMA_MISMATCH, {"errors": support_errors})
        readiness_warnings = [*scan["warnings"], *class_balance_warnings(counts, schema), *source_warnings(scan["summary"])]
        emitter.emit("environment", environment=_environment_payload(args, package_status, device))
        emitter.emit(
            "dataset_summary",
            dataset={"path": str(scan["dataset_root"]), "mode": scan["mode"], "manifest_path": str(scan["manifest_path"]) if scan["manifest_path"] else None},
            class_counts=counts,
            split_counts=split_counts,
            excluded_count=int(scan["summary"]["excluded_count"]),
            unreviewed_count=int(scan["summary"]["unreviewed_count"]),
            warnings=readiness_warnings,
            waste_source_selection=scan["waste_source_selection"],
        )
        if frozen_summary is not None:
            emitter.emit("split_contract_accepted", split_contract=frozen_summary)
            if bool(getattr(args, "preflight_only", False)):
                artifact = run_dir / "split_contract.json"
                artifact.write_text(json.dumps(frozen_summary, indent=2), encoding="utf-8")
                emitter.emit("artifact_written", artifact={"type": "split_contract_json", "path": str(artifact), "sha256": sha256_file(artifact), "required": True})
                emitter.emit("run_finished", status="preflight_ok", run_dir=str(run_dir), optimizer_updates=0, model_created=False, optimizer_created=False)
                return 0
        elif bool(getattr(args, "preflight_only", False)):
            raise CliError("PREFLIGHT_REQUIRES_FROZEN_SPLIT", "Preflight-only is permitted only for an authorized frozen split.", EXIT_TRAINING_FAILED, {})
        requested_onnx_opset = config.get("onnx_requested_opset")
        if requested_onnx_opset is not None:
            emitter.emit(
                "warning",
                level="warning",
                code="ONNX_OPSET_UPGRADED",
                message="Requested ONNX opset is below the supported torch exporter floor; exporting at opset 18 to avoid a post-export version-conversion traceback.",
                requested_opset=int(requested_onnx_opset),
                export_opset=int(config["onnx_opset"]),
            )

        train_loader, val_loader, test_loader = _make_loaders(scan["items"], schema, config, device)
        imbalance_mode = str(config.get("imbalance", {}).get("mode", "balanced_sampler"))
        if imbalance_mode == "class_weighted_loss":
            weights = compute_inverse_count_weights(split_counts["train"], schema.classes)
        elif imbalance_mode in {"effective_number", "balanced_sampler_effective_number"}:
            weights = _effective_number_weights(split_counts["train"], schema.classes, float(config["imbalance"].get("beta", 0.999)), float(config["imbalance"].get("cap", 10.0)))
        else:
            weights = None
        if weights:
            config["imbalance"]["class_weights"] = weights
        config["waste_source_selection"] = scan["waste_source_selection"]
        weight_tensor = torch.tensor([weights[class_id] for class_id in schema.classes], dtype=torch.float32, device=device) if weights else None
        criterion = FocalLoss(weight_tensor, float(config["imbalance"].get("gamma", 2.0))) if imbalance_mode == "focal_loss" else nn.CrossEntropyLoss(weight=weight_tensor)
        initialization = config.get("initialization", {})
        source_path = initialization.get("checkpoint_path") or initialization.get("weight_path")
        expected_source_hash = initialization.get("checkpoint_sha256") or initialization.get("weight_sha256")
        if config.get("split_mode") == "frozen_external" and (not source_path or not expected_source_hash or sha256_file(Path(source_path).expanduser().resolve()) != expected_source_hash):
            raise CliError("SOURCE_MODEL_HASH_MISMATCH", "Frozen run source model is missing or differs from its immutable hash.", EXIT_TRAINING_FAILED, {"path": source_path})
        model = _build_model(config, len(schema.classes))
        if initialization.get("mode") == "checkpoint":
            checkpoint_path = str(Path(initialization["checkpoint_path"]).expanduser().resolve())
            _load_source_checkpoint(model, checkpoint_path)
            emitter.emit(
                "checkpoint_loaded",
                architecture=config["architecture"],
                output_count=len(schema.classes),
                path=checkpoint_path,
            )
        else:
            emitter.emit(
                "imagenet_weights_loaded",
                architecture=config["architecture"],
                output_count=len(schema.classes),
                weight_id=initialization.get("weight_id"),
            )
        model = model.to(device)

        baseline_result = _evaluate(model, val_loader, criterion, device)
        baseline_metrics = _classification_metrics(baseline_result["targets"], baseline_result["preds"], schema.classes)
        baseline_diagnostics = _logit_diagnostics(baseline_result["logits"], baseline_result["preds"], len(schema.classes))
        emitter.emit("baseline_metrics", split="validation", metrics=baseline_metrics, diagnostics=baseline_diagnostics)

        emitter.emit("configuration", configuration=config, effective={"device": device.type, "imbalance_mode": imbalance_mode, "class_weights": weights})
        history: list[dict[str, Any]] = []
        global_best_metric = -1.0
        global_best_state = None
        global_best_identity: dict[str, Any] = {}
        total_optimizer_updates = 0
        global_epoch = 0
        true_final_state = None
        started = time.monotonic()
        for stage in config["stages"]:
            _freeze_for_stage(model, stage.get("trainable", "all"))
            optimizer = _make_optimizer(model, config, float(stage["learning_rate"]))
            scheduler = _make_scheduler(optimizer, config)
            patience = int(config.get("patience", 5))
            early_stopping = StageEarlyStopping(patience, float(config.get("min_delta", 1e-6)))
            stage_best_metric = -1.0
            stage_best_state = None
            stage_best_epoch = 0
            stage_initial_state = {name: value.detach().cpu().clone() for name, value in model.state_dict().items()}
            emitter.emit("stage_started", stage=stage["name"], epochs=int(stage["epochs"]), learning_rate=float(stage["learning_rate"]), trainable=stage.get("trainable"), trainable_parameters=_trainable_parameter_summary(model), optimizer=config["optimizer"], scheduler=config["scheduler"], patience=patience)
            if bool(config.get("use_amp", True)) and device.type == "cuda":
                scaler = _make_cuda_amp_scaler(torch)
            else:
                scaler = None
            for epoch in range(1, int(stage["epochs"]) + 1):
                global_epoch += 1
                emitter.emit("epoch_started", stage=stage["name"], epoch=epoch, global_epoch=global_epoch)
                train_result = _run_epoch(model, train_loader, criterion, optimizer, device, scaler)
                total_optimizer_updates += len(train_loader)
                val_result = _evaluate(model, val_loader, criterion, device)
                train_metrics = _classification_metrics(train_result["targets"], train_result["preds"], schema.classes)
                val_metrics = _classification_metrics(val_result["targets"], val_result["preds"], schema.classes)
                row = {
                    "stage": stage["name"],
                    "epoch": epoch,
                    "global_epoch": global_epoch,
                    "train_loss": train_result["loss"],
                    "train_accuracy": train_metrics["accuracy"],
                    "train_macro_f1": train_metrics["macro_f1"],
                    "val_loss": val_result["loss"],
                    "val_accuracy": val_metrics["accuracy"],
                    "val_macro_f1": val_metrics["macro_f1"],
                    "val_balanced_accuracy": val_metrics["balanced_accuracy"],
                    "learning_rate": optimizer.param_groups[0]["lr"],
                    "elapsed_seconds": time.monotonic() - started,
                    "optimizer_updates": total_optimizer_updates,
                    "parameter_delta_l2": _parameter_delta_l2(stage_initial_state, model),
                }
                history.append(row)
                diagnostics = _logit_diagnostics(val_result["logits"], val_result["preds"], len(schema.classes))
                emitter.emit("epoch_metrics", stage=stage["name"], epoch=epoch, global_epoch=global_epoch, metrics={**row, "validation": val_metrics}, diagnostics=diagnostics)
                metric = val_metrics["balanced_accuracy"]
                _step_scheduler(scheduler, config, metric)
                improved, should_stop = early_stopping.update(metric, epoch)
                if improved:
                    stage_best_metric = metric
                    stage_best_state = copy.deepcopy(model.state_dict())
                    stage_best_epoch = epoch
                    stage_path = run_dir / "checkpoints" / f"{stage['name']}_best.pth"
                    torch.save({"model_state": stage_best_state, "config": config, "stage": stage["name"], "stage_epoch": epoch, "global_epoch": global_epoch, "metric": metric}, stage_path)
                    emitter.emit("checkpoint_saved", checkpoint={"identity": "stage_best", "path": str(stage_path), "sha256": sha256_file(stage_path), "metric": "val_balanced_accuracy", "value": metric})
                    if metric > global_best_metric + float(config.get("min_delta", 1e-6)):
                        global_best_metric, global_best_state = metric, copy.deepcopy(stage_best_state)
                        global_best_identity = {"stage": stage["name"], "stage_epoch": epoch, "global_epoch": global_epoch}
                if should_stop:
                    emitter.emit("early_stopping", stage=stage["name"], epoch=epoch, best_epoch=stage_best_epoch, patience=patience, reason="NO_VALIDATION_BALANCED_ACCURACY_IMPROVEMENT")
                    break
            if stage_best_state is None or _parameter_delta_l2(stage_initial_state, model) <= 0.0:
                raise CliError("INVALID_CHECKPOINT", "Stage produced no restorable validation checkpoint.", EXIT_TRAINING_FAILED, {"stage": stage["name"]})
            true_final_state = copy.deepcopy(model.state_dict())
            model.load_state_dict(stage_best_state)
            emitter.emit("stage_finished", stage=stage["name"], best_epoch=stage_best_epoch, best_balanced_accuracy=stage_best_metric, restored_checkpoint=True)
        final_path = run_dir / "checkpoints" / "final.pth"
        torch.save({"model_state": true_final_state, "config": config, "identity": "true_final_pre_restore", "global_epoch": global_epoch, "optimizer_updates": total_optimizer_updates}, final_path)
        emitter.emit("checkpoint_saved", checkpoint={"identity": "true_final", "path": str(final_path), "sha256": sha256_file(final_path)})
        if global_best_state is None or total_optimizer_updates <= 0:
            raise CliError("ZERO_EFFECTIVE_OPTIMIZER_UPDATES", "Training produced no effective optimizer updates or valid checkpoint.", EXIT_TRAINING_FAILED, {"optimizer_updates": total_optimizer_updates})
        model.load_state_dict(global_best_state)
        best_path = run_dir / "checkpoints" / "best.pth"
        package_checkpoint = {
            "model_state": global_best_state,
            "architecture": config.get("architecture"),
            "class_count": len(schema.classes),
            "class_ids": list(schema.classes),
            "display_labels": dict(config["display_labels"]),
            "input_size": list(config["input_size"]),
            "config": config,
            "identity": global_best_identity,
            "metric": global_best_metric,
        }
        torch.save(package_checkpoint, best_path)
        package_checkpoint_path = run_dir / "checkpoint.pth"
        torch.save(package_checkpoint, package_checkpoint_path)
        emitter.emit("checkpoint_saved", checkpoint={"identity": "global_best", "path": str(best_path), "sha256": sha256_file(best_path), "metric": "val_balanced_accuracy", "value": global_best_metric})
        emitter.emit("artifact_written", artifact={"type": "package_checkpoint", "path": str(package_checkpoint_path), "required": True})
        persisted_best = torch.load(best_path, map_location=device, weights_only=False)
        if not isinstance(persisted_best, dict) or "model_state" not in persisted_best:
            raise CliError("INVALID_CHECKPOINT", "Selected best checkpoint is not restorable.", EXIT_TRAINING_FAILED, {"path": str(best_path)})
        model.load_state_dict(persisted_best["model_state"], strict=True)
        evaluation_loader = val_loader if config.get("split_mode") == "frozen_external" else test_loader
        evaluation_split = "validation" if config.get("split_mode") == "frozen_external" else "test"
        test_result = _evaluate(model, evaluation_loader, criterion, device)
        test_metrics = _classification_metrics(test_result["targets"], test_result["preds"], schema.classes)
        test_metrics["loss"] = test_result["loss"]
        test_metrics["logit_diagnostics"] = _logit_diagnostics(test_result["logits"], test_result["preds"], len(schema.classes))
        evaluated_items = list(evaluation_loader.dataset.items)
        prediction_rows = [{"record_id": str(item.get("record_id") or item.get("image_id") or index), "source_path": item["source_path"], "class_id": str(schema.classes[int(target)]), "predicted_class_id": str(schema.classes[int(pred)]), "logits": logits} for index, (item, target, pred, logits) in enumerate(zip(evaluated_items, test_result["targets"], test_result["preds"], test_result["logits"]))]
        telemetry = {"elapsed_seconds": time.monotonic() - started, "optimizer_updates": total_optimizer_updates, "device": device.type, "device_name": torch.cuda.get_device_name(device) if device.type == "cuda" else "CPU", "peak_cuda_memory_allocated": int(torch.cuda.max_memory_allocated(device)) if device.type == "cuda" else 0, "peak_cuda_memory_reserved": int(torch.cuda.max_memory_reserved(device)) if device.type == "cuda" else 0}
        test_metrics["telemetry"] = telemetry
        predictions_path = run_dir / "validation_predictions.jsonl"
        predictions_path.write_text("".join(json.dumps(row, sort_keys=True) + "\n" for row in prediction_rows), encoding="utf-8")
        fatal = [reason for reason in test_metrics["logit_diagnostics"]["failure_reasons"] if reason in {"NON_FINITE_LOGITS", "EFFECTIVELY_CONSTANT_LOGITS", "MISSING_PREDICTION_CLASSES"}]
        if fatal:
            failure_path = run_dir / "failure_diagnostics.json"
            failure_path.write_text(json.dumps({"split": evaluation_split, "metrics": test_metrics, "prediction_rows": len(prediction_rows), "checkpoint_identity": "global_best"}, indent=2), encoding="utf-8")
            emitter.emit("artifact_written", artifact={"type": "validation_predictions_jsonl", "path": str(predictions_path), "sha256": sha256_file(predictions_path), "required": True})
            emitter.emit("artifact_written", artifact={"type": "failure_diagnostics_json", "path": str(failure_path), "sha256": sha256_file(failure_path), "required": True})
            emitter.emit("run_diagnostics", split=evaluation_split, diagnostics=test_metrics["logit_diagnostics"], telemetry=telemetry)
            raise CliError("MODEL_COLLAPSE_DETECTED", "Training predictions failed the collapse or class-coverage gate.", EXIT_TRAINING_FAILED, {"failure_reasons": fatal, "diagnostics": test_metrics["logit_diagnostics"], "telemetry": telemetry})
        emitter.emit("validation_metrics", split=evaluation_split, checkpoint_identity="global_best", metrics=test_metrics)

        artifacts = _write_metrics_artifacts(run_dir, schema.classes, history, test_metrics)
        artifacts["validation_predictions_jsonl"] = str(predictions_path)
        artifacts.update({"best_checkpoint": str(best_path), "final_checkpoint": str(final_path), "package_checkpoint": str(package_checkpoint_path)})
        config_path = run_dir / "training_config.json"
        config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
        env_path = run_dir / "environment.json"
        env_path.write_text(json.dumps(_environment_payload(args, package_status, device), indent=2), encoding="utf-8")
        artifacts.update({"training_config_json": str(config_path), "environment_json": str(env_path)})
        onnx_path = None
        onnx_opset = None
        if bool(config.get("export_onnx", True)):
            onnx_path, onnx_opset = _export_onnx(model, run_dir, config)
            artifacts["model_onnx"] = str(onnx_path)
        onnx_external_data_files: list[dict[str, Any]] = []
        if onnx_path:
            sidecar_names = {f"{onnx_path.name}.data"}
            sidecar_names.update(path.name for path in run_dir.glob("*.onnx.data") if path.is_file())
            for sidecar_name in sorted(sidecar_names):
                sidecar_path = run_dir / sidecar_name
                if sidecar_path.is_file():
                    onnx_external_data_files.append(
                        {
                            "filename": sidecar_name,
                            "sha256": sha256_file(sidecar_path),
                            "byte_size": sidecar_path.stat().st_size,
                            "required": True,
                        }
                    )
        metadata_path = run_dir / "metadata.json"
        sorting_policy = sorting_policy_for_classes(schema.classes, config["display_labels"])
        metadata = {
            "schema_version": "simple-model-package-v1",
            "model_id": run_id,
            "model_name": args.run_name or (
                "Droplet MobileNetV3-Small candidate"
                if config.get("architecture") == "mobilenet_v3_small"
                else "Droplet EfficientNet-B0 candidate"
                if config.get("architecture") == "efficientnet_b0"
                else "Droplet model candidate"
            ),
            "created_at": utc_now(),
            "training_code_version": __version__,
            "label_schema_version": schema.schema_id,
            "class_count": len(schema.classes),
            "class_ids": list(schema.classes),
            "classes": schema.classes,
            "class_to_idx": schema.class_to_idx,
            "display_labels": config["display_labels"],
            "sorting_policy": sorting_policy,
            "input_size": config["input_size"],
            "trained": True,
            "files": {"checkpoint": "checkpoint.pth", "onnx": "model.onnx" if onnx_path else None, "metadata": "metadata.json"},
            "normalization": config["normalization"],
            "architecture": (
                {"family": "MobileNetV3", "variant": "small"}
                if config.get("architecture") == "mobilenet_v3_small"
                else {"family": "EfficientNet", "variant": "b0"}
                if config.get("architecture") == "efficientnet_b0"
                else {"family": "SqueezeNet", "variant": str(config.get("architecture", "squeezenet1_1")).replace("squeezenet", "")}
            ),
            "training_config": config,
            "artifact": {
                "onnx_file": onnx_path.name if onnx_path else None,
                "format": "onnx",
                "opset": onnx_opset,
            },
            "dataset_summary": {
                "class_counts": counts,
                "included_class_counts": counts,
                "split_counts": split_counts,
                "excluded_count": int(scan["summary"]["excluded_count"]),
                "unreviewed_count": int(scan["summary"]["unreviewed_count"]),
                "source_kind_counts": dict(scan["summary"]["source_kind_counts"]),
                "waste_source_counts": dict(scan["summary"]["waste_source_counts"]),
                "waste_source_selection": scan["waste_source_selection"],
            },
            "imbalance": config["imbalance"],
            "validation_summary": {"image_validation": {"status": "internal_test_split", **test_metrics}, "sequence_validation": {"status": "not_run"}},
            "checkpoint_selection": {"selected": "global_best", "identity": global_best_identity, "metric": "val_balanced_accuracy", "value": global_best_metric},
            "export": {"format": "onnx", "opset": onnx_opset},
            "limitations": ["Sequence validation has not been run for this candidate."],
        }
        metadata_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")
        artifacts["metadata_json"] = str(metadata_path)
        provenance_path = run_dir / "provenance.json"
        provenance = {
            "schema_version": 1,
            "created_at": utc_now(),
            "dataset": {"path": str(scan["dataset_root"]), "manifest_path": str(scan["manifest_path"]) if scan["manifest_path"] else None, "manifest_hash_sha256": sha256_file(scan["manifest_path"]) if scan["manifest_path"] else None},
            "class_schema": schema_payload(schema),
            "split_seed": config["seed"],
            "split_counts": split_counts,
            "excluded_count": int(scan["summary"]["excluded_count"]),
            "waste_source_selection": scan["waste_source_selection"],
            "excluded_item_manifest_hash": sha256_file(scan["manifest_path"]) if scan["manifest_path"] and int(scan["summary"]["excluded_count"]) else None,
            "imbalance": config["imbalance"],
            "initialization": config.get("initialization"),
            "wsl_source_lineage": WSL_SOURCE_LINEAGE,
            "config_hash_sha256": hashlib.sha256(json.dumps(config, sort_keys=True).encode("utf-8")).hexdigest(),
            "artifact_hashes": _artifact_hashes(artifacts),
            "checkpoint_selection": {"selected": "global_best", "identity": global_best_identity, "metric": "val_balanced_accuracy", "value": global_best_metric, "best_sha256": sha256_file(best_path), "true_final_sha256": sha256_file(final_path)},
            "known_limitations": ["No prepared dataset worker report was present during this implementation pass; command remains dataset-path driven."],
        }
        provenance_path.write_text(json.dumps(provenance, indent=2), encoding="utf-8")
        artifacts["provenance_json"] = str(provenance_path)
        for name, value in artifacts.items():
            emitter.emit("artifact_written", artifact={"type": name, "path": value, "sha256": sha256_file(Path(value)) if Path(value).is_file() else None, "required": True})
        emitter.emit("run_finished", status="ok", run_dir=str(run_dir), artifacts=artifacts)
        return 0
    except CliError as exc:
        emitter.emit("error", level="error", error={"code": exc.code, "message": exc.message, "details": exc.details}, exit_code=exc.exit_code)
        return exc.exit_code
    except Exception as exc:
        emitter.emit("error", level="error", error={"code": "TRAINING_FAILED", "message": str(exc), "details": {"exception_type": exc.__class__.__name__}}, exit_code=EXIT_TRAINING_FAILED)
        return EXIT_TRAINING_FAILED
