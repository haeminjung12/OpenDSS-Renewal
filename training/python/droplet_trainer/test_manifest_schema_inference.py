from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from droplet_trainer.dataset import scan_dataset
from droplet_trainer.schema import parse_schema


def _args(dataset: str, classes: str | None = None) -> SimpleNamespace:
    return SimpleNamespace(class_schema=None, legacy_schema=False, classes=classes, dataset=dataset)


def _write_manifest(root: Path, payload: dict[str, object]) -> Path:
    manifest_path = root / "metadata" / "dataset_manifest.json"
    manifest_path.parent.mkdir(parents=True)
    manifest_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return manifest_path


class ManifestSchemaInferenceTests(unittest.TestCase):
    def test_parse_schema_infers_ternary_manifest_classes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "prepared"
            _write_manifest(
                root,
                {
                    "class_schema": {
                        "kind": "target-nontarget-ternary",
                        "classes": [
                            {"id": "0", "index": 0, "display_name": "Empty"},
                            {"id": "1", "index": 1, "display_name": "Single"},
                            {"id": "2", "index": 2, "display_name": "MoreThanOne"},
                        ],
                        "excluded_label": {"id": "exclude", "display_name": "Exclude"},
                        "target_class_id": "1",
                    },
                    "items": [],
                },
            )

            schema = parse_schema(_args(str(root)))

        self.assertEqual(schema.classes, ["0", "1", "2"])
        self.assertEqual(schema.display_labels, {"0": "Empty", "1": "Single", "2": "MoreThanOne"})
        self.assertEqual(schema.schema_id, "target-nontarget-ternary")
        self.assertIn("exclude", schema.excluded_labels)

    def test_parse_schema_infers_binary_manifest_classes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "prepared"
            _write_manifest(
                root,
                {
                    "class_schema": {
                        "classes": [
                            {"id": "0", "index": 0, "display_name": "Non-target"},
                            {"id": "1", "index": 1, "display_name": "Target"},
                        ]
                    },
                    "items": [],
                },
            )

            schema = parse_schema(_args(str(root)))

        self.assertEqual(schema.classes, ["0", "1"])
        self.assertEqual(schema.display_labels, {"0": "Non-target", "1": "Target"})

    def test_explicit_classes_override_manifest_inference(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "prepared"
            _write_manifest(
                root,
                {
                    "class_schema": {
                        "classes": [
                            {"id": "0", "index": 0},
                            {"id": "1", "index": 1},
                            {"id": "2", "index": 2},
                        ]
                    },
                    "items": [],
                },
            )

            schema = parse_schema(_args(str(root), classes="0,1"))

        self.assertEqual(schema.classes, ["0", "1"])

    def test_scan_dataset_prefers_reviewed_label_fields(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "prepared"
            image_path = root / "images" / "2" / "sample.png"
            image_path.parent.mkdir(parents=True)
            image_path.write_bytes(b"not opened by scan")
            _write_manifest(
                root,
                {
                    "class_schema": {
                        "classes": [
                            {"id": "0", "index": 0},
                            {"id": "1", "index": 1},
                            {"id": "2", "index": 2},
                        ]
                    },
                    "items": [
                        {
                            "path": "images/2/sample.png",
                            "label": "0",
                            "reviewed_label": "2",
                            "review_state": "confirmed",
                            "status": "included",
                        }
                    ],
                },
            )
            schema = parse_schema(_args(str(root)))

            scan = scan_dataset(str(root), schema)

        self.assertEqual(schema.classes, ["0", "1", "2"])
        self.assertEqual(dict(scan["counts"]), {"2": 1})
        self.assertEqual(scan["items"][0]["class_id"], "2")


if __name__ == "__main__":
    unittest.main()
