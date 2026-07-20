from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from statistics import mean, stdev
from typing import Any


SEED_FOLDS = {1729: 0, 2718: 1}
RECALL_GATE = 0.70
CORRECTED_FOLD_SHA256 = "e76a21689920fc26481017a7401896b13be599cd9c818d3c8fd62a5949be14f3"
STABILITY_CONDITIONS = {
    "historical_full_5e-4": {
        "classifier_output": "legacy_terminal_relu",
        "batch_size": 64,
        "stages": [{"name": "historical_full", "epochs": 8, "learning_rate": 5e-4, "trainable": "all"}],
    },
    "staged_relu_1e-4_1e-5": {"classifier_output": "legacy_terminal_relu"},
    "staged_signed_1e-4_1e-5": {"classifier_output": "signed_logits"},
}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def stable_hash(value: Any) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def create_immutable_execution_root(parent: Path, execution_id: str) -> Path:
    target = parent / execution_id
    target.mkdir(parents=True, exist_ok=False)
    return target


def load_inputs(prep: Path) -> tuple[dict, dict, dict, dict]:
    names = ("derived_manifest.json", "fold_manifest.json", "experiment_protocol.json", "condition_specs.json")
    values = tuple(json.loads((prep / name).read_text(encoding="utf-8")) for name in names)
    derived, folds, protocol, conditions = values
    if not derived.get("ready") or protocol.get("training_authorized") is not False or conditions.get("training_authorized") is not False:
        raise ValueError("Frozen preparation artifacts are not in the required ready/no-training state")
    return values


def assert_fold_isolation(fold: dict, records: dict[str, dict]) -> None:
    development, evaluation = set(fold["development"]), set(fold["evaluation"])
    if development & evaluation or development | evaluation != set(records):
        raise ValueError("Frozen fold is overlapping or incomplete")
    train_hashes = {records[key]["sha256"] for key in development}
    eval_hashes = {records[key]["sha256"] for key in evaluation}
    if train_hashes & eval_hashes:
        raise ValueError("Content hash crosses train/validation boundary")


def manifest_for_fold(derived: dict, fold: dict) -> dict:
    records = {item["record_id"]: item for item in derived["records"]}
    assert_fold_isolation(fold, records)
    items = []
    for role, identities in (("train", fold["development"]), ("validation", fold["evaluation"])):
        for identity in identities:
            record = records[identity]
            source = str(Path(record["source_root"]) / record["source_path"])
            items.append({"image_id": identity, "record_id": identity, "source_path": source, "path": source,
                          "content_sha256": record["sha256"],
                          "class_id": record["class_id"], "label": record["class_id"], "role": role,
                          "trainer_eligible": True, "review_state": "user_confirmed", "source_kind": "publication_manifest"})
    return {"schema_version": "publication-screen-manifest-v1", "dataset_id": "organoid_wellplate_combined",
            "classes": [{"id": key, "display_name": value} for key, value in derived["class_semantics"].items()],
            "items": items, "split_mode": "frozen_external", "validation_only": True}


def attach_frozen_contract(config: dict, manifest: dict, manifest_path: Path, prep: Path, fold: dict) -> dict:
    from droplet_trainer.train import _config_binding_hash, _identity_set_hash
    expected = {item["record_id"]: {"role": item["role"], "source_path": str(Path(item["source_path"]).resolve()), "class_id": item["class_id"], "sha256": item["content_sha256"]} for item in manifest["items"]}
    config["split_mode"] = "frozen_external"
    config["frozen_split_contract"] = {
        "authorized": True,
        "classes": ["0", "1", "2"],
        "manifest_sha256": sha256_file(manifest_path),
        "derived_manifest_sha256": sha256_file(prep / "derived_manifest.json"),
        "fold_manifest_sha256": sha256_file(prep / "fold_manifest.json"),
        "fold": int(fold["fold"]),
        "counts": {"train": len(fold["development"]), "validation": len(fold["evaluation"])},
        "train_identities_sha256": _identity_set_hash(list(fold["development"])),
        "validation_identities_sha256": _identity_set_hash(list(fold["evaluation"])),
        "items": expected,
    }
    config["frozen_split_contract"]["config_binding_sha256"] = _config_binding_hash(config)
    return config

