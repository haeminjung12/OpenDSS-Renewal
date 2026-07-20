from __future__ import annotations

import argparse, json, os, subprocess
from datetime import datetime, timezone
from pathlib import Path
from statistics import mean, stdev

from .screen import (CORRECTED_FOLD_SHA256, attach_frozen_contract, base_config,
                     create_immutable_execution_root, load_inputs, manifest_for_fold,
                     parse_events, recompute_retained_metrics, sha256_file)

DERIVED_SHA256 = "756a84f251e535565740b82dfb68a0f1e1d6ffa6f55e32747dd60bd4833d525b"
SOURCE_SHA256 = "34eec09f49ab4612a34e3a24ccf85eccc98516b388fbadbfb0736ecbf8fb1769"
SEED_FOLDS = {1729: 0, 2718: 1}
PARENTS = [
    {"name": "balanced_sampler_only", "imbalance": {"mode": "balanced_sampler"}},
    {"name": "effective_cap10", "imbalance": {"mode": "effective_number", "beta": .999, "cap": 10.0}},
]
STAGES = [
    {"name": "stage1", "epochs": 20, "learning_rate": 1e-4, "trainable": "classifier_and_last_fire_modules"},
    {"name": "stage2", "epochs": 15, "learning_rate": 1e-5, "trainable": "fine_tune"},
]


def now_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")


def verify_pins(root: Path, source: Path) -> dict:
    hashes = {"fold": sha256_file(root / "prep" / "fold_manifest.json"),
              "derived": sha256_file(root / "prep" / "derived_manifest.json"),
              "source": sha256_file(source),
              "protocol": sha256_file(root / "prep" / "experiment_protocol.json"),
              "conditions": sha256_file(root / "prep" / "condition_specs.json")}
    expected = {"fold": CORRECTED_FOLD_SHA256, "derived": DERIVED_SHA256, "source": SOURCE_SHA256}
    mismatches = {key: {"expected": value, "actual": hashes[key]} for key, value in expected.items() if hashes[key] != value}
    if mismatches: raise RuntimeError(f"Pinned artifact mismatch: {mismatches}")
    return hashes


def config_for(condition: dict, seed: int, source: Path) -> dict:
    config = base_config("staged_signed_1e-4_1e-5", seed, source)
    config.update({"classifier_output": "signed_logits", "classifier_initialization": "partial_mean_existing",
                   "stages": STAGES})
    config["imbalance"] = {**condition["imbalance"], "class_weight_formula": "condition_specific",
                           "computed_from": "training_split_only", "class_weights": {}}
    return config


