from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import tempfile
from collections import Counter
from pathlib import Path, PurePosixPath

DEFAULT_DATASETS_ROOT = Path(
    r"C:\Users\goals\OneDrive\Documents\OpenDropletSortingSuite\datasets\prepared"
)
DEFAULT_SEQUENCE_ROOT = Path(
    r"D:\[2026] Visual Sorting Data\0226 Final please\20260226_163405_BEST SO FAR"
)

DATASETS = {
    "droplet_target_nontarget_3class_starter": {
        "name": "Droplet Target/Non-target 3-class Starter",
        "classes": [
            {"id": "0", "name": "Empty"},
            {"id": "1", "name": "Single"},
            {"id": "2", "name": "MoreThanOne"},
        ],
        "expected_counts": {"0": 3142, "1": 386, "2": 92},
    },
    "droplet_target_nontarget_binary_starter": {
        "name": "Droplet Target/Non-target Binary Starter",
        "classes": [
            {"id": "0", "name": "Non-target"},
            {"id": "1", "name": "Target"},
        ],
        "expected_counts": {"0": 3233, "1": 387},
    },
}


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8-sig") as handle:
        value = json.load(handle)
    if not isinstance(value, dict):
        raise ValueError(f"{path} is not a JSON object")
    return value


def write_json_atomically(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            newline="\n",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_path = Path(handle.name)
            json.dump(value, handle, indent=2)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_path, path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def png_facts(path: Path) -> tuple[int, int, int, int]:
    with path.open("rb") as handle:
        header = handle.read(33)
    if (
        len(header) != 33
        or header[:8] != b"\x89PNG\r\n\x1a\n"
        or header[12:16] != b"IHDR"
    ):
        raise ValueError(f"Not a PNG with a readable IHDR: {path}")
    width, height, bit_depth, color_type = struct.unpack(">IIBB", header[16:26])
    return width, height, bit_depth, color_type


def tiff_facts(path: Path) -> tuple[int, int, int]:
    with path.open("rb") as handle:
        byte_order = handle.read(2)
        if byte_order == b"II":
            endian = "<"
        elif byte_order == b"MM":
            endian = ">"
        else:
            raise ValueError(f"Invalid TIFF byte order: {path}")
        if struct.unpack(f"{endian}H", handle.read(2))[0] != 42:
            raise ValueError(f"Invalid classic TIFF header: {path}")
        ifd_offset = struct.unpack(f"{endian}I", handle.read(4))[0]
        handle.seek(ifd_offset)
        entry_count = struct.unpack(f"{endian}H", handle.read(2))[0]
        tags: dict[int, list[int]] = {}
        type_sizes = {3: 2, 4: 4}
        for _ in range(entry_count):
            entry = handle.read(12)
            if len(entry) != 12:
                raise ValueError(f"Truncated TIFF IFD: {path}")
            tag, value_type, count = struct.unpack(f"{endian}HHI", entry[:8])
            if tag not in {256, 257, 258}:
                continue
            if value_type not in type_sizes or count < 1:
                raise ValueError(f"Unsupported TIFF fact encoding in {path}")
            size = type_sizes[value_type] * count
            if size <= 4:
                raw = entry[8 : 8 + size]
            else:
                return_offset = handle.tell()
                handle.seek(struct.unpack(f"{endian}I", entry[8:12])[0])
                raw = handle.read(size)
                handle.seek(return_offset)
            code = "H" if value_type == 3 else "I"
            tags[tag] = list(struct.unpack(f"{endian}{count}{code}", raw))
    if 256 not in tags or 257 not in tags or 258 not in tags:
        raise ValueError(f"TIFF dimensions or bit depth are missing: {path}")
    bit_depths = set(tags[258])
    if len(bit_depths) != 1:
        raise ValueError(f"TIFF channels do not share one bit depth: {path}")
    return tags[256][0], tags[257][0], bit_depths.pop()


def safe_crop_path(dataset_root: Path, relative_text: str) -> Path:
    portable = PurePosixPath(relative_text)
    if (
        not relative_text
        or portable.is_absolute()
        or ".." in portable.parts
        or "\\" in relative_text
    ):
        raise ValueError(f"Unsafe crop path: {relative_text!r}")
    resolved = (dataset_root / Path(*portable.parts)).resolve()
    resolved.relative_to(dataset_root.resolve())
    return resolved


def build_dataset_manifest(dataset_root: Path, spec: dict) -> dict:
    source_path = dataset_root / "metadata" / "dataset_manifest.json"
    source = load_json(source_path)
    if (
        source.get("schema_version") != "dataset-manifest-v1"
        or source.get("dataset_id") != dataset_root.name
    ):
        raise ValueError(f"Unexpected legacy manifest identity: {source_path}")
    items = source.get("items")
    if not isinstance(items, list) or len(items) != 3625:
        raise ValueError(f"{source_path} must contain exactly 3625 items")

    records: list[dict] = []
    labels: list[dict] = []
    counts = Counter()
    removed = 0
    class_ids = {value["id"] for value in spec["classes"]}
    record_ids: set[str] = set()
    for item in items:
        if not isinstance(item, dict):
            raise ValueError(f"{source_path} contains a non-object item")
        row_index = item.get("row_index")
        if not isinstance(row_index, int) or row_index < 0:
            raise ValueError(f"Legacy item has an invalid row_index: {item!r}")
        record_id = f"legacy-row-{row_index:06d}"
        if record_id in record_ids:
            raise ValueError(f"Duplicate legacy row_index: {row_index}")
        record_ids.add(record_id)

        included = item.get("status") == "included"
        crop_path: str | None = None
        crop_sha256: str | None = None
        if included:
            class_id = str(item.get("reviewed_label") or item.get("label") or "")
            if class_id not in class_ids:
                raise ValueError(f"Included row {row_index} has invalid class {class_id!r}")
            crop_path = str(item.get("path") or "")
            crop_file = safe_crop_path(dataset_root, crop_path)
            if not crop_file.is_file():
                raise FileNotFoundError(f"Included crop is missing: {crop_file}")
            crop_sha256 = str(item.get("hash_sha256") or "").lower()
            if len(crop_sha256) != 64 or sha256_file(crop_file) != crop_sha256:
                raise ValueError(f"Crop SHA-256 mismatch: {crop_file}")
            if png_facts(crop_file) != (64, 64, 8, 0):
                raise ValueError(f"Crop is not verified 64x64 gray8 PNG: {crop_file}")
            labels.append(
                {
                    "label_id": f"label-{record_id}",
                    "record_id": record_id,
                    "class_id": class_id,
                }
            )
            counts[class_id] += 1
        else:
            if item.get("status") != "rejected":
                raise ValueError(f"Unsupported legacy item status at row {row_index}")
            labels.append(
                {
                    "label_id": f"label-{record_id}",
                    "record_id": record_id,
                    "excluded": True,
                }
            )
            removed += 1

        records.append(
            {
                "record_id": record_id,
                "crop_path": crop_path,
                "crop_sha256": crop_sha256,
                "source_frame_id": None,
                "source_frame_index": None,
                "source_event_id": None,
                "timestamp": None,
                "crop_rect": None,
            }
        )

    expected_counts = spec["expected_counts"]
    actual_counts = {key: counts[key] for key in expected_counts}
    if actual_counts != expected_counts or removed != 5:
        raise ValueError(
            f"{dataset_root.name} counts are {actual_counts} with {removed} excluded; "
            f"expected {expected_counts} with 5 excluded"
        )
    manifest = {
        "schema_version": "opendss.dataset.v2",
        "provenance_mode": "legacy_crop_only",
        "dataset_id": dataset_root.name,
        "name": spec["name"],
        "experiment_type": "",
        "notes": "Engineering-converted legacy crop-only Dataset; acquisition provenance is unavailable.",
        "status": "completed",
        "created_at": source.get("created_at"),
        "updated_at": source.get("updated_at"),
        "opendss_version": None,
        "capture": {
            "started_at": None,
            "ended_at": None,
            "requested_duration_seconds": None,
            "stop_reason": None,
            "sequence": None,
            "crop_settings": {
                "width": 64,
                "height": 64,
                "pixel_format": "gray8",
                "file_format": "png",
                "method": None,
                "interpolation": None,
            },
            "camera_settings": None,
            "detection_settings": None,
            "program_settings": None,
        },
        "counts": {
            "total": len(records),
            "unlabeled": 0,
            "labeled": sum(actual_counts.values()),
            "removed": removed,
            "by_class": actual_counts,
        },
        "classes": spec["classes"],
        "records": records,
        "labels": labels,
    }
    return manifest


def build_sequence_manifest(sequence_root: Path) -> dict:
    existing_manifest_path = sequence_root / "sequence.json"
    existing = load_json(existing_manifest_path)
    tiff_files = sorted(
        path.name for path in sequence_root.iterdir() if path.is_file() and path.suffix == ".tiff"
    )
    expected_names = [f"{index:06d}.tiff" for index in range(8687)]
    if tiff_files != expected_names:
        raise ValueError(
            f"Legacy Sequence must contain exactly 000000.tiff through 008686.tiff"
        )

    expected_facts: tuple[int, int, int] | None = None
    for name in expected_names:
        facts = tiff_facts(sequence_root / name)
        if expected_facts is None:
            expected_facts = facts
        elif facts != expected_facts:
            raise ValueError(f"TIFF facts differ for {sequence_root / name}")
    if expected_facts != (1200, 360, 8):
        raise ValueError(f"Unexpected legacy TIFF facts: {expected_facts}")

    sequence_id = existing.get("sequence_id")
    if not isinstance(sequence_id, str) or not sequence_id.strip():
        raise ValueError(f"Existing sequence.json has no stable sequence_id")
    manifest = {
        "schema_version": "opendss.sequence.v2",
        "provenance_mode": "legacy_tiff_sequence",
        "sequence_id": sequence_id,
        "name": sequence_root.name,
        "experiment_type": "",
        "notes": "Engineering-converted legacy TIFF Sequence; only frame naming, count, dimensions, and bit depth were verified.",
        "status": "completed",
        "created_at": None,
        "started_at": None,
        "ended_at": None,
        "requested_duration_seconds": None,
        "stop_reason": None,
        "opendss_version": None,
        "frame_format": "tiff",
        "frame_count": len(expected_names),
        "frame_filename_pattern": "%06d.tiff",
        "camera_settings": None,
        "image": {"width": 1200, "height": 360, "bit_depth": 8},
        "timing": {"timestamps_file": None, "nominal_fps": None},
        "integrity": None,
    }
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Reconstruct the two known crop-only Datasets and known legacy TIFF Sequence as factual OpenDSS v2 artifacts."
    )
    parser.add_argument("--datasets-root", type=Path, default=DEFAULT_DATASETS_ROOT)
    parser.add_argument("--sequence-root", type=Path, default=DEFAULT_SEQUENCE_ROOT)
    args = parser.parse_args()

    dataset_manifests = {}
    for dataset_id, spec in DATASETS.items():
        dataset_root = args.datasets_root.expanduser().resolve() / dataset_id
        dataset_manifests[dataset_id] = (
            dataset_root,
            build_dataset_manifest(dataset_root, spec),
        )
    sequence_root = args.sequence_root.expanduser().resolve()
    sequence_manifest = build_sequence_manifest(sequence_root)

    for dataset_root, manifest in dataset_manifests.values():
        path = dataset_root / "dataset.json"
        write_json_atomically(path, manifest)
        if load_json(path) != manifest:
            raise RuntimeError(f"Atomic Dataset manifest verification failed: {path}")
    sequence_path = sequence_root / "sequence.json"
    write_json_atomically(sequence_path, sequence_manifest)
    if load_json(sequence_path) != sequence_manifest:
        raise RuntimeError(f"Atomic Sequence manifest verification failed: {sequence_path}")

    result = {
        "datasets": {
            dataset_id: manifest["counts"]
            for dataset_id, (_, manifest) in dataset_manifests.items()
        },
        "sequence": {
            "frame_count": sequence_manifest["frame_count"],
            "first": "000000.tiff",
            "last": "008686.tiff",
            "image": sequence_manifest["image"],
        },
    }
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