def preflight(root: Path, repo: Path, python: Path, source_model: Path) -> dict:
    derived, folds_doc, protocol, conditions = load_inputs(root / "prep")
    records = {item["record_id"]: item for item in derived["records"]}
    bindings = []
    for condition in STABILITY_CONDITIONS:
        for seed, fold_index in SEED_FOLDS.items():
            fold = next(item for item in folds_doc["folds"] if int(item["fold"]) == fold_index)
            assert_fold_isolation(fold, records)
            manifest = manifest_for_fold(derived, fold)
            bindings.append({"condition": condition, "seed": seed, "fold": fold_index,
                             "development": len(fold["development"]), "evaluation": len(fold["evaluation"]),
                             "manifest_items": len(manifest["items"]), "accepted": True})
    fold = next(item for item in folds_doc["folds"] if int(item["fold"]) == 0)
    execution_id = "phase2b-preflight-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    execution_root = create_immutable_execution_root(root / "screening-readiness", execution_id)
    manifest = manifest_for_fold(derived, fold)
    dataset_root = execution_root / "valid-dataset"; (dataset_root / "metadata").mkdir(parents=True)
    manifest_path = dataset_root / "metadata" / "dataset_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    config = attach_frozen_contract(base_config("staged_signed_1e-4_1e-5", 1729, source_model), manifest, manifest_path, root / "prep", fold)
    config_path = execution_root / "valid-config.json"; config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    env = os.environ.copy(); env["PYTHONPATH"] = str(repo / "training" / "python")
    def invoke(name: str, dataset: Path, cfg: Path) -> tuple[subprocess.CompletedProcess, Path, list[dict]]:
        output = execution_root / name
        command = [str(python), "-m", "droplet_trainer", "train", "--dataset", str(dataset), "--output", str(output), "--config", str(cfg), "--device", "cuda", "--classes", "0,1,2", "--run-name", name, "--jsonl", "--preflight-only"]
        completed = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
        events_path = execution_root / f"{name}.jsonl"; events_path.write_text(completed.stdout, encoding="utf-8")
        return completed, events_path, parse_events(events_path)
    valid, valid_events_path, valid_events = invoke("valid", dataset_root, config_path)
    accepted = [event for event in valid_events if event.get("event") == "split_contract_accepted"]
    if valid.returncode != 0 or not accepted or any(event.get("event") in {"stage_started", "epoch_started"} for event in valid_events):
        raise RuntimeError("Actual trainer valid frozen-split preflight failed or entered training")
    summary = accepted[-1]["split_contract"]
    if summary["identity_counts"] != {"train": 3448, "validation": 863}:
        raise RuntimeError(f"Unexpected fold-0 preflight identities: {summary['identity_counts']}")
    corrupt_root = execution_root / "corrupt-dataset"; (corrupt_root / "metadata").mkdir(parents=True)
    corrupt = json.loads(json.dumps(manifest)); corrupt["items"][0]["class_id"] = "2" if corrupt["items"][0]["class_id"] != "2" else "1"
    corrupt_manifest = corrupt_root / "metadata" / "dataset_manifest.json"; corrupt_manifest.write_text(json.dumps(corrupt, indent=2), encoding="utf-8")
    corrupt_run, corrupt_events_path, corrupt_events = invoke("corrupt", corrupt_root, config_path)
    corrupt_errors = [event for event in corrupt_events if event.get("event") == "error"]
    if corrupt_run.returncode == 0 or not corrupt_errors or any(event.get("event") in {"stage_started", "epoch_started"} for event in corrupt_events):
        raise RuntimeError("Corrupt frozen-split preflight did not abort before training")
    result = {"status": "preflight-accepted", "execution_id": execution_id, "training_launched": False, "optimizer_steps": 0,
            "artifact_version": protocol.get("artifact_version"), "fold_manifest_sha256": protocol.get("fold_manifest_sha256"),
            "planned_bindings": bindings, "conditions_training_authorized": conditions.get("training_authorized"),
            "accepted_split_contract": summary, "valid_events": str(valid_events_path), "corrupt_events": str(corrupt_events_path),
            "corrupt_exit_code": corrupt_run.returncode, "corrupt_error": corrupt_errors[-1].get("error"), "model_created": False, "optimizer_created": False}
    evidence = execution_root / "preflight_result.json"
    evidence.write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


