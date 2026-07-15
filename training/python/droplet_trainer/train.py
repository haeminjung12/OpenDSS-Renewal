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
from .errors import CliError, EXIT_DEVICE_UNAVAILABLE, EXIT_MISSING_PACKAGE, EXIT_ONNX_EXPORT_FAILED, EXIT_OUTPUT_INVALID, EXIT_SCHEMA_MISMATCH, EXIT_TRAINING_FAILED
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
    "schema_version": 1,
    "architecture": "squeezenet1_1",
    "input_size": [96, 96, 3],
    "normalization": {
        "mean": [0.485, 0.456, 0.406],
        "std": [0.229, 0.224, 0.225],
    },
    "split": {"train": 0.70, "val": 0.15, "test": 0.15},
    "seed": 42,
    "batch_size": 32,
    "num_workers": 0,
    "epochs": 35,
    "stages": [
        {"name": "stage1", "epochs": 20, "learning_rate": 0.0001, "trainable": "classifier_and_last_fire_modules"},
        {"name": "stage2", "epochs": 15, "learning_rate": 0.00001, "trainable": "fine_tune"},
    ],
    "weight_decay": 0.0001,
    "patience": 5,
    "use_amp": True,
    "imbalance": {
        "mode": "class_weighted_loss",
        "class_weight_formula": "inverse_count_min_normalized",
        "computed_from": "training_split_only",
        "class_weights": {},
    },
    "pretrained": False,
    "source_checkpoint": None,
    "source_model_path": None,
    "export_onnx": True,
    "onnx_opset": MIN_ONNX_OPSET,
    "optional_logging": {"wandb": False},
}


