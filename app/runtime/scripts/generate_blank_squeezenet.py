from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _default_paths(script_path: Path) -> tuple[Path, Path, Path]:
    models_dir = script_path.resolve().parents[1] / "models"
    return (
        models_dir / "squeezenet_final_new_condition.onnx",
        models_dir / "blank_squeezenet_template.onnx",
        models_dir / "blank_squeezenet_template_metadata.json",
    )


def _randomize_initializer(array: Any, rng: Any, np: Any) -> Any:
    if not np.issubdtype(array.dtype, np.floating):
        return array
    if array.ndim <= 1:
        return np.zeros_like(array)
    fan_in = int(np.prod(array.shape[1:])) if array.shape[1:] else max(int(array.size), 1)
    bound = math.sqrt(6.0 / max(fan_in, 1))
    randomized = rng.uniform(-bound, bound, size=array.shape)
    return randomized.astype(array.dtype)


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


def generate_blank_model(source_model: Path, output_model: Path, output_metadata: Path, seed: int) -> dict[str, Any]:
    import numpy as np
    import onnx
    from onnx import numpy_helper

    model = onnx.load_model(str(source_model), load_external_data=True)
    original_initializers = list(model.graph.initializer)
    rng = np.random.default_rng(seed)

    rewritten = []
    randomized_names: list[str] = []
    for initializer in original_initializers:
        array = numpy_helper.to_array(initializer)
        updated = _randomize_initializer(array, rng, np)
        rewritten.append(numpy_helper.from_array(updated, initializer.name))
        if np.issubdtype(array.dtype, np.floating):
            randomized_names.append(initializer.name)

    del model.graph.initializer[:]
    model.graph.initializer.extend(rewritten)

    output_model.parent.mkdir(parents=True, exist_ok=True)
    onnx.save_model(model, str(output_model), save_as_external_data=False)
    onnx.checker.check_model(str(output_model))

    onnx_sha256 = _sha256(output_model)
    input_name = model.graph.input[0].name if model.graph.input else "input"
    output_name = model.graph.output[0].name if model.graph.output else "logits"
    input_shape = _value_info_shape(model.graph.input[0]) if model.graph.input else [1, 3, 96, 96]
    output_shape = _value_info_shape(model.graph.output[0]) if model.graph.output else [1, 2]

    metadata: dict[str, Any] = {
        "schema_version": "model-metadata-v1",
        "model_id": "blank_squeezenet_template_seed42",
        "model_name": "Blank SqueezeNet template (untrained)",
        "version": "2026-07-13",
        "created_at": "2026-07-13T00:00:00Z",
        "status": "blank_untrained_template",
        "training_code_version": {
            "repo": "OpenDSS",
            "commit": None,
            "dirty": None,
            "training_entrypoint": None,
            "training_script_legacy": None,
        },
        "artifact": {
            "onnx_file": output_model.name,
            "onnx_sha256": onnx_sha256,
            "metadata_sha256": None,
            "external_data_files": [],
            "format": "onnx",
            "opset": int(model.opset_import[0].version) if model.opset_import else None,
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
        "label_schema_version": "droplet-labels-binary-v1",
        "classes": ["0", "1"],
        "class_to_idx": {"0": 0, "1": 1},
        "display_labels": {"0": "Waste", "1": "Hits"},
        "aliases": {
            "0": ["Empty", "empty", "Waste", "waste", "MoreThanTwo", "MoreThan2", ">2", "2", "Multiple"],
            "1": ["Single", "single", "Hit", "Hits", "hit", "hits"],
        },
        "sorting_policy": {
            "target_class_id": "1",
            "target_display_label": "Hits",
            "waste_class_id": "0",
            "waste_display_label": "Waste",
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
        "architecture": {"family": "SqueezeNet", "variant": "1_1", "num_classes": 2},
        "training_config": {
            "architecture": "squeezenet1_1",
            "pretrained": False,
            "source_checkpoint": None,
            "export_onnx": True,
            "onnx_opset": int(model.opset_import[0].version) if model.opset_import else None,
            "seed": seed,
            "generation_method": "deterministic_reinitialized_release_graph",
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
                f"--seed={seed}",
                f"--source-model={source_model.as_posix()}",
                f"--output-model={output_model.as_posix()}",
                f"--output-metadata={output_metadata.as_posix()}",
            ],
            "validation_commands": [],
            "environment_file": None,
            "python_version": None,
            "package_versions": {},
            "hardware": {},
            "random_seed": seed,
            "split_manifest": None,
            "created_by": "generate_blank_squeezenet.py",
            "notes": [
                "Blank template generated by preserving the trained release graph topology and deterministically reinitializing floating-point parameters.",
                "This template is intentionally untrained and must not be treated as validated for live sorting.",
            ],
        },
        "promotion": {
            "promotion_gate_status": "not_evaluated",
            "rejection_reasons": ["blank_untrained_template", "not_validated_for_live_sorting"],
            "reviewed_by": None,
            "reviewed_at": None,
            "promotion_record": None,
        },
        "export": {
            "format": "onnx",
            "opset": int(model.opset_import[0].version) if model.opset_import else None,
            "sha256": onnx_sha256,
        },
        "limitations": [
            "Blank/untrained SqueezeNet template generated for packaging and future training bootstrap only.",
            "Do not use this template for live sorting or DAQ-triggered operation without separate training and validation.",
        ],
    }

    output_metadata.parent.mkdir(parents=True, exist_ok=True)
    output_metadata.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    return {
        "source_model": str(source_model),
        "output_model": str(output_model),
        "output_metadata": str(output_metadata),
        "model_sha256": onnx_sha256,
        "metadata_sha256": _sha256(output_metadata),
        "randomized_initializer_count": len(randomized_names),
        "seed": seed,
        "sidecars": [],
    }


def main() -> int:
    default_source, default_model, default_metadata = _default_paths(Path(__file__))
    parser = argparse.ArgumentParser(description="Generate a deterministic blank SqueezeNet ONNX template from the packaged trained graph.")
    parser.add_argument("--source-model", default=str(default_source))
    parser.add_argument("--output-model", default=str(default_model))
    parser.add_argument("--output-metadata", default=str(default_metadata))
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    result = generate_blank_model(
        source_model=Path(args.source_model).resolve(),
        output_model=Path(args.output_model).resolve(),
        output_metadata=Path(args.output_metadata).resolve(),
        seed=int(args.seed),
    )
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
