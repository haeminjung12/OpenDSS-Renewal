from __future__ import annotations

import unittest
import json
import tempfile
from pathlib import Path

from .screen import assert_fold_isolation, create_immutable_execution_root, enrich_failed_result, gate, manifest_for_fold, recompute_retained_metrics


class ScreeningRunnerTests(unittest.TestCase):
    def test_fold_overlap_rejected(self):
        records = {"a": {"sha256": "1"}, "b": {"sha256": "2"}}
        with self.assertRaises(ValueError):
            assert_fold_isolation({"development": ["a"], "evaluation": ["a", "b"]}, records)

    def test_hash_leakage_rejected(self):
        records = {"a": {"sha256": "same"}, "b": {"sha256": "same"}}
        with self.assertRaises(ValueError):
            assert_fold_isolation({"development": ["a"], "evaluation": ["b"]}, records)

    def test_manifest_has_validation_only_and_no_alias(self):
        derived = {"class_semantics": {"0": "Empty", "1": "Single", "2": "MoreThanOne"}, "records": [
            {"record_id": "a", "source_root": "C:/x", "source_path": "a.png", "sha256": "1", "class_id": "0"},
            {"record_id": "b", "source_root": "C:/x", "source_path": "b.png", "sha256": "2", "class_id": "1"}]}
        manifest = manifest_for_fold(derived, {"development": ["a"], "evaluation": ["b"]})
        roles = [item["role"] for item in manifest["items"]]
        self.assertEqual(roles, ["train", "validation"])
        self.assertEqual(len(manifest["items"]), 2)
        self.assertTrue(manifest["validation_only"])

    def test_gate_rejects_low_recall(self):
        metrics = {"per_class": {"0": {"recall": .9}, "1": {"recall": .69}, "2": {"recall": .8}}}
        passed, reasons = gate(metrics, {"failure_reasons": []}, 0, 10)
        self.assertFalse(passed); self.assertIn("CLASS_RECALL_BELOW_0.70", reasons)

    def test_gate_accepts_valid_run(self):
        metrics = {"per_class": {str(i): {"recall": .8} for i in range(3)}}
        self.assertTrue(gate(metrics, {"failure_reasons": []}, 0, 10)[0])

    def test_retained_predictions_recompute_exact_metrics(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "predictions.jsonl"
            rows = [{"class_id": "0", "predicted_class_id": "0", "logits": [2.0, 0.0, -1.0]}, {"class_id": "1", "predicted_class_id": "1", "logits": [0.0, 2.0, -1.0]}, {"class_id": "2", "predicted_class_id": "2", "logits": [0.0, -1.0, 2.0]}]
            path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")
            reported = {"accuracy": 1.0, "balanced_accuracy": 1.0, "macro_f1": 1.0, "confusion_matrix": [[1, 0, 0], [0, 1, 0], [0, 0, 1]], "logit_diagnostics": {"prediction_distribution": {"0": 1, "1": 1, "2": 1}}}
            self.assertTrue(recompute_retained_metrics(path, reported)["agreement"])

    def test_execution_id_never_overwrites_history(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            create_immutable_execution_root(root, "immutable")
            with self.assertRaises(FileExistsError):
                create_immutable_execution_root(root, "immutable")

    def test_event_audit_detects_trainer_overwriting_frozen_roles(self):
        with tempfile.TemporaryDirectory() as folder:
            manifest = Path(folder) / "dataset_manifest.json"
            manifest.write_text(json.dumps({"items": [
                {"role": "train", "class_id": "0"}, {"role": "val", "class_id": "1"},
                {"role": "test", "class_id": "1"}]}), encoding="utf-8")
            events = [{"event": "dataset_summary", "dataset": {"manifest_path": str(manifest)},
                       "split_counts": {"train": {"0": 1, "1": 1}, "val": {"1": 1}, "test": {}}}]
            result = enrich_failed_result({"gate_reasons": []}, events)
            self.assertFalse(result["split_contract_preserved"])
            self.assertIn("FROZEN_SPLIT_CONTRACT_OVERRIDDEN", result["gate_reasons"])


if __name__ == "__main__":
    unittest.main()
