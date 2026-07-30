from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from droplet_trainer.dataset import resolve_dataset_path, scan_dataset
from droplet_trainer.schema import default_binary_schema


class DatasetManifestPathTests(unittest.TestCase):
    def test_resolve_dataset_path_accepts_metadata_json_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "prepared"
            manifest_path = root / "metadata" / "dataset_manifest.json"
            manifest_path.parent.mkdir(parents=True)
            manifest_path.write_text(
                json.dumps({"schema_version": "dataset-manifest-v1", "items": []}),
                encoding="utf-8",
            )

            dataset_root, resolved_manifest, mode = resolve_dataset_path(str(manifest_path))

        self.assertEqual(dataset_root, root)
        self.assertEqual(resolved_manifest, manifest_path)
        self.assertEqual(mode, "manifest")

    def test_scan_dataset_accepts_folder_or_metadata_json_path(self) -> None:
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
                                "label": "0",
                                "status": "included",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )

            from_file = scan_dataset(str(manifest_path), default_binary_schema())
            from_folder = scan_dataset(str(root), default_binary_schema())

        self.assertEqual(from_file["mode"], "manifest")
        self.assertEqual(from_folder["mode"], "manifest")
        self.assertEqual(from_file["dataset_root"], root)
        self.assertEqual(from_folder["dataset_root"], root)
        self.assertEqual(from_file["manifest_path"], manifest_path)
        self.assertEqual(from_folder["manifest_path"], manifest_path)
        self.assertEqual(from_file["counts"], from_folder["counts"])

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
