r"""Convert one supported PyTorch checkpoint into an importable OpenDSS v2 package.

Run from any directory with the qualified training Python environment:

    python app/runtime/scripts/convert_pytorch_model_package.py ^
      --checkpoint C:\models\droplets.pth ^
      --architecture mobilenet_v3_small ^
      --name "Droplet Model" ^
      --output C:\models\DropletModel

The output folder is created only after ONNX export and metadata generation
complete. Import it in Model Library by selecting its ``metadata.json`` file.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import uuid
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
TRAINING_PYTHON = REPO_ROOT / "training" / "python"
sys.path.insert(0, str(TRAINING_PYTHON))

from droplet_trainer.train import _build_model, _checkpoint_output_count, _export_onnx


ARCHITECTURE_LABELS = {
    "mobilenet_v3_small": "MobileNetV3-Small",
    "efficientnet_b0": "EfficientNet-B0",
}
CLASSES = ["0", "1", "2"]
DISPLAY_LABELS = {"0": "Empty", "1": "Single", "2": "MoreThanOne"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def model_id(name: str, source_hash: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-") or "model"
    return f"opendss-{slug}-{source_hash[:12]}"


def load_checkpoint_weights(model, source: Path) -> None:
    import torch

    try:
        loaded = torch.load(source, map_location="cpu", weights_only=True)
    except Exception as exc:
        raise ValueError(
            "Checkpoint could not be loaded with the safe weights-only loader."
        ) from exc
    state = (
        loaded.get("model_state", loaded.get("state_dict", loaded))
        if isinstance(loaded, dict) else None
    )
    if not isinstance(state, dict):
        raise ValueError("Checkpoint does not contain a supported weights dictionary.")
    cleaned = {}
    for original_key, value in state.items():
        if not isinstance(original_key, str):
            raise ValueError("Checkpoint contains a non-string weight key.")
        key = original_key
        for prefix in ("module.", "model.", "net.", "1."):
            if key.startswith(prefix):
                key = key[len(prefix):]
        if not key.startswith("0."):
            cleaned[key] = value
    if _checkpoint_output_count(cleaned) != len(CLASSES):
        raise ValueError("Checkpoint must contain a supported three-class classifier head.")
    model.load_state_dict(cleaned, strict=True)


def metadata(name: str, architecture: str, source: Path, checkpoint: Path,
             onnx_model: Path, opset: int | None) -> dict:
    family = ARCHITECTURE_LABELS[architecture]
    source_hash = sha256(source)
    return {
        "schema_version": "model-metadata-v2",
        "model_id": model_id(name, source_hash),
        "model_name": name,
        "status": "trained",
        "architecture": {
            "id": architecture,
            "family": family,
            "num_classes": len(CLASSES),
        },
        "origin": "external_pytorch_checkpoint",
        "user_facing_label": family,
        "classes": CLASSES,
        "class_to_idx": {class_id: int(class_id) for class_id in CLASSES},
        "display_labels": DISPLAY_LABELS,
        "label_schema_version": "opendss-droplet-3class-v1",
        "sorting_policy": {
            "target_class_id": "1",
            "target_display_label": "Single",
            "waste_class_ids": ["0", "2"],
            "trigger_rule": "trigger_on_target_class",
        },
        "input_size": [96, 96, 3],
        "normalization": {
            "mean": [0.485, 0.456, 0.406],
            "std": [0.229, 0.224, 0.225],
        },
        "artifact": {
            "onnx_file": onnx_model.name,
            "checkpoint_file": checkpoint.name,
            "onnx_sha256": sha256(onnx_model),
            "checkpoint_sha256": sha256(checkpoint),
            "format": "onnx",
            "opset": opset or 18,
            "external_data_files": [],
            "input_tensor": {
                "name": "input",
                "shape": ["batch", 3, 96, 96],
                "dtype": "float32",
                "layout": "NCHW",
            },
            "output_tensor": {
                "name": "logits",
                "shape": ["batch", len(CLASSES)],
                "dtype": "float32",
                "score_type": "signed_logits",
            },
        },
        "initialization": {
            "mode": "checkpoint",
            "source_checkpoint_sha256": source_hash,
        },
        "provenance": {
            "converter": "app/runtime/scripts/convert_pytorch_model_package.py",
            "source_checkpoint_name": source.name,
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a complete OpenDSS v2 Model Package from a supported "
                    "three-class PyTorch checkpoint."
    )
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--architecture", choices=sorted(ARCHITECTURE_LABELS), required=True)
    parser.add_argument("--name", required=True, help="Nonblank Model Library display name.")
    parser.add_argument("--output", type=Path, required=True,
                        help="New package folder; it must not already exist.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source = args.checkpoint.expanduser().resolve()
    output = args.output.expanduser().resolve()
    name = args.name.strip()
    if not name:
        raise SystemExit("--name must be nonblank.")
    if not source.is_file():
        raise SystemExit(f"Checkpoint does not exist: {source}")
    if output.exists():
        raise SystemExit(f"Output already exists: {output}")

    output.parent.mkdir(parents=True, exist_ok=True)
    staging = output.parent / f".{output.name}.staging-{uuid.uuid4().hex}"
    staging.mkdir()
    try:
        config = {
            "architecture": args.architecture,
            "classes": list(CLASSES),
            "initialization": {"mode": "checkpoint"},
            "input_size": [96, 96, 3],
            "onnx_opset": 18,
            "seed": 42,
        }
        model = _build_model(config, len(CLASSES))
        load_checkpoint_weights(model, source)

        import torch

        checkpoint = staging / "checkpoint.pth"
        torch.save(
            {
                "model_state": model.state_dict(),
                "config": config,
                "class_ids": list(config["classes"]),
                "source_checkpoint_sha256": sha256(source),
            },
            checkpoint,
        )
        onnx_model, opset = _export_onnx(model, staging, config)
        package_metadata = metadata(
            name, args.architecture, source, checkpoint, onnx_model, opset
        )
        (staging / "metadata.json").write_text(
            json.dumps(package_metadata, indent=2) + "\n", encoding="utf-8"
        )
        os.replace(staging, output)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise

    print(json.dumps({"status": "complete", "package": str(output)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
