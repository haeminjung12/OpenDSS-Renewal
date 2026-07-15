from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

from .schema import DEFAULT_BINARY_CLASSES, LEGACY_CLASSES, default_aliases_for_classes, default_display_labels_for_classes, default_schema_id_for_classes


def sorting_policy_for_classes(classes: list[str], display_labels: dict[str, str]) -> dict[str, Any]:
    target_class_id = "1" if "1" in classes else None
    target_display_label = display_labels.get(target_class_id, target_class_id) if target_class_id else None
    sorting_policy: dict[str, Any] = {
        "mode": "trigger_on_target_class" if target_class_id else "project_configuration_required",
        "target_class_id": target_class_id,
        "target_display_label": target_display_label,
        "non_target_class_ids": [class_id for class_id in classes if class_id != target_class_id],
        "trigger_rule": "trigger_on_target_class" if target_class_id else "project_configuration_required",
    }
    if classes == DEFAULT_BINARY_CLASSES:
        sorting_policy["waste_class_id"] = "0"
        sorting_policy["waste_display_label"] = display_labels.get("0", "Non-target")
    else:
        sorting_policy["waste_class_id"] = None
        sorting_policy["waste_display_label"] = None
    return sorting_policy


def metadata_scaffold(classes: list[str], display_labels: dict[str, str]) -> dict[str, Any]:
    display_labels = {class_id: str(display_labels.get(class_id, default_display_labels_for_classes(classes).get(class_id, class_id))) for class_id in classes}
    class_to_idx = {class_id: index for index, class_id in enumerate(classes)}
    aliases = {key: list(value) for key, value in default_aliases_for_classes(classes).items()}
    sorting_policy = sorting_policy_for_classes(classes, display_labels)
    label_schema_version = default_schema_id_for_classes(classes)
    return {
        "schema_version": "model-metadata-v1",
        "model_id": None,
        "model_name": None,
        "version": None,
        "created_at": None,
        "status": "candidate",
        "training_code_version": {
            "repo": None,
            "commit": None,
            "dirty": None,
            "training_entrypoint": "python -m droplet_trainer train",
            "training_script_legacy": None,
        },
        "artifact": {
            "onnx_file": "model.onnx",
            "onnx_sha256": None,
            "external_data_files": [],
            "metadata_sha256": None,
            "format": "onnx",
            "opset": None,
            "input_tensor": {"name": None, "shape": [1, 3, 96, 96], "dtype": "float32", "layout": "NCHW"},
            "output_tensor": {
                "name": None,
                "shape": [1, len(classes)],
                "dtype": "float32",
                "score_type": "logits",
            },
        },
        "label_schema_version": label_schema_version,
        "class_count": len(classes),
        "class_ids": list(classes),
        "classes": classes,
        "class_to_idx": class_to_idx,
        "display_labels": display_labels,
        "aliases": aliases,
        "sorting_policy": sorting_policy,
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
        "architecture": {"family": "SqueezeNet", "variant": "1_1", "num_classes": len(classes)},
        "training_config": {
            "loss": "cross_entropy",
            "imbalance": {
                "mode": "class_weighted_loss",
                "class_weight_formula": "inverse_count_min_normalized",
                "computed_from": "training_split_only",
                "class_weights": {class_id: None for class_id in classes},
            },
        },
        "dataset_summary": {
            "dataset_id": None,
            "dataset_manifest": None,
            "dataset_manifest_sha256": None,
            "class_counts": {class_id: 0 for class_id in classes},
            "train_counts": {class_id: 0 for class_id in classes},
            "val_counts": {class_id: 0 for class_id in classes},
            "test_counts": {class_id: 0 for class_id in classes},
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
            "export_command": [],
            "validation_commands": [],
            "environment_file": "environment.json",
            "python_version": None,
            "package_versions": {},
            "hardware": {},
            "random_seed": None,
            "split_manifest": "split_manifest.json",
            "created_by": "droplet_trainer_scaffold",
            "notes": [],
        },
        "promotion": {
            "promotion_gate_status": "not_evaluated",
            "rejection_reasons": [],
            "reviewed_by": None,
            "reviewed_at": None,
            "promotion_record": None,
        },
        "limitations": [],
    }


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _present_onnx_external_data_files(metadata_dir: Path, onnx_file: str | None) -> list[str]:
    if not onnx_file:
        return sorted(path.name for path in metadata_dir.glob("*.onnx.data") if path.is_file())
    exact_sidecar = metadata_dir / f"{onnx_file}.data"
    if exact_sidecar.is_file():
        return [exact_sidecar.name]
    return []