SMOKE_CONFIG_OVERRIDE: dict[str, Any] = {
    "epochs": 1,
    "batch_size": 8,
    "num_workers": 0,
    "patience": 1,
    "export_onnx": False,
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
    config["classes"] = list(schema.classes)
    config["display_labels"] = dict(schema.display_labels)
    if path:
        with Path(path).expanduser().resolve().open("r", encoding="utf-8-sig") as handle:
            loaded = json.load(handle)
        config = _deep_update(config, loaded)
    if smoke:
        config = _deep_update(config, SMOKE_CONFIG_OVERRIDE)
    config["classes"] = [str(value) for value in config.get("classes", schema.classes)]
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
    imbalance.setdefault("mode", "class_weighted_loss")
    imbalance.setdefault("class_weight_formula", "inverse_count_min_normalized")
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
        raise CliError("REQUESTED_DEVICE_UNAVAILABLE", "CUDA was requested but torch.cuda.is_available() is false.", EXIT_DEVICE_UNAVAILABLE)
    return torch.device(requested)


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
    train_tf = transforms.Compose(
        [
            transforms.Resize(int(size * 1.1)),
            transforms.RandomResizedCrop(size, scale=(0.9, 1.0), ratio=(0.95, 1.05)),
            transforms.RandomAffine(degrees=5, translate=(0.03, 0.03)),
            transforms.ColorJitter(brightness=0.1, contrast=0.1),
            transforms.ToTensor(),
            transforms.Normalize(mean=mean, std=std),
        ]
    )
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


def _make_loaders(items: list[dict[str, Any]], schema: ClassSchema, config: dict[str, Any], device: Any):
    import torch
    from torch.utils.data import DataLoader

    train_tf, eval_tf = _build_transforms(config)
    class_to_idx = schema.class_to_idx
    train_items = [item for item in items if item.get("role") == "train"]
    val_items = [item for item in items if item.get("role") == "val"]
    test_items = [item for item in items if item.get("role") == "test"]
    train_ds = ManifestImageDataset(train_items, class_to_idx, train_tf)
    val_ds = ManifestImageDataset(val_items, class_to_idx, eval_tf)
    test_ds = ManifestImageDataset(test_items, class_to_idx, eval_tf)
    common = {"batch_size": int(config["batch_size"]), "num_workers": int(config["num_workers"]), "pin_memory": device.type == "cuda"}
    return (
        DataLoader(train_ds, shuffle=True, **common),
        DataLoader(val_ds, shuffle=False, **common),
        DataLoader(test_ds, shuffle=False, **common),
    )


def _build_model(config: dict[str, Any], num_classes: int):
    import torch.nn as nn
    from torchvision import models

    arch = config.get("architecture", "squeezenet1_1")
    pretrained = bool(config.get("pretrained", False))
    if arch == "squeezenet1_0":
        weights = models.SqueezeNet1_0_Weights.DEFAULT if pretrained else None
        model = models.squeezenet1_0(weights=weights)
    elif arch == "squeezenet1_1":
        weights = models.SqueezeNet1_1_Weights.DEFAULT if pretrained else None
        model = models.squeezenet1_1(weights=weights)
    else:
        raise CliError("UNSUPPORTED_ARCHITECTURE", "Only squeezenet1_0 and squeezenet1_1 are currently supported.", EXIT_TRAINING_FAILED, {"architecture": arch})
    model.classifier[1] = nn.Conv2d(512, num_classes, kernel_size=1)
    model.num_classes = num_classes
    return model


def _load_source_checkpoint(model: Any, path: str | None) -> list[str]:
    if not path:
        return []
    import torch

    ckpt_path = Path(path).expanduser().resolve()
    if not ckpt_path.is_file():
        raise CliError("SOURCE_CHECKPOINT_MISSING", "Configured source checkpoint does not exist.", EXIT_TRAINING_FAILED, {"path": str(ckpt_path)})
    ckpt = torch.load(ckpt_path, map_location="cpu")
    state = ckpt.get("model_state", ckpt.get("state_dict", ckpt)) if isinstance(ckpt, dict) else ckpt
    cleaned = {}
    for key, value in state.items():
        for prefix in ["module.", "model.", "net.", "1."]:
            if key.startswith(prefix):
                key = key[len(prefix) :]
        if key.startswith("0."):
            continue
        cleaned[key] = value
    missing, unexpected = model.load_state_dict(cleaned, strict=False)
    return [f"missing_keys={list(missing)}", f"unexpected_keys={list(unexpected)}"]


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


def _load_source_onnx(model: Any, path: str) -> list[str]:
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
    if shape_mismatches:
        warnings.append(f"shape_mismatches={shape_mismatches[:5]}")
    return warnings


def _load_source_artifact(model: Any, source_model_path: str | None, source_checkpoint: str | None) -> list[str]:
    if source_model_path:
        return _load_source_onnx(model, source_model_path)
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
    else:
        raise CliError("UNSUPPORTED_TRAINABLE_STAGE", "Unsupported stage trainable setting.", EXIT_TRAINING_FAILED, {"trainable": trainable})


def _classification_metrics(targets: list[int], preds: list[int], classes: list[str]) -> dict[str, Any]:
    import numpy as np
    from sklearn.metrics import accuracy_score, confusion_matrix, precision_recall_fscore_support

    labels = list(range(len(classes)))
    cm = confusion_matrix(targets, preds, labels=labels)
    precision, recall, f1, support = precision_recall_fscore_support(targets, preds, labels=labels, zero_division=0)
    return {
        "accuracy": float(accuracy_score(targets, preds)) if targets else 0.0,
        "macro_f1": float(np.mean(f1)) if len(f1) else 0.0,
        "confusion_matrix": cm.tolist(),
        "class_metrics": [
            {"class": classes[index], "precision": float(precision[index]), "recall": float(recall[index]), "f1": float(f1[index]), "support": int(support[index])}
            for index in labels
        ],
    }


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
    with torch.no_grad():
        for images, targets in loader:
            images = images.to(device)
            targets = targets.to(device)
            outputs = model(images)
            loss = criterion(outputs, targets)
            total_loss += float(loss.item()) * images.size(0)
            preds_all.extend(outputs.argmax(dim=1).detach().cpu().tolist())
            targets_all.extend(targets.detach().cpu().tolist())
    return {"loss": total_loss / max(len(loader.dataset), 1), "targets": targets_all, "preds": preds_all}


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
    write_csv(metrics_csv, history, ["stage", "epoch", "train_loss", "train_accuracy", "train_macro_f1", "val_loss", "val_accuracy", "val_macro_f1", "learning_rate", "elapsed_seconds"])
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
    )
    try:
        import onnx

        onnx.checker.check_model(str(onnx_path))
        exported = onnx.load(str(onnx_path), load_external_data=True)
    except Exception as exc:
        raise CliError("ONNX_EXPORT_FAILED", "ONNX export or checker validation failed.", EXIT_ONNX_EXPORT_FAILED, {"error": str(exc), "path": str(onnx_path)})
    return onnx_path, _primary_onnx_opset(exported)


