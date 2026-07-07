from __future__ import annotations

import csv
import json
import math
import time
from collections import Counter
from pathlib import Path
from typing import Any

from .dataset import scan_dataset, utc_now, write_csv
from .errors import EXIT_MISSING_PACKAGE, EXIT_SCHEMA_MISMATCH
from .metadata import validate_metadata
from .schema import ClassSchema


def _dependency_missing() -> str | None:
    try:
        import onnxruntime  # noqa: F401
        import PIL  # noqa: F401
        import numpy  # noqa: F401
    except Exception as exc:
        return str(exc)
    return None


def _normalization(metadata_status: dict[str, Any], metadata_path: Path) -> dict[str, Any]:
    with metadata_path.open("r", encoding="utf-8") as handle:
        metadata = json.load(handle)
    normalization = metadata.get("normalization") if isinstance(metadata.get("normalization"), dict) else {}
    input_size = metadata.get("input_size", [96, 96, 3])
    if not isinstance(input_size, list) or len(input_size) < 2:
        input_size = [96, 96, 3]
    mean = normalization.get("mean", [0.485, 0.456, 0.406])
    std = normalization.get("std", [0.229, 0.224, 0.225])
    if not isinstance(mean, list) or len(mean) != 3:
        mean = [0.485, 0.456, 0.406]
    if not isinstance(std, list) or len(std) != 3:
        std = [0.229, 0.224, 0.225]
    return {
        "classes": metadata_status["classes"],
        "input_size": [int(input_size[0]), int(input_size[1])],
        "mean": [float(value) for value in mean],
        "std": [float(value) if float(value) != 0 else 1.0 for value in std],
    }


def _preprocess_image(path: Path, preprocessing: dict[str, Any]) -> Any:
    import numpy as np
    from PIL import Image

    width, height = preprocessing["input_size"][0], preprocessing["input_size"][1]
    with Image.open(path) as image:
        image = image.convert("RGB")
        image = image.resize((width, height), Image.BILINEAR)
        array = np.asarray(image, dtype=np.float32)
    array /= 255.0
    mean = np.asarray(preprocessing["mean"], dtype=np.float32)
    std = np.asarray(preprocessing["std"], dtype=np.float32)
    array = (array - mean) / std
    return np.transpose(array, (2, 0, 1))[None, :, :, :].astype(np.float32)


def _softmax(values: Any) -> list[float]:
    import numpy as np

    logits = np.asarray(values, dtype=np.float64).reshape(-1)
    logits = logits - np.max(logits)
    exp = np.exp(logits)
    total = float(np.sum(exp))
    if total <= 0 or not math.isfinite(total):
        return [0.0 for _ in logits]
    return [float(value / total) for value in exp]


def _safe_divide(numerator: float, denominator: float) -> float | None:
    if denominator == 0:
        return None
    return numerator / denominator


def _format_metric(value: float | None) -> str:
    return "" if value is None else f"{value:.12g}"


def _metrics(labels: list[str], truth: list[str], predicted: list[str]) -> tuple[list[dict[str, Any]], dict[str, Any], dict[str, dict[str, int]]]:
    total = len(truth)
    correct = sum(1 for actual, pred in zip(truth, predicted) if actual == pred)
    confusion: dict[str, dict[str, int]] = {label: {pred: 0 for pred in labels} for label in labels}
    for actual, pred in zip(truth, predicted):
        if actual in confusion and pred in confusion[actual]:
            confusion[actual][pred] += 1

    rows: list[dict[str, Any]] = []
    precision_values: list[float] = []
    recall_values: list[float] = []
    f1_values: list[float] = []
    for label in labels:
        tp = confusion[label][label]
        fp = sum(confusion[other][label] for other in labels if other != label)
        fn = sum(confusion[label][other] for other in labels if other != label)
        tn = total - tp - fp - fn
        precision = _safe_divide(tp, tp + fp)
        recall = _safe_divide(tp, tp + fn)
        f1 = None if precision is None or recall is None or precision + recall == 0 else 2 * precision * recall / (precision + recall)
        accuracy_one_vs_rest = _safe_divide(tp + tn, total)
        if precision is not None:
            precision_values.append(precision)
        if recall is not None:
            recall_values.append(recall)
        if f1 is not None:
            f1_values.append(f1)
        rows.append(
            {
                "label": label,
                "support": sum(confusion[label].values()),
                "true_positive": tp,
                "false_positive": fp,
                "false_negative": fn,
                "true_negative": tn,
                "precision": _format_metric(precision),
                "recall": _format_metric(recall),
                "f1": _format_metric(f1),
                "accuracy_one_vs_rest": _format_metric(accuracy_one_vs_rest),
            }
        )
    aggregate = {
        "accuracy": _safe_divide(correct, total),
        "macro_precision": _safe_divide(sum(precision_values), len(precision_values)),
        "macro_recall": _safe_divide(sum(recall_values), len(recall_values)),
        "macro_f1": _safe_divide(sum(f1_values), len(f1_values)),
        "samples_total": total,
        "samples_correct": correct,
        "samples_incorrect": total - correct,
    }
    return rows, aggregate, confusion


