from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from droplet_trainer.metadata import metadata_scaffold, sorting_policy_for_classes, validate_metadata
from droplet_trainer.schema import parse_schema


class LabelMetadataTests(unittest.TestCase):
    def test_parse_schema_ternary_defaults_use_stable_ids_and_labels(self) -> None:
        schema = parse_schema(SimpleNamespace(class_schema=None, legacy_schema=False, classes="0,1,2"))

        self.assertEqual(schema.classes, ["0", "1", "2"])
        self.assertEqual(schema.display_labels, {"0": "Non-target A", "1": "Target", "2": "Non-target B"})
        self.assertEqual(schema.schema_id, "droplet-labels-target-nontarget-3class-v1")

    def test_metadata_scaffold_records_class_count_and_ids(self) -> None:
        metadata = metadata_scaffold(["0", "1"], {"0": "Keep", "1": "Sort"})

        self.assertEqual(metadata["class_count"], 2)
        self.assertEqual(metadata["class_ids"], ["0", "1"])
        self.assertEqual(metadata["sorting_policy"]["target_class_id"], "1")
        self.assertEqual(metadata["sorting_policy"]["target_display_label"], "Sort")

    def test_sorting_policy_for_ternary_uses_target_class_id_without_hardcoded_waste(self) -> None:
        sorting_policy = sorting_policy_for_classes(
            ["0", "1", "2"],
            {"0": "Non-target A", "1": "Target", "2": "Non-target B"},
        )

        self.assertEqual(sorting_policy["target_class_id"], "1")
        self.assertEqual(sorting_policy["target_display_label"], "Target")
        self.assertEqual(sorting_policy["non_target_class_ids"], ["0", "2"])
        self.assertIsNone(sorting_policy["waste_class_id"])

    def test_validate_metadata_accepts_binary_custom_display_labels(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            metadata_path = Path(tmp) / "metadata.json"
            metadata = metadata_scaffold(["0", "1"], {"0": "Keep", "1": "Sort"})
            metadata["model_id"] = "model-1"
            metadata["created_at"] = "2026-07-13T00:00:00Z"
            metadata["artifact"]["onnx_sha256"] = "a" * 64
            metadata["limitations"] = ["Sequence validation has not been run yet."]
            metadata_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")

            result = validate_metadata(metadata_path, promotion_gate=True)

        self.assertTrue(result["valid"])
        self.assertEqual(result["display_labels"], {"0": "Keep", "1": "Sort"})
        self.assertEqual(result["target_class_id"], "1")

    def test_validate_metadata_accepts_stable_ternary_ids_without_legacy_warning(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            metadata_path = Path(tmp) / "metadata.json"
            metadata = metadata_scaffold(
                ["0", "1", "2"],
                {"0": "Debris", "1": "Sortable", "2": "Doublet"},
            )
            metadata["model_id"] = "model-ternary"
            metadata["created_at"] = "2026-07-13T00:00:00Z"
            metadata["artifact"]["onnx_sha256"] = "b" * 64
            metadata["limitations"] = ["Sequence validation has not been run yet."]
            metadata_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")

            result = validate_metadata(metadata_path)

        self.assertTrue(result["valid"])
        self.assertEqual(result["mode"], "schema_valid")
        self.assertNotIn("legacy class ids are present", result["warnings"])


if __name__ == "__main__":
    unittest.main()