def validate_metadata(path: Path, promotion_gate: bool = False) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        metadata = json.load(handle)
    classes = metadata.get("classes")
    class_to_idx = metadata.get("class_to_idx")
    warnings: list[str] = []
    errors: list[str] = []
    rejection_reasons: list[str] = []
    mode = "schema_valid"
    schema_version = metadata.get("schema_version")

    if not isinstance(classes, list) or not classes:
        errors.append("metadata.classes is missing or empty")
        rejection_reasons.append("missing_or_empty_classes")
    if class_to_idx is not None:
        expected = {str(class_id): index for index, class_id in enumerate(classes or [])}
        found = {str(key): value for key, value in class_to_idx.items()} if isinstance(class_to_idx, dict) else {}
        if found != expected:
            message = "metadata class_to_idx does not match ordered classes"
            rejection_reasons.append("class_to_idx_mismatch")
            if schema_version == "model-metadata-v1" or promotion_gate:
                errors.append(message)
            else:
                warnings.append(message)
                mode = "legacy_unvalidated"
    else:
        warnings.append("metadata.class_to_idx is missing; ordered classes will be used for legacy inspection")
        mode = "legacy_unvalidated"
        rejection_reasons.append("missing_class_to_idx")

    if schema_version != "model-metadata-v1":
        warnings.append("metadata.schema_version is missing or is not model-metadata-v1")
        mode = "legacy_unvalidated"
        rejection_reasons.append("missing_model_metadata_v1_schema")

    label_schema_version = metadata.get("label_schema_version")
    default_display_labels = default_display_labels_for_classes([str(class_id) for class_id in (classes or [])])
    display_labels_raw = metadata.get("display_labels", {})
    display_labels = dict(default_display_labels)
    if isinstance(display_labels_raw, dict):
        for key, value in display_labels_raw.items():
            display_labels[str(key)] = str(value)
    sorting_policy = metadata.get("sorting_policy", {})
    if promotion_gate and classes == DEFAULT_BINARY_CLASSES:
        if not isinstance(sorting_policy, dict):
            errors.append("binary metadata sorting_policy is missing")
            rejection_reasons.append("missing_sorting_policy")
        else:
            if sorting_policy.get("target_class_id") != "1":
                errors.append("binary metadata sorting_policy.target_class_id must be '1'")
                rejection_reasons.append("target_class_not_1")
            if sorting_policy.get("waste_class_id") != "0":
                errors.append("binary metadata sorting_policy.waste_class_id must be '0'")
                rejection_reasons.append("waste_class_not_0")

    if any(str(class_id) in LEGACY_CLASSES for class_id in (classes or [])):
        warnings.append("legacy class ids are present")
        mode = "legacy_unvalidated"
        rejection_reasons.append("legacy_class_ids_present")
        if promotion_gate:
            errors.append("legacy class ids block promotion-gating conclusions")

    class_count = metadata.get("class_count")
    if class_count is not None:
        try:
            if int(class_count) != len(classes or []):
                message = "metadata.class_count does not match metadata.classes length"
                if schema_version == "model-metadata-v1" or promotion_gate:
                    errors.append(message)
                else:
                    warnings.append(message)
                rejection_reasons.append("class_count_mismatch")
        except (TypeError, ValueError):
            errors.append("metadata.class_count must be an integer when present")
            rejection_reasons.append("invalid_class_count")

    class_ids = metadata.get("class_ids")
    if class_ids is not None:
        normalized_class_ids = [str(value) for value in class_ids] if isinstance(class_ids, list) else []
        if normalized_class_ids != [str(class_id) for class_id in (classes or [])]:
            message = "metadata.class_ids does not match metadata.classes"
            if schema_version == "model-metadata-v1" or promotion_gate:
                errors.append(message)
            else:
                warnings.append(message)
            rejection_reasons.append("class_ids_mismatch")

    duplicates = sorted({str(class_id) for class_id in (classes or []) if [str(item) for item in (classes or [])].count(str(class_id)) > 1})
    if duplicates:
        errors.append("metadata.classes contains duplicate class ids")
        rejection_reasons.append("duplicate_classes")

    artifact = metadata.get("artifact", {})
    if schema_version == "model-metadata-v1" or promotion_gate:
        if not metadata.get("model_id"):
            errors.append("metadata.model_id is missing")
            rejection_reasons.append("missing_model_id")
        if not metadata.get("created_at"):
            errors.append("metadata.created_at is missing")
            rejection_reasons.append("missing_created_at")
        if not isinstance(artifact, dict) or not artifact.get("onnx_sha256"):
            errors.append("metadata.artifact.onnx_sha256 is missing")
            rejection_reasons.append("missing_onnx_sha256")
        if isinstance(artifact, dict):
            onnx_file = artifact.get("onnx_file")
            present_sidecars = _present_onnx_external_data_files(path.parent, str(onnx_file) if onnx_file else None)
            external_data_files = artifact.get("external_data_files")
            if present_sidecars and not external_data_files:
                errors.append("metadata.artifact.external_data_files is missing for present ONNX external data sidecar files")
                rejection_reasons.append("missing_external_data_files")
            elif external_data_files is not None:
                if not isinstance(external_data_files, list):
                    errors.append("metadata.artifact.external_data_files must be a list")
                    rejection_reasons.append("invalid_external_data_files")
                else:
                    recorded_filenames: set[str] = set()
                    for index, entry in enumerate(external_data_files):
                        if not isinstance(entry, dict):
                            errors.append(f"metadata.artifact.external_data_files[{index}] must be an object")
                            rejection_reasons.append("invalid_external_data_file_entry")
                            continue
                        filename = entry.get("filename")
                        sha256 = entry.get("sha256")
                        byte_size = entry.get("byte_size")
                        required = entry.get("required")
                        if not isinstance(filename, str) or not filename:
                            errors.append(f"metadata.artifact.external_data_files[{index}].filename is missing")
                            rejection_reasons.append("missing_external_data_filename")
                            continue
                        sidecar_relative = Path(filename)
                        if sidecar_relative.is_absolute() or ".." in sidecar_relative.parts:
                            errors.append(f"metadata.artifact.external_data_files[{index}].filename must be a candidate-relative filename")
                            rejection_reasons.append("invalid_external_data_filename")
                            continue
                        recorded_filenames.add(sidecar_relative.name)
                        if not isinstance(sha256, str) or len(sha256) != 64:
                            errors.append(f"metadata.artifact.external_data_files[{index}].sha256 must be a SHA-256 hex string")
                            rejection_reasons.append("invalid_external_data_sha256")
                        if not isinstance(byte_size, int) or byte_size < 0:
                            errors.append(f"metadata.artifact.external_data_files[{index}].byte_size must be a non-negative integer")
                            rejection_reasons.append("invalid_external_data_byte_size")
                        if not isinstance(required, bool):
                            errors.append(f"metadata.artifact.external_data_files[{index}].required must be true or false")
                            rejection_reasons.append("invalid_external_data_required")
                        sidecar_path = path.parent / sidecar_relative
                        if sidecar_path.is_file():
                            actual_size = sidecar_path.stat().st_size
                            actual_sha256 = _sha256_file(sidecar_path)
                            if isinstance(byte_size, int) and actual_size != byte_size:
                                errors.append(f"ONNX external data sidecar size mismatch for {filename}")
                                rejection_reasons.append("external_data_byte_size_mismatch")
                            if isinstance(sha256, str) and actual_sha256.lower() != sha256.lower():
                                errors.append(f"ONNX external data sidecar SHA-256 mismatch for {filename}")
                                rejection_reasons.append("external_data_sha256_mismatch")
                        elif required is True:
                            errors.append(f"required ONNX external data sidecar is missing: {filename}")
                            rejection_reasons.append("missing_required_external_data_file")
                        else:
                            warnings.append(f"optional ONNX external data sidecar is missing: {filename}")
                    unrecorded_sidecars = sorted(set(present_sidecars) - recorded_filenames)
                    if unrecorded_sidecars:
                        errors.append("present ONNX external data sidecar files are not recorded in metadata")
                        rejection_reasons.append("unrecorded_external_data_files")
        validation_summary = metadata.get("validation_summary")
        if not isinstance(validation_summary, dict):
            errors.append("metadata.validation_summary is missing")
            rejection_reasons.append("missing_validation_summary")
        if not metadata.get("limitations"):
            errors.append("metadata.limitations is empty")
            rejection_reasons.append("empty_limitations")

    output_shape = (artifact.get("output_tensor") or {}).get("shape") if isinstance(artifact, dict) else None
    if isinstance(output_shape, list) and output_shape:
        try:
            output_classes = int(output_shape[-1])
            if classes and output_classes != len(classes):
                errors.append("metadata output tensor class dimension does not match classes length")
                rejection_reasons.append("output_dimension_class_count_mismatch")
        except (TypeError, ValueError):
            warnings.append("metadata output tensor shape is not fully numeric")

    return {
        "valid": not errors,
        "mode": mode if not errors else "invalid",
        "classes": classes or [],
        "class_count": len(classes or []),
        "class_ids": [str(class_id) for class_id in (classes or [])],
        "display_labels": display_labels,
        "sorting_policy": sorting_policy if isinstance(sorting_policy, dict) else {},
        "target_class_id": sorting_policy.get("target_class_id") if isinstance(sorting_policy, dict) else None,
        "warnings": warnings,
        "errors": errors,
        "promotion_gate_allowed": promotion_gate and not errors and mode == "schema_valid",
        "promotion_rejection_reasons": sorted(set(rejection_reasons)) if promotion_gate else [],
    }
