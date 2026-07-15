from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import shutil
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

SOURCE_MANIFEST_CANDIDATES = [
    Path(r"C:\Users\goals\Documents\OpenDSS\datasets\droplet_binary_2026-04-30\metadata\dataset_manifest.json"),
    Path(r"C:\Users\goals\Codex\CNN for Droplet Sorting\datasets\prepared\droplet_binary_2026-04-30\metadata\dataset_manifest.json"),
]

BINARY_DATASET_ID = "droplet_target_nontarget_binary_starter"
THREE_CLASS_DATASET_ID = "droplet_target_nontarget_3class_starter"
BINARY_SCHEMA_ID = "droplet-labels-target-nontarget-binary-v1"
THREE_CLASS_SCHEMA_ID = "droplet-labels-target-nontarget-3class-v1"

LEGACY_EMPTY = "Empty"
LEGACY_SINGLE = "Single"
LEGACY_MORE = "MoreThanTwo"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def normalize_legacy_label(label: str | None) -> str | None:
    text = (label or "").strip()
    lowered = text.lower()
    if lowered in {"empty", "waste"}:
        return LEGACY_EMPTY
    if lowered in {"single", "target"}:
        return LEGACY_SINGLE
    if lowered in {"morethantwo", "more than two", "morethan2", ">2", "2", "multiple"}:
        return LEGACY_MORE
    return None


def inverse_count_weights(train_counts: dict[str, int], classes: list[str]) -> dict[str, float]:
    raw = [1.0 / float(train_counts[class_id]) for class_id in classes]
    minimum = min(raw)
    return {class_id: raw[index] / minimum for index, class_id in enumerate(classes)}


def link_or_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        return
    try:
        os.link(source, destination)
    except OSError:
        shutil.copy2(source, destination)


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def detect_source_manifest(path_arg: str | None) -> Path:
    if path_arg:
        path = Path(path_arg).expanduser().resolve()
        if not path.is_file():
            raise FileNotFoundError(f"Source manifest not found: {path}")
        return path
    for candidate in SOURCE_MANIFEST_CANDIDATES:
        if candidate.is_file():
            return candidate
    joined = "\n".join(f"- {candidate}" for candidate in SOURCE_MANIFEST_CANDIDATES)
    raise FileNotFoundError(f"No source dataset manifest found. Checked:\n{joined}")


def dataset_spec(kind: str) -> dict:
    if kind == "binary":
        return {
            "dataset_id": BINARY_DATASET_ID,
            "schema_id": BINARY_SCHEMA_ID,
            "classes": ["0", "1"],
            "display_labels": {"0": "Non-target", "1": "Target"},
            "aliases": {
                "0": ["0", "Non-target", "non-target", "Nontarget", "Empty", "empty", "Waste", "waste", "MoreThanTwo", "More than two", "MoreThan2", ">2", "2", "Multiple"],
                "1": ["1", "Target", "target", "Single", "single"],
            },
            "legacy_to_class": {LEGACY_EMPTY: "0", LEGACY_SINGLE: "1", LEGACY_MORE: "0"},
        }
    if kind == "three_class":
        return {
            "dataset_id": THREE_CLASS_DATASET_ID,
            "schema_id": THREE_CLASS_SCHEMA_ID,
            "classes": ["0", "1", "2"],
            "display_labels": {"0": "Non-target A", "1": "Target", "2": "Non-target B"},
            "aliases": {
                "0": ["0", "Non-target A", "non-target a", "NonTargetA", "Empty", "empty", "Waste", "waste"],
                "1": ["1", "Target", "target", "Single", "single"],
                "2": ["2", "Non-target B", "non-target b", "NonTargetB", "MoreThanTwo", "More than two", "MoreThan2", ">2", "Multiple"],
            },
            "legacy_to_class": {LEGACY_EMPTY: "0", LEGACY_SINGLE: "1", LEGACY_MORE: "2"},
        }
    raise ValueError(f"Unsupported dataset kind: {kind}")


