from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from droplet_trainer.dataset import scan_dataset
from droplet_trainer.schema import default_binary_schema


class DatasetManifestPathTests(unittest.TestCase):
    def test_prepared_local_path_is_preferred_over_stale_source_path(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "prepared"
            image_path = root / "images" / "0" / "local.png"
            manifest_path = root / "metadata" / "dataset_manifest.json"
            image_path.parent.mkdir(parents=True)
            manifest_path.parent.mkdir(parents=True)
            image_path.write_bytes(b"not opened by scan")
            manifest_path.write_text(
                json.dumps(
                    {
                        "schema_version": "dataset-manifest-v1",
                        "items": [
                            {
                                "path": "images/0/local.png",
                                "crop_path": "images/0/local.png",
                                "source_path": r"D:\Data\missing.png",
                                "label": "0",
                                "status": "included",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )

            scan = scan_dataset(str(manifest_path), default_binary_schema())

        self.assertEqual(scan["items"][0]["source_path"], str(image_path.resolve()))
        self.assertEqual(scan["items"][0]["manifest_source_path"], r"D:\Data\missing.png")

    def test_valid_legacy_source_path_is_used_when_local_paths_are_missing(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            root = tmp_path / "prepared"
            legacy_path = tmp_path / "legacy.png"
            manifest_path = root / "metadata" / "dataset_manifest.json"
            manifest_path.parent.mkdir(parents=True)
            legacy_path.write_bytes(b"not opened by scan")
            manifest_path.write_text(
                json.dumps(
                    {
                        "schema_version": "dataset-manifest-v1",
                        "items": [
                            {
                                "path": "images/0/missing.png",
                                "crop_path": "images/0/missing.png",
                                "source_path": str(legacy_path),
                                "label": "0",
                                "status": "included",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )

            scan = scan_dataset(str(manifest_path), default_binary_schema())

        self.assertEqual(scan["items"][0]["source_path"], str(legacy_path.resolve()))


if __name__ == "__main__":
    unittest.main()