def _environment_payload(args: Any, package_status: dict[str, Any], device: Any) -> dict[str, Any]:
    import torch

    return {
        "backend_version": __version__,
        "python": {"executable": sys.executable, "version": platform.python_version()},
        "platform": {"system": platform.system(), "release": platform.release(), "machine": platform.machine()},
        "packages": package_status,
        "device": {
            "selected": device.type,
            "cuda_available": bool(torch.cuda.is_available()),
            "cuda_version": getattr(torch.version, "cuda", None),
            "gpu_names": [torch.cuda.get_device_name(index) for index in range(torch.cuda.device_count())] if torch.cuda.is_available() else [],
        },
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


def run_train(args: Any, schema: ClassSchema) -> int:
    config = load_training_config(args.config, schema, smoke=bool(getattr(args, "smoke", False)))
    run_id, run_dir = _run_folder(args.output, args.run_name)
    emitter = JsonlEmitter("train", run_id)
    emitter.emit("run_started", run_dir=str(run_dir), class_schema=schema_payload(schema))
    try:
        package_status = _require_training_imports()
        import torch
        import torch.nn as nn
        import torch.optim as optim

        device = _device(args.device)
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
        imbalance_mode = str(config.get("imbalance", {}).get("mode", "class_weighted_loss"))
        weights = compute_inverse_count_weights(split_counts["train"], schema.classes) if imbalance_mode == "class_weighted_loss" else None
        if weights:
            config["imbalance"]["class_weights"] = weights
        config["waste_source_selection"] = scan["waste_source_selection"]
        weight_tensor = torch.tensor([weights[class_id] for class_id in schema.classes], dtype=torch.float32, device=device) if weights else None
        criterion = nn.CrossEntropyLoss(weight=weight_tensor)
        model = _build_model(config, len(schema.classes))
        checkpoint_warnings = _load_source_artifact(
            model, config.get("source_model_path"), config.get("source_checkpoint")
        )
        for warning in checkpoint_warnings:
            emitter.emit("warning", level="warning", code="SOURCE_CHECKPOINT_PARTIAL_LOAD", message=warning)
        model = model.to(device)

        history: list[dict[str, Any]] = []
        best_metric = -1.0
        best_state = None
        started = time.monotonic()
        for stage in config["stages"]:
            _freeze_for_stage(model, stage.get("trainable", "all"))
            optimizer = optim.Adam(filter(lambda p: p.requires_grad, model.parameters()), lr=float(stage["learning_rate"]), weight_decay=float(config["weight_decay"]))
            patience = int(config.get("patience", 5))
            stale = 0
            emitter.emit("stage_started", stage=stage["name"], epochs=int(stage["epochs"]), trainable=stage.get("trainable"))
            if bool(config.get("use_amp", True)) and device.type == "cuda":
                scaler = _make_cuda_amp_scaler(torch)
            else:
                scaler = None
            for epoch in range(1, int(stage["epochs"]) + 1):
                emitter.emit("epoch_started", stage=stage["name"], epoch=epoch)
                train_result = _run_epoch(model, train_loader, criterion, optimizer, device, scaler)
                val_result = _evaluate(model, val_loader, criterion, device)
                train_metrics = _classification_metrics(train_result["targets"], train_result["preds"], schema.classes)
                val_metrics = _classification_metrics(val_result["targets"], val_result["preds"], schema.classes)
                row = {
                    "stage": stage["name"],
                    "epoch": epoch,
                    "train_loss": train_result["loss"],
                    "train_accuracy": train_metrics["accuracy"],
                    "train_macro_f1": train_metrics["macro_f1"],
                    "val_loss": val_result["loss"],
                    "val_accuracy": val_metrics["accuracy"],
                    "val_macro_f1": val_metrics["macro_f1"],
                    "learning_rate": optimizer.param_groups[0]["lr"],
                    "elapsed_seconds": time.monotonic() - started,
                }
                history.append(row)
                emitter.emit("epoch_metrics", stage=stage["name"], epoch=epoch, metrics=row)
                if val_metrics["macro_f1"] > best_metric + 1e-6:
                    best_metric = val_metrics["macro_f1"]
                    best_state = copy.deepcopy(model.state_dict())
                    stale = 0
                    best_path = run_dir / "checkpoints" / "best.pth"
                    torch.save({"model_state": best_state, "config": config, "classes": schema.classes, "class_to_idx": schema.class_to_idx}, best_path)
                    emitter.emit("checkpoint_saved", checkpoint={"path": str(best_path), "metric": "val_macro_f1", "value": best_metric})
                else:
                    stale += 1
                if stale >= patience:
                    emitter.emit("progress", phase=stage["name"], current=epoch, total=int(stage["epochs"]), unit="epoch", percent=100.0 * epoch / int(stage["epochs"]), message="Early stopping patience reached.")
                    break
        if best_state is not None:
            model.load_state_dict(best_state)
        torch.save({"model_state": model.state_dict(), "config": config, "classes": schema.classes, "class_to_idx": schema.class_to_idx}, run_dir / "checkpoints" / "last.pth")
        test_result = _evaluate(model, test_loader, criterion, device)
        test_metrics = _classification_metrics(test_result["targets"], test_result["preds"], schema.classes)
        test_metrics["loss"] = test_result["loss"]
        emitter.emit("validation_metrics", split="test", metrics=test_metrics)

        artifacts = _write_metrics_artifacts(run_dir, schema.classes, history, test_metrics)
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
        metadata_path = run_dir / "metadata.json"
        sorting_policy = sorting_policy_for_classes(schema.classes, config["display_labels"])
        metadata = {
            "schema_version": "model-metadata-v1",
            "model_id": run_id,
            "model_name": args.run_name or "Droplet SqueezeNet candidate",
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
            "normalization": config["normalization"],
            "architecture": {"family": "SqueezeNet", "variant": str(config.get("architecture", "squeezenet1_1")).replace("squeezenet", "")},
            "training_config": config,
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
            "export": {"format": "onnx", "opset": onnx_opset, "sha256": sha256_file(onnx_path) if onnx_path else None},
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
            "source_model_path": config.get("source_model_path"),
            "source_checkpoint": config.get("source_checkpoint"),
            "wsl_source_lineage": WSL_SOURCE_LINEAGE,
            "config_hash_sha256": hashlib.sha256(json.dumps(config, sort_keys=True).encode("utf-8")).hexdigest(),
            "artifact_hashes": _artifact_hashes(artifacts),
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