def build_manifest_classes(classes: list[str], display_labels: dict[str, str]) -> list[dict]:
    return [
        {
            "id": class_id,
            "index": index,
            "display_name": display_labels[class_id],
            "folder": f"images/{class_id}",
        }
        for index, class_id in enumerate(classes)
    ]


def migrate_dataset(source_manifest: dict, source_manifest_path: Path, output_root: Path, kind: str) -> Path:
    spec = dataset_spec(kind)
    dataset_root = output_root / spec["dataset_id"]
    if dataset_root.exists():
        shutil.rmtree(dataset_root)

    source_dataset_roots = [source_manifest_path.parent.parent]
    for candidate in SOURCE_MANIFEST_CANDIDATES:
        candidate_root = candidate.parent.parent
        if candidate.is_file() and candidate_root not in source_dataset_roots:
            source_dataset_roots.append(candidate_root)
    image_root = dataset_root / "images"
    metadata_root = dataset_root / "metadata"
    image_root.mkdir(parents=True, exist_ok=True)
    metadata_root.mkdir(parents=True, exist_ok=True)

    source_items = source_manifest.get("items", [])
    manifest_items: list[dict] = []
    included_counts = Counter()
    split_counts = {role: Counter() for role in ("train", "val", "test")}
    display_label_counts = Counter()
    legacy_label_counts = Counter()

    for raw in source_items:
        item = dict(raw)
        status = str(item.get("status", "included"))
        raw_label = str(item.get("original_label", item.get("label", "")))
        normalized_legacy = normalize_legacy_label(raw_label)
        role = str(item.get("split", item.get("role", "unknown")))

        source_rel = str(item.get("path") or item.get("crop_path") or "").strip()
        source_file = None
        if source_rel:
            for source_root in source_dataset_roots:
                candidate_file = (source_root / source_rel).resolve()
                if candidate_file.is_file():
                    source_file = candidate_file
                    break
        target_rel = ""
        class_id = ""
        display_label = ""
        hash_value = str(item.get("hash_sha256", "")).strip()

        if status not in {"rejected", "excluded"} and normalized_legacy is not None and source_file and source_file.is_file():
            class_id = spec["legacy_to_class"][normalized_legacy]
            display_label = spec["display_labels"][class_id]
            target_rel = f"images/{class_id}/{source_file.name}"
            target_file = dataset_root / target_rel
            link_or_copy(source_file, target_file)
            if not hash_value:
                hash_value = sha256_file(target_file)
            included_counts[class_id] += 1
            display_label_counts[display_label] += 1
            if role in split_counts:
                split_counts[role][class_id] += 1
        else:
            status = "rejected"
            role = "rejected"

        legacy_label_key = normalized_legacy or (raw_label if raw_label else "<blank>")
        legacy_label_counts[legacy_label_key] += 1

        manifest_item = {
            "row_index": item.get("row_index"),
            "image_index": item.get("image_index"),
            "source_path": item.get("source_path"),
            "source_csv_image_path": item.get("source_csv_image_path"),
            "source_relative_path": item.get("source_relative_path"),
            "original_label": display_label if display_label else "",
            "legacy_original_label": raw_label,
            "label": class_id,
            "status": status,
            "exclude_reason": "" if status == "included" else str(item.get("exclude_reason", "blank_or_unknown_label")),
            "split": role,
            "role": role,
            "provenance": dict(item.get("provenance", {})),
            "hash_sha256": hash_value,
            "path": target_rel,
        }
        if "review_state" in item:
            manifest_item["review_state"] = item.get("review_state")
        if "trainer_eligible" in item:
            manifest_item["trainer_eligible"] = item.get("trainer_eligible")
        if "timestamp" in item:
            manifest_item["timestamp"] = item.get("timestamp")
        manifest_items.append(manifest_item)

    train_counts = {class_id: int(split_counts["train"].get(class_id, 0)) for class_id in spec["classes"]}
    weights = inverse_count_weights(train_counts, spec["classes"])
    manifest_payload = {
        "schema_version": "dataset-manifest-v1",
        "dataset_id": spec["dataset_id"],
        "created_at": utc_now(),
        "root": str(dataset_root.resolve()),
        "source": {
            "type": "migrated_from_binary_manifest_with_legacy_labels",
            "path": str(source_manifest_path.parent.parent.resolve()),
            "manifest": str(source_manifest_path.resolve()),
        },
        "classes": build_manifest_classes(spec["classes"], spec["display_labels"]),
        "class_schema": {
            "label_schema_version": spec["schema_id"],
            "classes": spec["classes"],
            "class_to_idx": {class_id: index for index, class_id in enumerate(spec["classes"])},
            "display_labels": spec["display_labels"],
            "aliases": spec["aliases"],
            "excluded_labels": ["Reject", "reject", "Rejected", "rejected", "exclude", "Exclude", ""],
        },
        "split": source_manifest.get("split", {"train": 0.7, "val": 0.15, "test": 0.15}),
        "seed": source_manifest.get("seed", 42),
        "items_total": len(manifest_items),
        "items_included": sum(included_counts.values()),
        "items_excluded": len([item for item in manifest_items if item["status"] != "included"]),
        "items": manifest_items,
    }

    summary_payload = {
        "schema_version": 1,
        "dataset_id": spec["dataset_id"],
        "created_at": utc_now(),
        "dataset_path": str(dataset_root.resolve()),
        "source_manifest": str(source_manifest_path.resolve()),
        "samples_total_rows": len(manifest_items),
        "samples_included": sum(included_counts.values()),
        "samples_excluded": len([item for item in manifest_items if item["status"] != "included"]),
        "class_counts": {class_id: int(included_counts.get(class_id, 0)) for class_id in spec["classes"]},
        "display_label_counts": {label: int(display_label_counts.get(label, 0)) for label in spec["display_labels"].values()},
        "legacy_original_label_counts": {label: int(legacy_label_counts[label]) for label in sorted(legacy_label_counts)},
        "split_counts": {
            role: {class_id: int(split_counts[role].get(class_id, 0)) for class_id in spec["classes"]}
            for role in ("train", "val", "test")
        },
        "balancing": {
            "mode": "inverse-count-min-normalized",
            "class_weights": weights,
        },
        "warnings": [
            "Starter dataset was rebuilt from a binary prepared manifest that preserved legacy source labels in legacy_original_label/original manifest fields."
        ],
    }

    balance_rows = [
        {
            "class": class_id,
            "display_label": spec["display_labels"][class_id],
            "count": int(included_counts.get(class_id, 0)),
            "train_count": train_counts[class_id],
            "weight": weights[class_id],
        }
        for class_id in spec["classes"]
    ]

    (metadata_root / "dataset_manifest.json").write_text(json.dumps(manifest_payload, indent=2), encoding="utf-8")
    (metadata_root / "dataset_summary.json").write_text(json.dumps(summary_payload, indent=2), encoding="utf-8")
    with (metadata_root / "class_balance.csv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["class", "display_label", "count", "train_count", "weight"])
        writer.writeheader()
        for row in balance_rows:
            writer.writerow(row)
    return dataset_root


def main() -> int:
    parser = argparse.ArgumentParser(description="Create package-ready target/non-target starter datasets.")
    parser.add_argument("--source-manifest", help="Path to the existing prepared binary dataset manifest.")
    parser.add_argument(
        "--output-root",
        default=str(Path(__file__).resolve().parents[3] / "datasets" / "prepared"),
        help="Prepared dataset parent directory.",
    )
    args = parser.parse_args()

    source_manifest_path = detect_source_manifest(args.source_manifest)
    source_manifest = load_json(source_manifest_path)
    output_root = Path(args.output_root).expanduser().resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    three_class_root = migrate_dataset(source_manifest, source_manifest_path, output_root, "three_class")
    binary_root = migrate_dataset(source_manifest, source_manifest_path, output_root, "binary")

    result = {
        "status": "ok",
        "source_manifest": str(source_manifest_path),
        "three_class_dataset": str(three_class_root),
        "binary_dataset": str(binary_root),
    }
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