def execute_canary(root: Path, repo: Path, python: Path, source_model: Path) -> dict:
    derived, folds_doc, protocol, _ = load_inputs(root / "prep")
    fold = next(item for item in folds_doc["folds"] if int(item["fold"]) == 0)
    execution_id = "phase2b-canary-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    run_root = create_immutable_execution_root(root / "screening-readiness", execution_id)
    manifest = manifest_for_fold(derived, fold)
    dataset_root = run_root / "dataset"; (dataset_root / "metadata").mkdir(parents=True)
    manifest_path = dataset_root / "metadata" / "dataset_manifest.json"; manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    config = base_config("staged_signed_1e-4_1e-5", 1729, source_model)
    config["imbalance"] = {"mode": "effective_number", "beta": 0.999, "cap": 5.0, "class_weight_formula": "effective_number_capped", "computed_from": "training_split_only", "class_weights": {}}
    config["stages"] = [{"name": "canary_stage1", "epochs": 3, "learning_rate": 1e-4, "trainable": "classifier_and_last_fire_modules"}, {"name": "canary_stage2", "epochs": 2, "learning_rate": 1e-5, "trainable": "fine_tune"}]
    config = attach_frozen_contract(config, manifest, manifest_path, root / "prep", fold)
    config_path = run_root / "immutable_config.json"; config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    env = os.environ.copy(); env["PYTHONPATH"] = str(repo / "training" / "python")
    output = run_root / "trainer"; run_id = "canary"
    command = [str(python), "-m", "droplet_trainer", "train", "--dataset", str(dataset_root), "--output", str(output), "--config", str(config_path), "--device", "cuda", "--classes", "0,1,2", "--run-name", run_id, "--jsonl"]
    started = datetime.now(timezone.utc); completed = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace"); elapsed = (datetime.now(timezone.utc) - started).total_seconds()
    events_path = run_root / "events.jsonl"; events_path.write_text(completed.stdout, encoding="utf-8"); events = parse_events(events_path)
    trainer_dir = output / run_id; metrics_path = trainer_dir / "metrics.json"
    metrics_doc = json.loads(metrics_path.read_text(encoding="utf-8")) if metrics_path.exists() else {}
    metrics = metrics_doc.get("test", {}); diagnostics = metrics.get("logit_diagnostics", {})
    failure_path = trainer_dir / "failure_diagnostics.json"
    if not metrics and failure_path.exists(): metrics = json.loads(failure_path.read_text(encoding="utf-8"))["metrics"]; diagnostics = metrics.get("logit_diagnostics", {})
    history = metrics_doc.get("history", []) or [event.get("metrics", {}) for event in events if event.get("event") == "epoch_metrics"]
    updates = max((int(row.get("optimizer_updates", 0)) for row in history), default=0); delta = max((float(row.get("parameter_delta_l2", 0)) for row in history), default=0)
    recalls = [float(row["recall"]) for row in metrics.get("class_metrics", [])]
    telemetry = metrics.get("telemetry", {})
    checkpoints = {path.name: sha256_file(path) for path in (trainer_dir / "checkpoints").glob("*.pth")}
    predictions_path = trainer_dir / "validation_predictions.jsonl"
    recomputed = recompute_retained_metrics(predictions_path, metrics) if predictions_path.exists() and metrics else None
    accepted = completed.returncode == 0 and updates > 0 and delta > 0 and not diagnostics.get("failure_reasons") and len(recalls) == 3 and len(set(checkpoints.values())) >= 2 and telemetry.get("peak_cuda_memory_allocated", 0) > 0 and bool(recomputed and recomputed["agreement"])
    result = {"status": "accepted" if accepted else "failed", "execution_id": execution_id, "exit_code": completed.returncode, "elapsed_seconds": elapsed, "optimizer_updates": updates, "parameter_delta_l2": delta, "balanced_accuracy": metrics.get("balanced_accuracy"), "per_class_recall": recalls, "diagnostics": diagnostics, "telemetry": telemetry, "checkpoint_hashes": checkpoints, "retained_prediction_recomputation": recomputed, "events": str(events_path), "trainer_dir": str(trainer_dir), "config_sha256": sha256_file(config_path), "manifest_sha256": sha256_file(manifest_path), "fold_manifest_sha256": sha256_file(root / "prep" / "fold_manifest.json"), "publication_result": False}
    (run_root / "canary_result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    if not accepted: raise RuntimeError(f"Bounded CUDA canary failed readiness: {result}")
    return result


def recover_canary_result(run_root: Path, experiment_root: Path) -> dict:
    events_path = run_root / "events.jsonl"; events = parse_events(events_path)
    trainer_dir = run_root / "trainer" / "canary"
    failure = json.loads((trainer_dir / "failure_diagnostics.json").read_text(encoding="utf-8")); metrics = failure["metrics"]
    history = [event.get("metrics", {}) for event in events if event.get("event") == "epoch_metrics"]
    updates = max(int(row.get("optimizer_updates", 0)) for row in history); delta = max(float(row.get("parameter_delta_l2", 0)) for row in history)
    predictions = recompute_retained_metrics(trainer_dir / "validation_predictions.jsonl", metrics)
    checkpoints = {path.name: sha256_file(path) for path in (trainer_dir / "checkpoints").glob("*.pth")}
    config_path = run_root / "immutable_config.json"; manifest_path = run_root / "dataset" / "metadata" / "dataset_manifest.json"
    result = {"status": "failed", "execution_id": run_root.name, "exit_code": 40, "elapsed_seconds": metrics["telemetry"]["elapsed_seconds"], "optimizer_updates": updates, "parameter_delta_l2": delta, "balanced_accuracy": metrics["balanced_accuracy"], "per_class_recall": [row["recall"] for row in metrics["class_metrics"]], "diagnostics": metrics["logit_diagnostics"], "telemetry": metrics["telemetry"], "checkpoint_hashes": checkpoints, "retained_prediction_recomputation": predictions, "events": str(events_path), "trainer_dir": str(trainer_dir), "config_sha256": sha256_file(config_path), "manifest_sha256": sha256_file(manifest_path), "fold_manifest_sha256": sha256_file(experiment_root / "prep" / "fold_manifest.json"), "publication_result": False, "disposition": "readiness_canary_failed_missing_class_2; no matrix authorized"}
    (run_root / "canary_result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


def evaluate_source_baseline(derived: dict, fold: dict, source_model: Path, seed: int) -> dict:
    import torch
    import torch.nn as nn
    from torch.utils.data import DataLoader
    from droplet_trainer.schema import ClassSchema
    from droplet_trainer.train import (ManifestImageDataset, _build_model, _build_transforms,
                                       _classification_metrics, _evaluate, _load_source_artifact,
                                       _logit_diagnostics, _seed_everything)
    _seed_everything(seed)
    schema = ClassSchema(["0", "1", "2"], {"0": "Empty", "1": "Single", "2": "MoreThanOne"},
                         schema_id="publication-3class-v1")
    config = base_config("staged_relu_1e-4_1e-5", seed, source_model)
    _, eval_tf = _build_transforms(config)
    records = {item["record_id"]: item for item in derived["records"]}
    items = [{"source_path": str(Path(records[key]["source_root"]) / records[key]["source_path"]),
              "class_id": records[key]["class_id"]} for key in fold["evaluation"]]
    loader = DataLoader(ManifestImageDataset(items, schema.class_to_idx, eval_tf), batch_size=64,
                        shuffle=False, num_workers=0, pin_memory=True)
    device = torch.device("cuda")
    model = _build_model(config, 3)
    warnings = _load_source_artifact(model, str(source_model), None)
    model = model.to(device)
    evaluated = _evaluate(model, loader, nn.CrossEntropyLoss(), device)
    metrics = _classification_metrics(evaluated["targets"], evaluated["preds"], schema.classes)
    diagnostics = _logit_diagnostics(evaluated["logits"], evaluated["preds"], 3)
    passed, reasons = gate(metrics, diagnostics, 0, 1)
    reasons = [reason for reason in reasons if reason != "ZERO_OPTIMIZER_UPDATES"]
    return {"condition": "source_baseline", "seed": seed, "fold": int(fold["fold"]),
            "status": "passed" if not reasons else "rejected", "optimizer_updates": 0,
            "balanced_accuracy": metrics["balanced_accuracy"], "macro_f1": metrics["macro_f1"],
            "per_class": per_class_map(metrics), "confusion_matrix": metrics["confusion_matrix"],
            "logit_diagnostics": diagnostics, "gate_reasons": reasons, "source_load_warnings": warnings}


def base_config(condition: str, seed: int, source_model: Path) -> dict:
    config = {
        "schema_version": 2, "architecture": "squeezenet1_1", "classes": ["0", "1", "2"],
        "display_labels": {"0": "Empty", "1": "Single", "2": "MoreThanOne"},
        "seed": seed, "input_size": [96, 96, 3], "batch_size": 64,
        "normalization": {"mean": [0.485, 0.456, 0.406], "std": [0.229, 0.224, 0.225]},
        "num_workers": 0, "weight_decay": 1e-4, "patience": 5, "min_delta": 1e-6,
        "use_amp": True, "classifier_output": "legacy_terminal_relu", "classifier_initialization": "preserve_random", "source_model_path": str(source_model),
        "source_model_sha256": sha256_file(source_model),
        "export_onnx": False, "optimizer": {"name": "adam"},
        "scheduler": {"name": "reduce_on_plateau", "factor": .5, "patience": 2, "min_lr": 1e-7},
        "augmentation": {"random_resized_crop": True, "affine": True, "color_jitter": True, "horizontal_flip": False},
        "imbalance": {"mode": "none", "class_weight_formula": "inverse_count_min_normalized", "computed_from": "training_split_only", "class_weights": {}},
        "stages": [
            {"name": "stage1", "epochs": 20, "learning_rate": 1e-4, "trainable": "classifier_and_last_fire_modules"},
            {"name": "stage2", "epochs": 15, "learning_rate": 1e-5, "trainable": "fine_tune"},
        ],
    }
    config.update(STABILITY_CONDITIONS[condition])
    return config


def gate(metrics: dict, diagnostics: dict, exit_code: int, optimizer_updates: int) -> tuple[bool, list[str]]:
    reasons = []
    recalls = [float(row["recall"]) for row in per_class_map(metrics).values()]
    if exit_code != 0: reasons.append(f"TRAINER_EXIT_{exit_code}")
    if optimizer_updates <= 0: reasons.append("ZERO_OPTIMIZER_UPDATES")
    if len(recalls) != 3 or min(recalls, default=0) < RECALL_GATE: reasons.append("CLASS_RECALL_BELOW_0.70")
    reasons.extend(diagnostics.get("failure_reasons", []))
    return not reasons, sorted(set(reasons))


def per_class_map(metrics: dict) -> dict[str, dict]:
    return metrics.get("per_class") or {str(row["class"]): row for row in metrics.get("class_metrics", [])}


def parse_events(path: Path) -> list[dict]:
    events = []
    if not path.exists(): return events
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        try: events.append(json.loads(line))
        except json.JSONDecodeError: continue
    return events


def recompute_retained_metrics(predictions_path: Path, reported: dict) -> dict:
    from droplet_trainer.train import _classification_metrics, _logit_diagnostics
    rows = [json.loads(line) for line in predictions_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    targets = [int(row["class_id"]) for row in rows]
    preds = [int(row["predicted_class_id"]) for row in rows]
    logits = [row["logits"] for row in rows]
    computed = _classification_metrics(targets, preds, ["0", "1", "2"])
    diagnostics = _logit_diagnostics(logits, preds, 3)
    for key in ("accuracy", "balanced_accuracy", "macro_f1"):
        if abs(float(computed[key]) - float(reported.get(key, -999))) > 1e-12:
            raise ValueError(f"Retained prediction metric mismatch for {key}")
    if computed["confusion_matrix"] != reported.get("confusion_matrix") or diagnostics["prediction_distribution"] != reported.get("logit_diagnostics", {}).get("prediction_distribution"):
        raise ValueError("Retained prediction confusion/distribution mismatch")
    return {"rows": len(rows), "metrics": computed, "diagnostics": diagnostics, "agreement": True}


def enrich_failed_result(result: dict, events: list[dict]) -> dict:
    epochs = [event for event in events if event.get("event") == "epoch_metrics"]
    if epochs:
        last = epochs[-1]
        result["epochs_completed"] = len(epochs)
        result["optimizer_updates"] = max(int(event.get("metrics", {}).get("optimizer_updates", 0)) for event in epochs)
        validation = last.get("metrics", {}).get("validation", {})
        result["last_epoch_validation_balanced_accuracy"] = validation.get("balanced_accuracy")
        result["last_epoch_validation_macro_f1"] = validation.get("macro_f1")
        result["last_epoch_validation_per_class"] = per_class_map(validation)
    errors = [event for event in events if event.get("event") == "error"]
    if errors:
        error = errors[-1].get("error", {})
        result["trainer_error"] = error
        details = error.get("details", {})
        if details.get("diagnostics"):
            result["logit_diagnostics"] = details["diagnostics"]
    summaries = [event for event in events if event.get("event") == "dataset_summary"]
    if summaries:
        result["observed_split_counts"] = summaries[-1].get("split_counts", {})
        manifest_path = Path(summaries[-1]["dataset"]["manifest_path"])
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        expected = {role: dict(Counter(item["class_id"] for item in manifest["items"] if item.get("role") == role))
                    for role in ("train", "val", "test")}
        result["intended_split_counts"] = expected
        result["split_contract_preserved"] = result["observed_split_counts"] == expected
        if not result["split_contract_preserved"]:
            result.setdefault("gate_reasons", []).append("FROZEN_SPLIT_CONTRACT_OVERRIDDEN")
    result["evidence_version"] = 3
    return result


def execute_stability(root: Path, repo: Path, python: Path, source_model: Path, authorization: str,
                      expected_fold_sha256: str = CORRECTED_FOLD_SHA256) -> dict:
    prep = root / "prep"
    derived, folds_doc, protocol, _ = load_inputs(prep)
    actual_fold_sha256 = sha256_file(prep / "fold_manifest.json")
    if actual_fold_sha256 != expected_fold_sha256:
        raise ValueError(f"Corrected fold SHA-256 mismatch: expected {expected_fold_sha256}, got {actual_fold_sha256}")
    source_hash = sha256_file(source_model)
    execution = root / "screening"
    execution.mkdir(parents=True, exist_ok=True)
    auth = {"schema_version": 1, "phase": 3, "disposition": "relaunch-after-accepted-fold-manifest-v2",
            "authorized": True, "authorized_scope": "backend CUDA screening", "work_order": authorization,
            "phase_1d_report": "docs/worker-reports/publication-gpu-training-experiments-2026-07-18/phase-1d-fold-manifest-correction.md",
            "prior_stopped_authorization": str(execution / "execution_authorization.json"),
            "fold_manifest_sha256": actual_fold_sha256, "created_at": utc_now(), "frozen_inputs_unchanged": True}
    auth_path = execution / "execution_disposition_fold_manifest_v2.json"
    if auth_path.exists() and json.loads(auth_path.read_text(encoding="utf-8")).get("fold_manifest_sha256") != actual_fold_sha256:
        raise ValueError("Existing relaunch disposition does not match corrected fold hash")
    if not auth_path.exists(): auth_path.write_text(json.dumps(auth, indent=2), encoding="utf-8")
    records = {item["record_id"]: item for item in derived["records"]}
    baseline_results = []
    baseline_root = execution / "baselines"; baseline_root.mkdir(parents=True, exist_ok=True)
    for seed, fold_index in SEED_FOLDS.items():
        fold = next(item for item in folds_doc["folds"] if int(item["fold"]) == fold_index)
        target = baseline_root / f"source_baseline__seed{seed}__fold{fold_index}.json"
        if target.exists(): baseline = json.loads(target.read_text(encoding="utf-8"))
        else:
            baseline = evaluate_source_baseline(derived, fold, source_model, seed)
            baseline.update({"source_model": str(source_model), "source_model_sha256": source_hash,
                             "fold_manifest_sha256": actual_fold_sha256, "created_at": utc_now()})
            target.write_text(json.dumps(baseline, indent=2), encoding="utf-8")
        baseline_results.append(baseline)
    results = []
    for condition in STABILITY_CONDITIONS:
        for seed, fold_index in SEED_FOLDS.items():
            fold = next(item for item in folds_doc["folds"] if int(item["fold"]) == fold_index)
            assert_fold_isolation(fold, records)
            run_id = f"{condition}__seed{seed}__fold{fold_index}"
            run_root = execution / "runs" / run_id
            result_path = run_root / "screening_result.json"
            if result_path.exists():
                result = json.loads(result_path.read_text(encoding="utf-8"))
                if result.get("status") in {"passed", "rejected", "failed"}:
                    if int(result.get("evidence_version", 0)) < 3:
                        result = enrich_failed_result(result, parse_events(Path(result["events"])))
                        passed, reasons = gate({"per_class": result.get("last_epoch_validation_per_class", {})},
                                               result.get("logit_diagnostics", {}), int(result.get("exit_code", 1)),
                                               int(result.get("optimizer_updates", 0)))
                        result["gate_reasons"] = reasons
                        if not result.get("split_contract_preserved", True):
                            result["gate_reasons"] = sorted(set(result["gate_reasons"] + ["FROZEN_SPLIT_CONTRACT_OVERRIDDEN"]))
                        result_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
                    results.append(result); continue
            run_root.mkdir(parents=True, exist_ok=True)
            dataset_root = run_root / "dataset"
            (dataset_root / "metadata").mkdir(parents=True, exist_ok=True)
            manifest = manifest_for_fold(derived, fold)
            manifest_path = dataset_root / "metadata" / "dataset_manifest.json"
            manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
            config = attach_frozen_contract(base_config(condition, seed, source_model), manifest, manifest_path, prep, fold)
            config_path = run_root / "immutable_config.json"
            config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
            provenance = {"run_id": run_id, "condition": condition, "seed": seed, "fold": fold_index,
                          "created_at": utc_now(), "source_model": str(source_model), "source_model_sha256": source_hash,
                          "derived_manifest_sha256": sha256_file(prep / "derived_manifest.json"),
                          "fold_manifest_sha256": sha256_file(prep / "fold_manifest.json"),
                          "config_sha256": sha256_file(config_path), "authorization_sha256": sha256_file(auth_path),
                          "python": str(python), "platform": platform.platform(), "cuda_visible_devices": os.environ.get("CUDA_VISIBLE_DEVICES")}
            (run_root / "provenance.json").write_text(json.dumps(provenance, indent=2), encoding="utf-8")
            output = run_root / "trainer"
            command = [str(python), "-m", "droplet_trainer", "train", "--dataset", str(dataset_root), "--output", str(output),
                       "--config", str(config_path), "--device", "cuda", "--classes", "0,1,2", "--run-name", run_id, "--jsonl"]
            env = os.environ.copy(); env["PYTHONPATH"] = str(repo / "training" / "python")
            started = datetime.now(timezone.utc)
            completed = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
            elapsed = (datetime.now(timezone.utc) - started).total_seconds()
            events_path = run_root / "events.jsonl"; events_path.write_text(completed.stdout, encoding="utf-8")
            events = parse_events(events_path)
            trainer_dir = output / run_id
            metrics_path = trainer_dir / "metrics.json"
            metrics_doc = json.loads(metrics_path.read_text(encoding="utf-8")) if metrics_path.exists() else {}
            validation = metrics_doc.get("test", {})
            diagnostics = validation.get("logit_diagnostics", {})
            epochs = metrics_doc.get("history", [])
            updates = max((int(row.get("optimizer_updates", 0)) for row in epochs), default=0)
            passed, reasons = gate(validation, diagnostics, completed.returncode, updates)
            result = {**provenance, "status": "passed" if passed else "rejected", "exit_code": completed.returncode,
                      "elapsed_seconds": elapsed, "optimizer_updates": updates, "epochs_completed": len(epochs),
                      "balanced_accuracy": validation.get("balanced_accuracy"), "macro_f1": validation.get("macro_f1"),
                      "per_class": per_class_map(validation), "confusion_matrix": validation.get("confusion_matrix", []),
                      "prediction_distribution": diagnostics.get("prediction_distribution", {}), "logit_diagnostics": diagnostics,
                      "gate_reasons": reasons, "trainer_dir": str(trainer_dir), "events": str(events_path),
                      "checkpoint_hashes": {p.name: sha256_file(p) for p in (trainer_dir / "checkpoints").glob("*.pth")},
                      "event_count": len(events)}
            result = enrich_failed_result(result, events)
            result_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
            results.append(result)
    conditions = []
    for name in STABILITY_CONDITIONS:
        rows = [r for r in results if r["condition"] == name]
        passing = [r for r in rows if r["status"] == "passed"]
        values = [float(r["balanced_accuracy"]) for r in passing]
        conditions.append({"condition": name, "runs": len(rows), "passes": len(passing), "decision": "pass" if len(passing) == len(rows) else "reject",
                           "balanced_accuracy_mean": mean(values) if values else None,
                           "balanced_accuracy_stdev": stdev(values) if len(values) > 1 else None,
                           "worst_class_recall": min((float(v["recall"]) for r in passing for v in r["per_class"].values()), default=None),
                           "runtime_seconds": sum(float(r["elapsed_seconds"]) for r in rows)})
    summary = {"schema_version": 1, "phase": 3, "family": "stability", "created_at": utc_now(),
               "selection_uses_locked_evaluation": False, "validation_alias_only": True,
               "fold_manifest_sha256": actual_fold_sha256, "execution_disposition": str(auth_path),
               "baseline_runs": baseline_results, "trainable_runs": len(results), "conditions": conditions, "runs": results,
               "stable_parents": [row["condition"] for row in conditions if row["decision"] == "pass"]}
    (execution / "stability_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True); parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--python", type=Path, required=True); parser.add_argument("--source-model", type=Path, required=True)
    parser.add_argument("--authorization", required=True)
    parser.add_argument("--expected-fold-sha256", default=CORRECTED_FOLD_SHA256)
    parser.add_argument("--preflight-only", action="store_true")
    parser.add_argument("--canary-only", action="store_true")
    parser.add_argument("--recover-canary", type=Path)
    args = parser.parse_args(argv)
    if args.preflight_only:
        print(json.dumps(preflight(args.root.resolve(), args.repo.resolve(), args.python.resolve(), args.source_model.resolve()), indent=2)); return 0
    if args.canary_only:
        print(json.dumps(execute_canary(args.root.resolve(), args.repo.resolve(), args.python.resolve(), args.source_model.resolve()), indent=2)); return 0
    if args.recover_canary:
        print(json.dumps(recover_canary_result(args.recover_canary.resolve(), args.root.resolve()), indent=2)); return 0
    summary = execute_stability(args.root.resolve(), args.repo.resolve(), args.python.resolve(), args.source_model.resolve(), args.authorization, args.expected_fold_sha256)
    print(json.dumps({"trainable_runs": summary["trainable_runs"], "stable_parents": summary["stable_parents"]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
