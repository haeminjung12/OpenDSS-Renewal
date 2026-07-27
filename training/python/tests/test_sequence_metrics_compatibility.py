from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from droplet_trainer.validate_sequence import load_sequence_metrics_artifact


class SequenceMetricsCompatibilityTests(unittest.TestCase):
    def test_old_metric_value_csv_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            metrics_path = Path(tmp) / "sequence_metrics.csv"
            metrics_path.write_text("metric,value\nmatched_events,2\nclass_accuracy_on_matched,0.5\n", encoding="utf-8")

            loaded = load_sequence_metrics_artifact(metrics_csv_path=metrics_path)

        self.assertEqual(loaded["csv_schema"], "metric,value")
        self.assertEqual(loaded["ground_truth_metrics"]["matched_events"], 2)
        self.assertEqual(loaded["adjudicated_ground_truth_metrics"]["matched_events"], 2)
        self.assertEqual(loaded["ground_truth_metrics"]["class_accuracy_on_matched"], 0.5)

    def test_new_raw_and_adjudicated_csv_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            metrics_path = Path(tmp) / "sequence_metrics.csv"
            metrics_path.write_text(
                "metric,raw_value,adjudicated_value\n"
                "missed_events,1,0\n"
                "accepted_startup_no_runtime_events,,1\n",
                encoding="utf-8",
            )

            loaded = load_sequence_metrics_artifact(metrics_csv_path=metrics_path)

        self.assertEqual(loaded["csv_schema"], "metric,raw_value,adjudicated_value")
        self.assertEqual(loaded["ground_truth_metrics"]["missed_events"], 1)
        self.assertEqual(loaded["adjudicated_ground_truth_metrics"]["missed_events"], 0)
        self.assertNotIn("accepted_startup_no_runtime_events", loaded["ground_truth_metrics"])
        self.assertEqual(loaded["adjudicated_ground_truth_metrics"]["accepted_startup_no_runtime_events"], 1)

    def test_summary_json_is_preferred_over_csv(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            summary_path = tmp_path / "sequence_validation_summary.json"
            metrics_path = tmp_path / "sequence_metrics.csv"
            summary_path.write_text(
                json.dumps(
                    {
                        "ground_truth_metrics": {"matched_events": 7},
                        "adjudicated_ground_truth_metrics": {"matched_events": 8},
                    }
                ),
                encoding="utf-8",
            )
            metrics_path.write_text("metric,value\nmatched_events,2\n", encoding="utf-8")

            loaded = load_sequence_metrics_artifact(summary_path=summary_path, metrics_csv_path=metrics_path)

        self.assertEqual(loaded["source"], "sequence_validation_summary_json")
        self.assertEqual(loaded["ground_truth_metrics"]["matched_events"], 7)
        self.assertEqual(loaded["adjudicated_ground_truth_metrics"]["matched_events"], 8)


if __name__ == "__main__":
    unittest.main()