def run_validate_images(args: Any, schema: ClassSchema) -> tuple[dict[str, Any], int]:
    metadata_path = Path(args.metadata).expanduser().resolve()
    model_path = Path(args.model).expanduser().resolve()
    output = Path(args.output).expanduser().resolve()
    metadata_status = validate_metadata(metadata_path, promotion_gate=args.promotion_gate)
    scan = scan_dataset(args.dataset, schema)
    missing_dependency = _dependency_missing()
    if missing_dependency:
        payload = {
            "schema_version": 1,
            "command": "validate-images",
            "status": "error",
            "timestamp": utc_now(),
            "error": {
                "code": "VALIDATION_DEPENDENCY_UNAVAILABLE",
                "message": "ONNX image validation requires onnxruntime, numpy, and Pillow. No inference was run.",
                "details": {"error": missing_dependency},
            },
            "metadata": metadata_status,
            "dataset": {"path": str(scan["dataset_root"]), "samples_total": len(scan["items"]), "class_counts": dict(scan["counts"])},
        }
        return payload, EXIT_MISSING_PACKAGE
    if not metadata_status["valid"]:
        payload = {
            "schema_version": 1,
            "command": "validate-images",
            "status": "error",
            "timestamp": utc_now(),
            "error": {"code": "METADATA_INVALID", "message": "Metadata is inconsistent or invalid.", "details": metadata_status},
        }
        return payload, EXIT_SCHEMA_MISMATCH
    labels = [str(label) for label in metadata_status["classes"]]
    if labels != schema.classes:
        payload = {
            "schema_version": 1,
            "command": "validate-images",
            "status": "error",
            "timestamp": utc_now(),
            "error": {
                "code": "MODEL_DATASET_SCHEMA_MISMATCH",
                "message": "Metadata class order does not match the validation class schema.",
                "details": {"metadata_classes": labels, "dataset_schema_classes": schema.classes},
            },
            "metadata": metadata_status,
            "dataset": {"path": str(scan["dataset_root"]), "samples_total": len(scan["items"]), "class_counts": dict(scan["counts"])},
        }
        return payload, EXIT_SCHEMA_MISMATCH

    output.mkdir(parents=True, exist_ok=True)
    image_dir = output / "image_validation"
    image_dir.mkdir(parents=True, exist_ok=True)
    predictions_path = image_dir / "predictions.csv"
    confusion_path = image_dir / "confusion_matrix.csv"
    class_metrics_path = image_dir / "class_metrics.csv"
    failure_cases_path = image_dir / "failure_cases.csv"
    summary_path = image_dir / "validation_summary.json"

    import numpy as np
    import onnxruntime as ort

    providers = ["CPUExecutionProvider"]
    if args.device == "cuda" and "CUDAExecutionProvider" in ort.get_available_providers():
        providers = ["CUDAExecutionProvider", "CPUExecutionProvider"]
    elif args.device == "cuda":
        return {
            "schema_version": 1,
            "command": "validate-images",
            "status": "error",
            "timestamp": utc_now(),
            "error": {
                "code": "VALIDATION_DEVICE_UNAVAILABLE",
                "message": "CUDA validation was requested but ONNX Runtime CUDA provider is unavailable.",
                "details": {"available_providers": ort.get_available_providers()},
            },
        }, EXIT_MISSING_PACKAGE

    session = ort.InferenceSession(str(model_path), providers=providers)
    input_meta = session.get_inputs()[0]
    input_name = input_meta.name
    preprocessing = _normalization(metadata_status, metadata_path)

    prediction_rows: list[dict[str, Any]] = []
    failure_rows: list[dict[str, Any]] = []
    truth: list[str] = []
    predicted: list[str] = []
    errors: list[dict[str, Any]] = []
    latencies: list[float] = []

    for index, item in enumerate(scan["items"]):
        image_path = Path(item["source_path"])
        sample_id = item.get("image_id") or f"sample-{index:06d}"
        true_label = str(item["class_id"])
        row: dict[str, Any] = {
            "sample_id": sample_id,
            "image_path": str(image_path),
            "true_label": true_label,
            "split": item.get("role", "unknown"),
        }
        try:
            tensor = _preprocess_image(image_path, preprocessing)
            start = time.perf_counter()
            outputs = session.run(None, {input_name: tensor})
            latency_ms = (time.perf_counter() - start) * 1000.0
            scores = _softmax(np.asarray(outputs[0]).reshape(-1))
            if len(scores) != len(labels):
                raise ValueError(f"Model returned {len(scores)} scores for {len(labels)} metadata classes.")
            pred_index = int(np.argmax(scores))
            pred_label = labels[pred_index]
            sorted_scores = sorted(scores, reverse=True)
            margin = sorted_scores[0] - sorted_scores[1] if len(sorted_scores) > 1 else sorted_scores[0]
            row.update(
                {
                    "pred_label": pred_label,
                    "pred_index": pred_index,
                    "correct": true_label == pred_label,
                    "confidence": f"{scores[pred_index]:.12g}",
                    "margin": f"{margin:.12g}",
                    "latency_ms": f"{latency_ms:.6f}",
                    "error_code": "",
                }
            )
            for label, score in zip(labels, scores):
                row[f"score_{label}"] = f"{score:.12g}"
            truth.append(true_label)
            predicted.append(pred_label)
            latencies.append(latency_ms)
            if true_label != pred_label:
                failure_rows.append(
                    {
                        "rank": 0,
                        "sample_id": sample_id,
                        "image_path": str(image_path),
                        "true_label": true_label,
                        "pred_label": pred_label,
                        "confidence": f"{scores[pred_index]:.12g}",
                        "margin": f"{margin:.12g}",
                        "latency_ms": f"{latency_ms:.6f}",
                        "notes": "",
                    }
                )
        except Exception as exc:
            error_code = "IMAGE_INFERENCE_FAILED"
            row.update({"pred_label": "", "pred_index": "", "correct": "", "confidence": "", "margin": "", "latency_ms": "", "error_code": error_code})
            for label in labels:
                row[f"score_{label}"] = ""
            errors.append({"code": error_code, "message": "Image could not be preprocessed or evaluated.", "details": {"path": str(image_path), "error": str(exc)}})
        prediction_rows.append(row)

    class_metrics, aggregate_metrics, confusion = _metrics(labels, truth, predicted)
    failure_rows.sort(key=lambda row: float(row["confidence"]) if row["confidence"] else 0.0, reverse=True)
    for rank, row in enumerate(failure_rows, start=1):
        row["rank"] = rank

    prediction_fields = ["sample_id", "image_path", "true_label", "pred_label", "pred_index", "correct", "confidence", "margin", "latency_ms", "split", "error_code"] + [f"score_{label}" for label in labels]
    with predictions_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=prediction_fields)
        writer.writeheader()
        for row in prediction_rows:
            writer.writerow({field: row.get(field, "") for field in prediction_fields})
    write_csv(confusion_path, [{"true_label": label, **{f"pred_{pred}": confusion[label][pred] for pred in labels}} for label in labels], ["true_label"] + [f"pred_{label}" for label in labels])
    write_csv(class_metrics_path, class_metrics, ["label", "support", "true_positive", "false_positive", "false_negative", "true_negative", "precision", "recall", "f1", "accuracy_one_vs_rest"])
    write_csv(failure_cases_path, failure_rows, ["rank", "sample_id", "image_path", "true_label", "pred_label", "confidence", "margin", "latency_ms", "notes"])

    latency_summary = {
        "mean_ms": _safe_divide(sum(latencies), len(latencies)),
        "min_ms": min(latencies) if latencies else None,
        "max_ms": max(latencies) if latencies else None,
    }
    split_counts = {split: dict(Counter(item["class_id"] for item in scan["items"] if item.get("role", "unknown") == split)) for split in ["train", "val", "test", "unknown"]}
    status = "ok" if not errors else "completed_with_errors"
    summary = {
        "schema_version": "validator.v1",
        "run_id": output.name,
        "mode": "image",
        "status": status,
        "created_at": utc_now(),
        "model": {"model_path": str(model_path), "metadata_path": str(metadata_path), "model_id": None},
        "dataset": {
            "dataset_path": str(scan["dataset_root"]),
            "samples_total": len(scan["items"]),
            "samples_evaluated": len(truth),
            "samples_failed": len(errors),
            "class_counts": dict(scan["counts"]),
            "split_counts": split_counts,
        },
        "labels": labels,
        "metrics": aggregate_metrics,
        "latency": latency_summary,
        "promotion_gate_allowed": metadata_status["mode"] == "schema_valid",
        "artifacts": {
            "predictions_csv": str(predictions_path),
            "confusion_matrix_csv": str(confusion_path),
            "class_metrics_csv": str(class_metrics_path),
            "failure_cases_csv": str(failure_cases_path),
        },
        "warnings": metadata_status["warnings"],
        "errors": errors,
    }
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return {
        "schema_version": 1,
        "command": "validate-images",
        "status": status,
        "timestamp": utc_now(),
        "message": "Image validation inference completed.",
        "summary_path": str(summary_path),
        "metadata": metadata_status,
        "dataset": {"path": str(scan["dataset_root"]), "samples_total": len(scan["items"]), "samples_evaluated": len(truth), "class_counts": dict(scan["counts"])},
        "metrics": aggregate_metrics,
    }, 0
