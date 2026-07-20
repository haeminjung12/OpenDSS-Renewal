from __future__ import annotations

import argparse, json
from datetime import datetime, timezone
from pathlib import Path
from statistics import mean, stdev

from .phase3_relaunch2 import (DERIVED_SHA256, SOURCE_SHA256, invoke_preflight, run_one,
                               verify_pins)
from .screen import CORRECTED_FOLD_SHA256, create_immutable_execution_root

ALPHAS = (0.65, 0.75, 0.85, 0.90)


def condition(alpha: float) -> dict:
    return {"name": f"sampler_alpha_{alpha:.2f}",
            "imbalance": {"mode": "balanced_sampler", "sampler_alpha": float(alpha)}}


def frozen_plan() -> dict:
    return {"schema_version": 1, "plan": "phase3b-class1-class2-boundary",
            "conditions": [condition(alpha) for alpha in ALPHAS],
            "primary_binding": {"seed": 1729, "fold": 0},
            "confirmation_binding": {"seed": 2718, "fold": 1},
            "maximum_primary_runs": 6, "maximum_total_runs": 8,
            "prune": {"class0_min": .60, "class1_min": .60, "class2_min": .50},
            "strict_gate": {"all_class_recall_min": .70},
            "fixed": {"signed_logits": True, "classifier_initialization": "partial_mean_existing",
                      "batch_size": 64, "amp": True, "schedule": [[20, 1e-4], [15, 1e-5]],
                      "fold_sha256": CORRECTED_FOLD_SHA256, "derived_sha256": DERIVED_SHA256,
                      "source_sha256": SOURCE_SHA256},
            "labels_modified": False, "locked_evaluation_access": False}


def recalls(row: dict) -> list[float]:
    return [float(item["recall"]) for item in row.get("class_metrics", [])]


def eligible(row: dict) -> bool:
    values = recalls(row); diagnostics = row.get("diagnostics", {})
    return (len(values) == 3 and values[0] >= .60 and values[1] >= .60 and values[2] >= .50
            and not diagnostics.get("failure_reasons") and row.get("metric_recomputation", {}).get("agreement"))


def rank_key(row: dict) -> tuple[float, float, float]:
    values = recalls(row)
    return (float(row.get("balanced_accuracy") or -1), float(row.get("macro_f1") or -1), min(values, default=-1))


def execute(root: Path, repo: Path, python: Path, source: Path) -> dict:
    hashes = verify_pins(root, source); execution_id = "phase3b-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    execution = create_immutable_execution_root(root / "boundary-tuning", execution_id)
    plan = frozen_plan(); plan["frozen_at"] = datetime.now(timezone.utc).isoformat(); plan["artifact_hashes"] = hashes
    plan_path = execution / "frozen_plan.json"; plan_path.write_text(json.dumps(plan, indent=2), encoding="utf-8")
    disposition = {"schema_version": 1, "execution_id": execution_id, "work_order": "phase-3b-class-boundary-tuning.md",
                   "frozen_plan": str(plan_path), "history_preserved": True,
                   "historical_roots": [str(root / name) for name in ("screening", "screening-readiness", "class2-readiness", "screening-valid")],
                   "labels_modified": False, "locked_evaluation_access": False}
    (execution / "execution_disposition.json").write_text(json.dumps(disposition, indent=2), encoding="utf-8")
    conditions = plan["conditions"]
    invoke_preflight(execution, root, repo, python, source, conditions, {1729: 0, 2718: 1})
    rows = [run_one(execution, root, repo, python, source, item, 1729, 0) for item in conditions]
    candidates = sorted([row for row in rows if eligible(row)], key=rank_key, reverse=True)[:2]
    for selected in candidates:
        rows.append(run_one(execution, root, repo, python, source, selected["condition"], 2718, 1))
    aggregates = []
    finalists = []
    for item in conditions:
        selected = [row for row in rows if row["condition"]["name"] == item["name"]]
        if not selected: continue
        passed = len(selected) == 2 and all(row["status"] == "passed" for row in selected)
        values = [float(row["balanced_accuracy"]) for row in selected]
        aggregate = {"condition": item["name"], "alpha": item["imbalance"]["sampler_alpha"],
                     "runs": len(selected), "eligible_fold0": eligible(selected[0]), "passes_both_folds": passed,
                     "mean_balanced_accuracy": mean(values), "balanced_accuracy_stdev": stdev(values) if len(values) > 1 else None,
                     "mean_macro_f1": mean(float(row["macro_f1"]) for row in selected),
                     "worst_class_recall": min(value for row in selected for value in recalls(row)),
                     "runtime_seconds": sum(float(row["wall_seconds"]) for row in selected),
                     "peak_cuda_memory_allocated": max(row["telemetry"].get("peak_cuda_memory_allocated", 0) for row in selected)}
        aggregates.append(aggregate)
        if passed: finalists.append(item["name"])
    ledger = {"schema_version": 1, "execution_id": execution_id, "frozen_plan": str(plan_path),
              "integrity_valid_runs": len(rows), "primary_runs": 4, "confirmation_runs": len(rows) - 4,
              "rescue_runs": 0, "runs": rows, "aggregates": aggregates, "phase4_finalist_recommendations": finalists,
              "phase4_started": False, "locked_evaluation_access": False, "labels_modified": False}
    (execution / "run_ledger.json").write_text(json.dumps(ledger, indent=2), encoding="utf-8")
    (root / "boundary-tuning" / "latest_phase3b.json").write_text(json.dumps({"execution_id": execution_id,
        "ledger": str(execution / "run_ledger.json")}, indent=2), encoding="utf-8")
    print(json.dumps({"execution_id": execution_id, "runs": len(rows), "finalists": finalists}, indent=2)); return ledger


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True); parser.add_argument("--python", type=Path, required=True)
    parser.add_argument("--source-model", type=Path, required=True); args = parser.parse_args(argv)
    execute(args.root.resolve(), args.repo.resolve(), args.python.resolve(), args.source_model.resolve()); return 0


if __name__ == "__main__": raise SystemExit(main())