def invoke_preflight(execution: Path, root: Path, repo: Path, python: Path, source: Path,
                     conditions: list[dict] | None = None, seed_folds: dict[int, int] | None = None) -> dict:
    derived, folds, _, _ = load_inputs(root / "prep")
    accepted = []
    env = os.environ.copy(); env["PYTHONPATH"] = str(repo / "training" / "python")
    for condition in conditions or PARENTS:
        for seed, fold_index in (seed_folds or SEED_FOLDS).items():
            fold = next(item for item in folds["folds"] if int(item["fold"]) == fold_index)
            target = execution / "preflight" / f"{condition['name']}__seed{seed}__fold{fold_index}"
            dataset = target / "dataset"; (dataset / "metadata").mkdir(parents=True)
            manifest = manifest_for_fold(derived, fold); manifest_path = dataset / "metadata" / "dataset_manifest.json"
            manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
            config = attach_frozen_contract(config_for(condition, seed, source), manifest, manifest_path, root / "prep", fold)
            config_path = target / "config.json"; config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
            command = [str(python), "-m", "droplet_trainer", "train", "--dataset", str(dataset), "--output", str(target / "trainer"),
                       "--config", str(config_path), "--device", "cuda", "--classes", "0,1,2", "--run-name", "preflight", "--jsonl", "--preflight-only"]
            cp = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, encoding="utf-8", errors="replace")
            events_path = target / "events.jsonl"; events_path.write_text(cp.stdout, encoding="utf-8")
            events = parse_events(events_path); contracts = [e for e in events if e.get("event") == "split_contract_accepted"]
            if cp.returncode or len(contracts) != 1 or any(e.get("event") in {"stage_started", "epoch_started"} for e in events):
                raise RuntimeError(f"Frozen preflight failed: {condition['name']} seed {seed}")
            counts = contracts[0]["split_contract"]["identity_counts"]
            if counts != {"train": 3448, "validation": 863}: raise RuntimeError(f"Wrong frozen counts: {counts}")
            accepted.append({"condition": condition["name"], "seed": seed, "fold": fold_index, "counts": counts,
                             "manifest_sha256": sha256_file(manifest_path), "config_sha256": sha256_file(config_path),
                             "optimizer_updates": 0, "model_created": False, "events": str(events_path)})
    result = {"status": "accepted", "bindings": accepted, "training_started": False}
    (execution / "preflight_result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


def run_one(execution: Path, root: Path, repo: Path, python: Path, source: Path,
            condition: dict, seed: int, fold_index: int) -> dict:
    derived, folds, _, _ = load_inputs(root / "prep")
    fold = next(item for item in folds["folds"] if int(item["fold"]) == fold_index)
    run_id = f"{condition['name']}__seed{seed}__fold{fold_index}"
    run_root = create_immutable_execution_root(execution / "runs", run_id)
    dataset = run_root / "dataset"; (dataset / "metadata").mkdir(parents=True)
    manifest = manifest_for_fold(derived, fold); manifest_path = dataset / "metadata" / "dataset_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    config = attach_frozen_contract(config_for(condition, seed, source), manifest, manifest_path, root / "prep", fold)
    config_path = run_root / "immutable_config.json"; config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    env = os.environ.copy(); env["PYTHONPATH"] = str(repo / "training" / "python")
    output = run_root / "trainer"
    command = [str(python), "-m", "droplet_trainer", "train", "--dataset", str(dataset), "--output", str(output),
               "--config", str(config_path), "--device", "cuda", "--classes", "0,1,2", "--run-name", "run", "--jsonl"]
    started = datetime.now(timezone.utc); cp = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
    wall = (datetime.now(timezone.utc) - started).total_seconds(); events_path = run_root / "events.jsonl"
    events_path.write_text(cp.stdout, encoding="utf-8"); events = parse_events(events_path); trainer = output / "run"
    metrics_path, failure_path = trainer / "metrics.json", trainer / "failure_diagnostics.json"
    if metrics_path.exists(): metrics = json.loads(metrics_path.read_text(encoding="utf-8"))["test"]
    elif failure_path.exists(): metrics = json.loads(failure_path.read_text(encoding="utf-8"))["metrics"]
    else: metrics = {}
    epoch_rows = [e.get("metrics", {}) for e in events if e.get("event") == "epoch_metrics"]
    updates = max((int(row.get("optimizer_updates", 0)) for row in epoch_rows), default=0)
    delta = max((float(row.get("parameter_delta_l2", 0)) for row in epoch_rows), default=0)
    predictions = trainer / "validation_predictions.jsonl"
    recomputed = recompute_retained_metrics(predictions, metrics) if predictions.exists() and metrics else None
    recalls = [float(row["recall"]) for row in metrics.get("class_metrics", [])]
    diagnostics, telemetry = metrics.get("logit_diagnostics", {}), metrics.get("telemetry", {})
    checkpoint_hashes = {path.name: sha256_file(path) for path in (trainer / "checkpoints").glob("*.pth")}
    contracts = [e for e in events if e.get("event") == "split_contract_accepted"]
    reasons = []
    if cp.returncode: reasons.append(f"TRAINER_EXIT_{cp.returncode}")
    if updates <= 0 or delta <= 0: reasons.append("ZERO_EFFECTIVE_UPDATES")
    if len(recalls) != 3 or min(recalls, default=0) < .70: reasons.append("CLASS_RECALL_BELOW_0.70")
    reasons.extend(diagnostics.get("failure_reasons", []))
    if not recomputed or not recomputed.get("agreement"): reasons.append("METRIC_RECOMPUTATION_FAILED")
    if len(contracts) != 1 or contracts[0]["split_contract"]["identity_counts"] != {"train": 3448, "validation": 863}: reasons.append("FROZEN_SPLIT_CONTRACT_FAILED")
    if len(set(checkpoint_hashes.values())) < 2: reasons.append("CHECKPOINT_IDENTITY_INVALID")
    if telemetry.get("peak_cuda_memory_allocated", 0) <= 0: reasons.append("CUDA_TELEMETRY_MISSING")
    result = {"run_id": run_id, "condition": condition, "seed": seed, "fold": fold_index,
              "status": "passed" if not reasons else "rejected", "gate_reasons": sorted(set(reasons)),
              "exit_code": cp.returncode, "wall_seconds": wall, "optimizer_updates": updates,
              "parameter_delta_l2": delta, "balanced_accuracy": metrics.get("balanced_accuracy"),
              "macro_f1": metrics.get("macro_f1"), "class_metrics": metrics.get("class_metrics", []),
              "confusion_matrix": metrics.get("confusion_matrix", []), "diagnostics": diagnostics,
              "telemetry": telemetry, "metric_recomputation": recomputed, "checkpoint_hashes": checkpoint_hashes,
              "manifest_sha256": sha256_file(manifest_path), "config_sha256": sha256_file(config_path),
              "fold_manifest_sha256": sha256_file(root / "prep" / "fold_manifest.json"),
              "derived_manifest_sha256": sha256_file(root / "prep" / "derived_manifest.json"),
              "source_model_sha256": sha256_file(source), "events": str(events_path), "trainer_dir": str(trainer),
              "publication_result": not reasons}
    (run_root / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


def execute(root: Path, repo: Path, python: Path, source: Path) -> dict:
    hashes = verify_pins(root, source); execution_id = "relaunch2-" + now_id()
    execution = create_immutable_execution_root(root / "screening-valid", execution_id)
    disposition = {"schema_version": 1, "execution_id": execution_id, "authorization": "phase-3-gpu-screening-relaunch-2.md",
                   "created_at": datetime.now(timezone.utc).isoformat(), "hashes": hashes,
                   "historical_roots_preserved": [str(root / "screening"), str(root / "screening-readiness"), str(root / "class2-readiness")],
                   "maximum_valid_runs": 26, "locked_evaluation_access": False}
    (execution / "execution_disposition.json").write_text(json.dumps(disposition, indent=2), encoding="utf-8")
    invoke_preflight(execution, root, repo, python, source)
    results = []
    for condition in PARENTS:
        for seed, fold in SEED_FOLDS.items(): results.append(run_one(execution, root, repo, python, source, condition, seed, fold))
    aggregates = []
    for condition in PARENTS:
        rows = [row for row in results if row["condition"]["name"] == condition["name"]]
        passed = [row for row in rows if row["status"] == "passed"]
        values = [float(row["balanced_accuracy"]) for row in rows if row.get("balanced_accuracy") is not None]
        aggregates.append({"condition": condition["name"], "runs": len(rows), "passes": len(passed),
                           "decision": "pass" if len(passed) == 2 else "reject",
                           "balanced_accuracy_mean": mean(values) if values else None,
                           "balanced_accuracy_stdev": stdev(values) if len(values) > 1 else None,
                           "worst_class_recall": min((metric["recall"] for row in rows for metric in row["class_metrics"]), default=None),
                           "runtime_seconds": sum(row["wall_seconds"] for row in rows),
                           "peak_cuda_memory_allocated": max((row["telemetry"].get("peak_cuda_memory_allocated", 0) for row in rows), default=0)})
    parents = [row["condition"] for row in aggregates if row["decision"] == "pass"]
    ledger = {"schema_version": 1, "execution_id": execution_id, "hashes": hashes, "integrity_valid_run_count": len(results),
              "parents_passing_two_seed_gate": parents, "aggregates": aggregates, "runs": results,
              "balancing_started": False, "efficiency_started": False, "phase4_started": False}
    (execution / "valid_run_ledger.json").write_text(json.dumps(ledger, indent=2), encoding="utf-8")
    latest = root / "screening-valid" / "latest_relaunch2.json"
    latest.write_text(json.dumps({"execution_id": execution_id, "ledger": str(execution / "valid_run_ledger.json")}, indent=2), encoding="utf-8")
    print(json.dumps({"execution_id": execution_id, "parents": parents, "runs": len(results)}, indent=2))
    return ledger


def finalize_existing_ledger(path: Path) -> dict:
    ledger = json.loads(path.read_text(encoding="utf-8")); rows = ledger["runs"]; aggregates = []
    for condition in PARENTS:
        selected = [row for row in rows if row["condition"]["name"] == condition["name"]]
        passed = [row for row in selected if row["status"] == "passed"]
        values = [float(row["balanced_accuracy"]) for row in selected]
        aggregates.append({"condition": condition["name"], "runs": len(selected), "passes": len(passed),
            "decision": "pass" if len(passed) == 2 else "reject", "balanced_accuracy_mean": mean(values),
            "balanced_accuracy_stdev": stdev(values),
            "worst_class_recall": min(metric["recall"] for row in selected for metric in row["class_metrics"]),
            "runtime_seconds": sum(row["wall_seconds"] for row in selected),
            "peak_cuda_memory_allocated": max(row["telemetry"].get("peak_cuda_memory_allocated", 0) for row in selected)})
    ledger["aggregates"] = aggregates; ledger.pop("valid_run_count", None); ledger["integrity_valid_run_count"] = len(rows)
    path.write_text(json.dumps(ledger, indent=2), encoding="utf-8"); return ledger


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True); parser.add_argument("--python", type=Path, required=True)
    parser.add_argument("--source-model", type=Path, required=True); args = parser.parse_args(argv)
    execute(args.root.resolve(), args.repo.resolve(), args.python.resolve(), args.source_model.resolve()); return 0


if __name__ == "__main__": raise SystemExit(main())
