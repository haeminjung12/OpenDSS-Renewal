from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DEFAULT_CHECKPOINT = (
    r"\\wsl.localhost\Ubuntu\home\haeminjung\CNN_Droplet_Sorting"
    r"\python_env\.cache\torch\hub\checkpoints\squeezenet1_1-b8a52dc0.pth"
)
DEFAULT_LEGACY_TRAIN_SCRIPT = (
    r"\\wsl.localhost\Ubuntu\home\haeminjung\CNN_Droplet_Sorting\python_env\train_droplet.py"
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _default_paths(script_path: Path) -> tuple[Path, Path]:
    models_dir = script_path.resolve().parents[1] / "models"
    return (
        models_dir / "blank_squeezenet_template.onnx",
        models_dir / "blank_squeezenet_template_metadata.json",
    )


def _value_info_shape(value_info: Any) -> list[int | str | None]:
    dims: list[int | str | None] = []
    tensor_type = value_info.type.tensor_type
    for dim in tensor_type.shape.dim:
        if dim.HasField("dim_value"):
            dims.append(int(dim.dim_value))
        elif dim.HasField("dim_param"):
            dims.append(dim.dim_param)
        else:
            dims.append(None)
    return dims


def _clean_state_dict(raw: Any) -> dict[str, Any]:
    if isinstance(raw, dict):
        state = raw.get("model_state", raw.get("state_dict", raw))
    else:
        state = raw
    if not isinstance(state, dict):
        raise TypeError("Checkpoint does not contain a state_dict-like mapping.")
    cleaned: dict[str, Any] = {}
    for key, value in state.items():
        name = str(key)
        for prefix in ("module.", "model.", "net.", "1."):
            if name.startswith(prefix):
                name = name[len(prefix) :]
        if name.startswith("0."):
            continue
        cleaned[name] = value
    return cleaned


def generate_blank_model(
    checkpoint_path: Path,
    checkpoint_record_path: str,
    output_model: Path,
    output_metadata: Path,
    seed: int,
    num_classes: int,
    opset: int,
) -> dict[str, Any]:
    import onnx
    import torch
    import torch.nn as nn
    import torchvision
    from torchvision import models

    checkpoint = checkpoint_path.expanduser().resolve()
    if not checkpoint.is_file():
        raise FileNotFoundError(f"Checkpoint does not exist: {checkpoint}")
    recorded_checkpoint = checkpoint_record_path.strip() or str(checkpoint)

    torch.manual_seed(seed)
    try:
        raw_state = torch.load(str(checkpoint), map_location="cpu", weights_only=True)
    except TypeError:
        raw_state = torch.load(str(checkpoint), map_location="cpu")
    state_dict = _clean_state_dict(raw_state)

    model = models.squeezenet1_1(weights=None)
    model.load_state_dict(state_dict)

    torch.manual_seed(seed)
    model.classifier[1] = nn.Conv2d(512, num_classes, kernel_size=1)
    model.num_classes = num_classes
    model = model.cpu().eval()

    output_model.parent.mkdir(parents=True, exist_ok=True)
    dummy = torch.zeros(1, 3, 96, 96)
    torch.onnx.export(
        model,
        dummy,
        output_model,
        input_names=["input"],
        output_names=["logits"],
        dynamic_axes={"input": {0: "batch"}, "logits": {0: "batch"}},
        opset_version=opset,
    )
    onnx.checker.check_model(str(output_model))

    exported = onnx.load_model(str(output_model), load_external_data=True)
    onnx_sha256 = _sha256(output_model)
    checkpoint_sha256 = _sha256(checkpoint)
    input_name = exported.graph.input[0].name if exported.graph.input else "input"
    output_name = exported.graph.output[0].name if exported.graph.output else "logits"
    input_shape = _value_info_shape(exported.graph.input[0]) if exported.graph.input else [1, 3, 96, 96]
    output_shape = _value_info_shape(exported.graph.output[0]) if exported.graph.output else [1, num_classes]
    created_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")

    metadata: dict[str, Any] = {
        "schema_version": "model-metadata-v1",
        "model_id": "blank_squeezenet_template_seed42",
        "model_name": "Blank SqueezeNet starter (ImageNet-start)",
        "version": created_at[:10],
        "created_at": created_at,
        "status": "imagenet_transfer_start",
        "training_code_version": {
            "repo": "OpenDSS",
            "commit": None,
            "dirty": None,
            "training_entrypoint": "training/python/droplet_trainer/train.py",
            "training_script_legacy": DEFAULT_LEGACY_TRAIN_SCRIPT,
        },
        "artifact": {
            "onnx_file": output_model.name,
            "onnx_sha256": onnx_sha256,
            "metadata_sha256": None,
            "external_data_files": [],
            "format": "onnx",
            "opset": opset,
            "input_tensor": {
                "name": input_name,
                "shape": input_shape,
                "dtype": "float32",
                "layout": "NCHW",
            },
            "output_tensor": {
                "name": output_name,
                "shape": output_shape,
                "dtype": "float32",
                "score_type": "logits",
            },
        },
        "label_schema_version": "droplet-labels-target-nontarget-binary-v1",
        "classes": ["0", "1"],
        "class_to_idx": {"0": 0, "1": 1},
        "display_labels": {"0": "Non-target", "1": "Target"},
        "aliases": {
            "0": [
                "Non-target",
                "non-target",
                "Nontarget",
                "Empty",
                "empty",
                "Waste",
                "waste",
                "MoreThanTwo",
                "MoreThan2",
                "More than two",
                ">2",
                "2",
                "Multiple",
            ],
            "1": ["Target", "target", "Single", "single", "Hit", "Hits", "hit", "hits"],
        },
        "sorting_policy": {
            "target_class_id": "1",
            "target_display_label": "Target",
            "waste_class_id": "0",
            "waste_display_label": "Non-target",
            "trigger_rule": "trigger_on_target_class",
        },
        "input_size": [96, 96, 3],
        "normalization": {
            "scale": "uint8_div_255_or_uint16_div_65535",
            "color_order": "RGB",
            "mean": [0.485, 0.456, 0.406],
            "std": [0.229, 0.224, 0.225],
        },
        "preprocessing": {
            "resize": {"width": 96, "height": 96, "method": "linear"},
            "crop": "runtime_detector_crop_then_resize",
            "training_augmentation_summary": [],
        },
        "architecture": {"family": "SqueezeNet", "variant": "1_1", "num_classes": num_classes},
        "initialization": {
            "source": "TorchVision SqueezeNet1_1_Weights.DEFAULT / ImageNet",
            "source_checkpoint": recorded_checkpoint,
            "source_checkpoint_runtime_path": str(checkpoint),
            "source_checkpoint_sha256": checkpoint_sha256,
            "backbone_weights_preserved": True,
            "classifier_head_reset": True,
            "classifier_head_num_classes": num_classes,
            "classifier_head_init": f"torch_default_after_manual_seed_{seed}",
            "not_droplet_trained": True,
        },
        "training_config": {
            "architecture": "squeezenet1_1",
            "pretrained": True,
            "source_checkpoint": recorded_checkpoint,
            "export_onnx": True,
            "onnx_opset": opset,
            "seed": seed,
            "generation_method": "load_exact_torchvision_imagenet_checkpoint_then_reset_binary_head",
            "loss": None,
            "imbalance": {
                "mode": "none",
                "class_weight_formula": None,
                "computed_from": None,
                "class_weights": {"0": None, "1": None},
            },
        },
        "dataset_summary": {
            "dataset_id": None,
            "dataset_manifest": None,
            "dataset_manifest_sha256": None,
            "class_counts": {"0": 0, "1": 0},
            "train_counts": {"0": 0, "1": 0},
            "val_counts": {"0": 0, "1": 0},
            "test_counts": {"0": 0, "1": 0},
            "bundled_seed_images_used": False,
            "bundled_seed_manifest": None,
            "rejected_count": 0,
            "excluded_count": 0,
            "seed_or_existing_waste_counts": {},
        },
        "validation_summary": {
            "image_validation": {
                "status": "not_run",
                "run_id": None,
                "dataset_manifest_sha256": None,
                "accuracy": None,
                "macro_f1": None,
                "minimum_thresholds": {},
            },
            "sequence_validation": {
                "status": "not_available",
                "run_id": None,
                "sequence_manifest_sha256": None,
                "summary_file": None,
                "minimum_thresholds": {},
            },
        },
        "provenance": {
            "training_command": [],
            "export_command": [
                "python app/runtime/scripts/generate_blank_squeezenet.py",
                f"--checkpoint={checkpoint.as_posix()}",
                f"--checkpoint-record-path={recorded_checkpoint}",
                f"--output-model={output_model.as_posix()}",
                f"--output-metadata={output_metadata.as_posix()}",
                f"--seed={seed}",
                f"--num-classes={num_classes}",
                f"--onnx-opset={opset}",
            ],
            "validation_commands": [],
            "environment_file": None,
            "python_version": sys.version.split()[0],
            "package_versions": {
                "torch": torch.__version__,
                "torchvision": torchvision.__version__,
                "onnx": onnx.__version__,
            },
            "hardware": {},
            "random_seed": seed,
            "split_manifest": None,
            "created_by": "generate_blank_squeezenet.py",
            "notes": [
                "Backbone weights were loaded from the exact TorchVision SqueezeNet 1.1 ImageNet checkpoint cache file.",
                "The 1000-class classifier head was replaced with a new 2-class head to match the OpenDSS training-start flow.",
                "This starter is not droplet-trained and must be trained and validated before any live sorting use.",
                "Public redistribution of TorchVision/ImageNet weights should be reviewed before public release packaging.",
            ],
        },
        "promotion": {
            "promotion_gate_status": "not_evaluated",
            "rejection_reasons": [
                "imagenet_transfer_start",
                "training_starter_only",
                "not_validated_for_live_sorting",
            ],
            "reviewed_by": None,
            "reviewed_at": None,
            "promotion_record": None,
        },
        "export": {
            "format": "onnx",
            "opset": opset,
            "sha256": onnx_sha256,
        },
        "limitations": [
            "ImageNet-start training starter only; not droplet-trained.",
            "Do not use this starter for live sorting or DAQ-triggered operation without separate training and validation.",
            "Public redistribution of TorchVision/ImageNet weights should be reviewed before public release packaging.",
        ],
    }

    output_metadata.parent.mkdir(parents=True, exist_ok=True)
    output_metadata.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    metadata_sha256 = _sha256(output_metadata)

    return {
        "checkpoint_path": str(checkpoint),
        "checkpoint_record_path": recorded_checkpoint,
        "checkpoint_sha256": checkpoint_sha256,
        "output_model": str(output_model),
        "output_metadata": str(output_metadata),
        "model_sha256": onnx_sha256,
        "metadata_sha256": metadata_sha256,
        "seed": seed,
        "num_classes": num_classes,
        "opset": opset,
        "backbone_weights_preserved": True,
        "classifier_head_reset": True,
        "classifier_head_num_classes": num_classes,
    }


def main() -> int:
    default_model, default_metadata = _default_paths(Path(__file__))
    parser = argparse.ArgumentParser(
        description="Generate the packaged ImageNet-start SqueezeNet ONNX starter used by Add blank model."
    )
    parser.add_argument("--checkpoint", default=DEFAULT_CHECKPOINT)
    parser.add_argument("--checkpoint-record-path", default=DEFAULT_CHECKPOINT)
    parser.add_argument("--output-model", default=str(default_model))
    parser.add_argument("--output-metadata", default=str(default_metadata))
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--num-classes", type=int, default=2)
    parser.add_argument("--onnx-opset", type=int, default=18)
    args = parser.parse_args()

    result = generate_blank_model(
        checkpoint_path=Path(args.checkpoint),
        checkpoint_record_path=str(args.checkpoint_record_path),
        output_model=Path(args.output_model).resolve(),
        output_metadata=Path(args.output_metadata).resolve(),
        seed=int(args.seed),
        num_classes=int(args.num_classes),
        opset=int(args.onnx_opset),
    )
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
