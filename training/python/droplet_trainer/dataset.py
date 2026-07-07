from __future__ import annotations

import csv
import hashlib
import json
import random
import re
import shutil
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .errors import CliError, EXIT_DATASET_MISSING, EXIT_MANIFEST_INVALID, EXIT_OUTPUT_INVALID, EXIT_SCHEMA_MISMATCH, EXIT_SPLIT_INVALID
from .schema import ClassSchema, compute_inverse_count_weights


IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"}
PATH_PATTERN = re.compile(r"[A-Za-z]:\\[^`\n\r,;]+")
DATASET_BUILDER_MANIFEST_VERSION = "dataset-builder-manifest-v1"
TRAINER_REVIEW_STATES = {"user_confirmed", "confirmed", "relabeled"}
UNREVIEWED_STATES = {"", "unreviewed", "auto_labeled_unreviewed"}
SEED_OR_EXISTING_SOURCE_KINDS = {"seed", "bundled_seed", "bundled_empty_waste_seed", "existing_waste_seed", "existing_user_import", "legacy_existing"}
WASTE_SOURCE_MODES = {"new-reviewed-only", "existing-reviewed-only", "mixed-reviewed"}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def repo_root_from_training_package() -> Path:
    return Path(__file__).resolve().parents[3]


def discover_dataset_references(repo_root: Path | None = None, limit: int = 50) -> list[dict[str, str]]:
    root = repo_root or repo_root_from_training_package()
    search_roots = [root / "docs", root / "open-visual-droplet-sorter-suite" / "docs"]
    references: list[dict[str, str]] = []
    seen: set[tuple[str, str]] = set()
    for search_root in search_roots:
        if not search_root.exists():
            continue
        for path in search_root.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in {".md", ".csv", ".txt", ".json"}:
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            for match in PATH_PATTERN.findall(text):
                cleaned = match.strip().rstrip(".")
                key = (str(path.relative_to(root)), cleaned)
                if key in seen:
                    continue
                seen.add(key)
                references.append({"record": str(path.relative_to(root)), "path": cleaned})
                if len(references) >= limit:
                    return references
    return references


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8-sig") as handle:
            return json.load(handle)
    except Exception as exc:
        raise CliError("DATASET_MANIFEST_INVALID", "Dataset manifest could not be read.", EXIT_MANIFEST_INVALID, {"path": str(path), "error": str(exc)})


def resolve_dataset_path(dataset_arg: str) -> tuple[Path, Path | None, str]:
    path = Path(dataset_arg).expanduser().resolve()
    if not path.exists():
        raise CliError("DATASET_PATH_MISSING", "Dataset path does not exist.", EXIT_DATASET_MISSING, {"path": str(path)})
    if path.is_file():
        return path.parent.parent if path.name == "dataset_manifest.json" else path.parent, path, "manifest"
    manifest = path / "metadata" / "dataset_manifest.json"
    if manifest.exists():
        return path, manifest, "manifest"
    return path, None, "folder"


def _candidate_label_roots(dataset_root: Path) -> list[Path]:
    labeled = dataset_root / "labeled"
    if labeled.is_dir():
        return [labeled]
    return [dataset_root]


def _empty_scan_summary(schema_version: str | None = None) -> dict[str, Any]:
    return {
        "manifest_schema_version": schema_version,
        "review_state_counts": Counter(),
        "excluded_count": 0,
        "unreviewed_count": 0,
        "invalid_eligible_items": [],
        "source_kind_counts": Counter(),
        "waste_source_counts": Counter(),
    }


def _scan_folder_mode(dataset_root: Path, schema: ClassSchema) -> tuple[list[dict[str, Any]], list[dict[str, Any]], Counter[str], dict[str, Any]]:
    items: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    rejected = Counter()
    summary = _empty_scan_summary()
    seen: set[Path] = set()
    for label_root in _candidate_label_roots(dataset_root):
        if not label_root.is_dir():
            continue
        for label_dir in sorted([entry for entry in label_root.iterdir() if entry.is_dir()]):
            original_label = label_dir.name
            class_id, label_status = schema.normalize_label(original_label)
            if label_status == "excluded":
                count = len([path for path in label_dir.rglob("*") if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS])
                rejected[original_label] += count
                continue
            if class_id is None:
                warnings.append({"code": "UNKNOWN_LABEL_FOLDER", "message": "Folder label is not in the configured schema.", "details": {"label": original_label, "path": str(label_dir)}})
                continue
            for image_path in sorted(label_dir.rglob("*")):
                if not image_path.is_file() or image_path.suffix.lower() not in IMAGE_EXTENSIONS:
                    continue
                resolved = image_path.resolve()
                if resolved in seen:
                    continue
                seen.add(resolved)
                items.append(
                    {
                        "source_path": str(resolved),
                        "path": str(resolved.relative_to(dataset_root)) if resolved.is_relative_to(dataset_root) else str(resolved),
                        "original_label": original_label,
                        "class_id": class_id,
                        "label_status": label_status,
                        "role": "unknown",
                        "origin": "folder_scan",
                    }
                )
    summary["excluded_count"] = sum(rejected.values())
    return items, warnings, rejected, summary


def _dataset_builder_label_map(manifest: dict[str, Any], schema: ClassSchema, manifest_path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    classes = manifest.get("classes", [])
    if classes is None:
        classes = []
    if not isinstance(classes, list):
        raise CliError("DATASET_MANIFEST_INVALID", "Dataset Builder manifest classes must be a list.", EXIT_MANIFEST_INVALID, {"path": str(manifest_path)})
    for entry in classes:
        if not isinstance(entry, dict):
            continue
        class_id = str(entry.get("id", ""))
        dataset_label = str(entry.get("dataset_label", entry.get("label", "")))
        if not class_id or not dataset_label:
            continue
        if class_id not in schema.classes:
            raise CliError("CLASS_SCHEMA_MISMATCH", "Dataset Builder class id is not in the selected class schema.", EXIT_SCHEMA_MISMATCH, {"class_id": class_id, "dataset_label": dataset_label})
        result[dataset_label] = class_id

    if not result:
        class_schema = manifest.get("class_schema", {})
        if not isinstance(class_schema, dict):
            raise CliError("DATASET_MANIFEST_INVALID", "Dataset Builder manifest class_schema must be an object.", EXIT_MANIFEST_INVALID, {"path": str(manifest_path)})
        schema_classes = class_schema.get("classes", [])
        if not isinstance(schema_classes, list):
            raise CliError("DATASET_MANIFEST_INVALID", "Dataset Builder manifest class_schema.classes must be a list.", EXIT_MANIFEST_INVALID, {"path": str(manifest_path)})
        for entry in schema_classes:
            if not isinstance(entry, dict):
                continue
            dataset_label = str(entry.get("id", ""))
            index = entry.get("index")
            if not dataset_label or index is None:
                continue
            try:
                class_index = int(index)
            except (TypeError, ValueError):
                raise CliError("CLASS_SCHEMA_MISMATCH", "Dataset Builder class_schema class index is not an integer.", EXIT_SCHEMA_MISMATCH, {"dataset_label": dataset_label, "index": index})
            if class_index < 0 or class_index >= len(schema.classes):
                raise CliError("CLASS_SCHEMA_MISMATCH", "Dataset Builder class_schema class index is outside the selected class schema.", EXIT_SCHEMA_MISMATCH, {"dataset_label": dataset_label, "index": class_index, "schema_classes": schema.classes})
            result[dataset_label] = schema.classes[class_index]

    missing = [class_id for class_id in schema.classes if class_id not in set(result.values())]
    if missing:
        raise CliError("CLASS_SCHEMA_MISMATCH", "Dataset Builder manifest is missing class mapping for selected schema classes.", EXIT_SCHEMA_MISMATCH, {"missing_classes": missing})
    return result


def _is_dataset_builder_manifest(manifest: dict[str, Any]) -> bool:
    return manifest.get("schema_version") == DATASET_BUILDER_MANIFEST_VERSION


def _item_image_path(raw: dict[str, Any], dataset_root: Path) -> Path:
    raw_path = raw.get("source_path") or raw.get("crop_path") or raw.get("path") or ""
    source_path = Path(str(raw_path))
    if not source_path.is_absolute():
        source_path = dataset_root / source_path
    return source_path


def _scan_manifest_mode(dataset_root: Path, manifest_path: Path, schema: ClassSchema) -> tuple[list[dict[str, Any]], list[dict[str, Any]], Counter[str], dict[str, Any]]:
    manifest = load_manifest(manifest_path)
    manifest_schema_version = str(manifest.get("schema_version", ""))
    builder_mode = _is_dataset_builder_manifest(manifest)
    builder_label_map = _dataset_builder_label_map(manifest, schema, manifest_path) if builder_mode else {}
    raw_items = manifest.get("items", [])
    if not isinstance(raw_items, list):
        raise CliError("DATASET_MANIFEST_INVALID", "Manifest items must be a list.", EXIT_MANIFEST_INVALID, {"path": str(manifest_path)})
    items: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    rejected = Counter()
    summary = _empty_scan_summary(manifest_schema_version)
    for raw in raw_items:
        if not isinstance(raw, dict):
            warnings.append({"code": "DATASET_MANIFEST_INVALID_ITEM", "message": "Manifest item is not an object."})
            continue
        review_state = str(raw.get("review_state", raw.get("status", "included")))
        summary["review_state_counts"][review_state] += 1
        source_kind = str(raw.get("source_kind", raw.get("provenance", {}).get("origin", raw.get("origin", "manifest"))))
        summary["source_kind_counts"][source_kind] += 1
        if builder_mode:
            label = str(raw.get("review_label", raw.get("reviewed_label", raw.get("label", ""))))
            class_id = str(raw.get("class_id", "")) if raw.get("class_id") is not None else builder_label_map.get(label)
        else:
            label = str(raw.get("label", raw.get("class_id", "")))
            class_id = ""
        status = str(raw.get("status", "included"))
        source_path = _item_image_path(raw, dataset_root)
        normalized_class_id, label_status = schema.normalize_label(label)
        if builder_mode and class_id:
            if class_id not in schema.classes:
                warnings.append({"code": "CLASS_SCHEMA_MISMATCH", "message": "Dataset Builder item class_id is not in the selected class schema.", "details": {"image_id": raw.get("image_id"), "class_id": class_id}})
                continue
            normalized_class_id = class_id
            label_status = "canonical" if label == class_id else "dataset_builder_label"
        if status in {"rejected", "excluded"} or review_state == "excluded" or label_status == "excluded" or label == "exclude":
            rejected[label] += 1
            summary["excluded_count"] += 1
            continue
        if builder_mode and review_state in UNREVIEWED_STATES:
            rejected["unreviewed"] += 1
            summary["unreviewed_count"] += 1
            if raw.get("trainer_eligible") is True:
                summary["invalid_eligible_items"].append({"image_id": raw.get("image_id"), "review_state": review_state, "label": label})
            continue
        if builder_mode and review_state not in TRAINER_REVIEW_STATES:
            rejected[review_state or "unreviewed"] += 1
            if raw.get("trainer_eligible") is True:
                summary["invalid_eligible_items"].append({"image_id": raw.get("image_id"), "review_state": review_state, "label": label})
            continue
        if normalized_class_id is None:
            warnings.append({"code": "UNKNOWN_MANIFEST_LABEL", "message": "Manifest label is not in the configured schema.", "details": {"label": label}})
            continue
        if normalized_class_id == "0":
            summary["waste_source_counts"][source_kind] += 1
        items.append(
            {
                "source_path": str(source_path.resolve()),
                "path": str(raw.get("path", source_path.name)),
                "original_label": label,
                "class_id": normalized_class_id,
                "label_status": label_status,
                "role": str(raw.get("split", raw.get("role", "unknown"))),
                "origin": raw.get("provenance", {}).get("origin", raw.get("origin", "manifest")),
                "source_kind": source_kind,
                "image_id": raw.get("image_id"),
                "review_state": review_state,
            }
        )
    return items, warnings, rejected, summary


def scan_dataset(dataset_arg: str, schema: ClassSchema) -> dict[str, Any]:
    dataset_root, manifest_path, mode = resolve_dataset_path(dataset_arg)
    if manifest_path:
        items, warnings, rejected, summary = _scan_manifest_mode(dataset_root, manifest_path, schema)
    else:
        items, warnings, rejected, summary = _scan_folder_mode(dataset_root, schema)
    counts = Counter(item["class_id"] for item in items)
    alias_counts = Counter(item["original_label"] for item in items if item["label_status"] == "alias")
    return {
        "dataset_root": dataset_root,
        "manifest_path": manifest_path,
        "mode": mode,
        "items": items,
        "counts": counts,
        "alias_counts": alias_counts,
        "rejected_counts": rejected,
        "summary": summary,
        "warnings": warnings,
    }


def inspect_dataset(args: Any, schema: ClassSchema) -> dict[str, Any]:
    scan = scan_dataset(args.dataset, schema)
    warnings = list(scan["warnings"])
    if scan["manifest_path"] is None:
        warnings.append({"code": "DATASET_MANIFEST_MISSING", "message": "No metadata/dataset_manifest.json was found; folder-mode inspection was used."})
    for class_id in schema.classes:
        if scan["counts"].get(class_id, 0) == 0:
            warnings.append({"code": "CLASS_HAS_NO_EXAMPLES", "message": "Configured class has no examples.", "details": {"class": class_id}})
    warnings.extend(class_balance_warnings(dict(scan["counts"]), schema))
    warnings.extend(source_warnings(scan["summary"]))
    return {
        "schema_version": 1,
        "command": "dataset-inspect",
        "status": "ok",
        "timestamp": utc_now(),
        "dataset": {
            "path": str(scan["dataset_root"]),
            "mode": scan["mode"],
            "manifest_path": str(scan["manifest_path"]) if scan["manifest_path"] else None,
            "samples_total": len(scan["items"]),
        },
        "class_schema": schema_payload(schema),
        "class_counts": dict(scan["counts"]),
        "alias_counts": dict(scan["alias_counts"]),
        "rejected_counts": dict(scan["rejected_counts"]),
        "review_state_counts": dict(scan["summary"]["review_state_counts"]),
        "excluded_count": int(scan["summary"]["excluded_count"]),
        "unreviewed_count": int(scan["summary"]["unreviewed_count"]),
        "source_kind_counts": dict(scan["summary"]["source_kind_counts"]),
        "waste_source_counts": dict(scan["summary"]["waste_source_counts"]),
        "dataset_source_references": discover_dataset_references(limit=25),
        "warnings": warnings,
    }


def schema_payload(schema: ClassSchema) -> dict[str, Any]:
    return {
        "label_schema_version": schema.schema_id,
        "classes": schema.classes,
        "class_to_idx": schema.class_to_idx,
        "display_labels": schema.display_labels,
        "aliases": schema.aliases,
        "excluded_labels": schema.excluded_labels,
    }


def parse_split(split: str) -> dict[str, float]:
    result: dict[str, float] = {}
    for part in split.split(","):
        if not part.strip():
            continue
        key, value = part.split("=", 1)
        result[key.strip()] = float(value)
    required = {"train", "val", "test"}
    if set(result) != required or abs(sum(result.values()) - 1.0) > 1e-6 or any(value < 0 for value in result.values()):
        raise CliError("INVALID_SPLIT", "Split must define train,val,test ratios summing to 1.0.", EXIT_SPLIT_INVALID, {"split": split})
    return result


def class_balance_warnings(counts: dict[str, int], schema: ClassSchema) -> list[dict[str, Any]]:
    values = [int(counts.get(class_id, 0)) for class_id in schema.classes]
    if not values or any(value <= 0 for value in values):
        return []
    minority = min(values)
    majority = max(values)
    ratio = minority / majority if majority else 1.0
    if ratio < 0.10:
        return [{"code": "CLASS_IMBALANCE_SEVERE", "message": "Minority/majority class ratio is below 0.10.", "details": {"minority": minority, "majority": majority, "ratio": ratio}}]
    if ratio < 0.25:
        return [{"code": "CLASS_IMBALANCE_MODERATE", "message": "Minority/majority class ratio is below 0.25.", "details": {"minority": minority, "majority": majority, "ratio": ratio}}]
    return []


def source_warnings(summary: dict[str, Any]) -> list[dict[str, Any]]:
    warnings: list[dict[str, Any]] = []
    waste_source_counts = dict(summary.get("waste_source_counts", {}))
    total_waste = sum(int(value) for value in waste_source_counts.values())
    if total_waste:
        seed_or_existing = sum(int(value) for key, value in waste_source_counts.items() if key in SEED_OR_EXISTING_SOURCE_KINDS or "seed" in key.lower() or "existing" in key.lower())
        if seed_or_existing / total_waste > 0.5:
            warnings.append({"code": "WASTE_SOURCE_SKEW", "message": "Waste class is mostly seed or existing images.", "details": {"seed_or_existing_waste": seed_or_existing, "total_waste": total_waste}})
    return warnings


def _is_existing_waste_source(source_kind: str) -> bool:
    lowered = source_kind.lower()
    return source_kind in SEED_OR_EXISTING_SOURCE_KINDS or "seed" in lowered or "existing" in lowered or "legacy" in lowered


def _is_waste_item(item: dict[str, Any], schema: ClassSchema) -> bool:
    waste_class = "0" if "0" in schema.classes else schema.classes[0]
    return item.get("class_id") == waste_class


def waste_source_selection(scan: dict[str, Any], schema: ClassSchema, mode: str) -> dict[str, Any]:
    if mode not in WASTE_SOURCE_MODES:
        raise CliError("CLI_USAGE_ERROR", "Unsupported waste source mode.", EXIT_SCHEMA_MISMATCH, {"mode": mode, "allowed_modes": sorted(WASTE_SOURCE_MODES)})

    new_count = 0
    existing_count = 0
    for item in scan["items"]:
        if not _is_waste_item(item, schema):
            continue
        source_kind = str(item.get("source_kind", item.get("origin", "")))
        if _is_existing_waste_source(source_kind):
            existing_count += 1
        else:
            new_count += 1

    selected_existing = existing_count if mode in {"existing-reviewed-only", "mixed-reviewed"} else 0
    selected_new = new_count if mode in {"new-reviewed-only", "mixed-reviewed"} else 0
    selected_total = selected_existing + selected_new
    return {
        "mode": mode,
        "selected_waste_count": selected_total,
        "selected_new_reviewed_waste_count": selected_new,
        "selected_existing_reviewed_waste_count": selected_existing,
        "available_new_reviewed_waste_count": new_count,
        "available_existing_reviewed_waste_count": existing_count,
        "allowed_modes": sorted(WASTE_SOURCE_MODES),
        "custom_waste_folder_allowed": False,
        "provenance_requirement": "Existing reviewed waste must already be present in the Dataset Builder manifest with source_kind/provenance and hashes.",
    }


def apply_waste_source_mode(scan: dict[str, Any], schema: ClassSchema, mode: str) -> dict[str, Any]:
    selection = waste_source_selection(scan, schema, mode)
    if mode == "mixed-reviewed":
        scan["waste_source_selection"] = selection
        return scan

    kept: list[dict[str, Any]] = []
    for item in scan["items"]:
        if not _is_waste_item(item, schema):
            kept.append(item)
            continue
        source_kind = str(item.get("source_kind", item.get("origin", "")))
        is_existing = _is_existing_waste_source(source_kind)
        if mode == "existing-reviewed-only" and is_existing:
            kept.append(item)
        elif mode == "new-reviewed-only" and not is_existing:
            kept.append(item)

    scan["items"] = kept
    scan["counts"] = Counter(item["class_id"] for item in kept)
    scan["waste_source_selection"] = selection
    return scan


def split_support_errors(split_counts: dict[str, dict[str, int]], schema: ClassSchema) -> list[dict[str, Any]]:
    errors: list[dict[str, Any]] = []
    for role in ["val", "test"]:
        for class_id in schema.classes:
            count = int(split_counts.get(role, {}).get(class_id, 0))
            if count <= 0:
                errors.append({"code": "INVALID_SPLIT_SUPPORT", "message": "Split cannot include every class.", "details": {"split": role, "class": class_id, "count": count}})
    return errors


def split_support_warnings(split_counts: dict[str, dict[str, int]], schema: ClassSchema) -> list[dict[str, Any]]:
    warnings: list[dict[str, Any]] = []
    for role in ["val", "test"]:
        for class_id in schema.classes:
            count = int(split_counts.get(role, {}).get(class_id, 0))
            if 0 < count < 5:
                warnings.append({"code": "VALIDATION_SUPPORT_LOW", "message": "Validation/test split has fewer than 5 examples for a class.", "details": {"split": role, "class": class_id, "count": count}})
    return warnings


def assign_splits(items: list[dict[str, Any]], schema: ClassSchema, split: dict[str, float], seed: int) -> None:
    rng = random.Random(seed)
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for item in items:
        grouped[item["class_id"]].append(item)
    for class_id in schema.classes:
        group = grouped.get(class_id, [])
        rng.shuffle(group)
        n = len(group)
        if n == 0:
            continue
        train_end = int(round(n * split["train"]))
        val_end = train_end + int(round(n * split["val"]))
        if n >= 3:
            train_end = max(1, min(train_end, n - 2))
            val_end = max(train_end + 1, min(val_end, n - 1))
        for index, item in enumerate(group):
            if index < train_end:
                item["role"] = "train"
            elif index < val_end:
                item["role"] = "val"
            else:
                item["role"] = "test"


def validate_images_openable(items: list[dict[str, Any]]) -> list[dict[str, Any]]:
    try:
        from PIL import Image
    except Exception as exc:
        return [{"code": "PILLOW_UNAVAILABLE", "message": "Pillow is not available; image-open validation was skipped.", "details": {"error": str(exc)}}]
    errors: list[dict[str, Any]] = []
    for item in items:
        path = Path(item["source_path"])
        try:
            with Image.open(path) as image:
                image.verify()
        except Exception as exc:
            errors.append({"code": "IMAGE_OPEN_FAILED", "message": "Image could not be opened by Pillow.", "details": {"path": str(path), "error": str(exc)}})
    return errors


def write_csv(path: Path, rows: list[dict[str, Any]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fieldnames})


def write_manifest(path: Path, dataset_id: str, source_dataset: Path, schema: ClassSchema, items: list[dict[str, Any]], seed: int, split: dict[str, float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    manifest_items = []
    for index, item in enumerate(items):
        manifest_items.append(
            {
                "image_id": f"img-{index:06d}",
                "path": item["path"],
                "source_path": item["source_path"],
                "original_label": item["original_label"],
                "label": item["class_id"],
                "hash_sha256": item.get("hash_sha256"),
                "split": item.get("role", "unknown"),
                "provenance": {
                    "origin": item.get("origin", "unknown"),
                    "seed_image": item.get("origin") in {"seed", "bundled_empty_waste_seed"},
                    "seed_source": item.get("seed_source"),
                    "redistribution_approved": None,
                },
            }
        )
    payload = {
        "schema_version": "dataset-manifest-v1",
        "dataset_id": dataset_id,
        "created_at": utc_now(),
        "root": str(path.parent.parent.resolve()),
        "source": {"type": "prepared_copy" if "prepared" in path.parts else "dataset_scan", "path": str(source_dataset)},
        "classes": schema.to_manifest_classes(),
        "class_schema": schema_payload(schema),
        "excluded_folders": ["labeled/Reject"],
        "split": split,
        "seed": seed,
        "items": manifest_items,
        "redistribution": {"approval_status": "unknown", "notes": "Candidate seed/demo data is not redistribution-approved by this scaffold."},
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def prepare_dataset(args: Any, schema: ClassSchema, items: list[dict[str, Any]], split: dict[str, float]) -> Path:
    source_root = Path(args.dataset).expanduser().resolve()
    dataset_id = args.dataset_id or source_root.name
    prepared_root = Path(args.prepared_root).expanduser().resolve() if args.prepared_root else (repo_root_from_training_package() / "datasets" / "prepared")
    prepared_dataset = prepared_root / dataset_id
    if prepared_dataset.exists() and any(prepared_dataset.iterdir()) and not args.force:
        raise CliError("OUTPUT_EXISTS", "Prepared dataset output already exists; pass --force to overwrite scaffold outputs.", EXIT_OUTPUT_INVALID, {"path": str(prepared_dataset)})
    image_root = prepared_dataset / "images"
    metadata_dir = prepared_dataset / "metadata"
    image_root.mkdir(parents=True, exist_ok=True)
    metadata_dir.mkdir(parents=True, exist_ok=True)

    used_names: set[Path] = set()
    for index, item in enumerate(items):
        source_path = Path(item["source_path"])
        digest = sha256_file(source_path)
        item["hash_sha256"] = digest
        class_dir = image_root / item["class_id"]
        class_dir.mkdir(parents=True, exist_ok=True)
        target_name = f"{index:06d}_{digest[:12]}{source_path.suffix.lower()}"
        target_path = class_dir / target_name
        while target_path in used_names:
            target_name = f"{index:06d}_{digest[:16]}{source_path.suffix.lower()}"
            target_path = class_dir / target_name
        used_names.add(target_path)
        shutil.copy2(source_path, target_path)
        item["path"] = str(target_path.relative_to(prepared_dataset))
    write_manifest(metadata_dir / "dataset_manifest.json", dataset_id, source_root, schema, items, int(args.seed), split)
    return prepared_dataset


def validate_dataset(args: Any, schema: ClassSchema) -> tuple[dict[str, Any], int]:
    scan = scan_dataset(args.dataset, schema)
    waste_mode = getattr(args, "waste_source_mode", "new-reviewed-only")
    scan = apply_waste_source_mode(scan, schema, waste_mode)
    warnings = list(scan["warnings"])
    errors: list[dict[str, Any]] = []
    for invalid in scan["summary"].get("invalid_eligible_items", []):
        errors.append({"code": "UNREVIEWED_ITEMS_NOT_ELIGIBLE", "message": "Dataset Builder item is trainer-eligible but not manually reviewed.", "details": invalid})
    min_per_class = int(args.min_per_class)
    for class_id in schema.classes:
        count = scan["counts"].get(class_id, 0)
        if count < min_per_class:
            errors.append({"code": "INSUFFICIENT_CLASS_EXAMPLES", "message": "Class has fewer examples than --min-per-class.", "details": {"class": class_id, "count": count, "min_per_class": min_per_class}})
    image_errors = validate_images_openable(scan["items"])
    if image_errors and image_errors[0]["code"] == "PILLOW_UNAVAILABLE":
        warnings.extend(image_errors)
    else:
        errors.extend(image_errors)
    split = parse_split(args.split)
    if not errors:
        assign_splits(scan["items"], schema, split, int(args.seed))
    split_counts: dict[str, dict[str, int]] = {role: {} for role in ["train", "val", "test", "unknown"]}
    for role in split_counts:
        role_counts = Counter(item["class_id"] for item in scan["items"] if item.get("role", "unknown") == role)
        split_counts[role] = dict(role_counts)
    if not errors:
        errors.extend(split_support_errors(split_counts, schema))
    warnings.extend(split_support_warnings(split_counts, schema))
    warnings.extend(class_balance_warnings(dict(scan["counts"]), schema))
    warnings.extend(source_warnings(scan["summary"]))

    output = Path(args.output).expanduser().resolve() if args.output else None
    artifacts: dict[str, str | None] = {"dataset_summary_json": None, "class_balance_csv": None, "dataset_manifest_json": None, "prepared_dataset": None}
    weights = None
    if not errors:
        weights = compute_inverse_count_weights(split_counts["train"], schema.classes) if args.balancing == "inverse-count-min-normalized" else None
        if output:
            output.mkdir(parents=True, exist_ok=True)
            summary_path = output / "dataset_summary.json"
            balance_path = output / "class_balance.csv"
            summary = {
                "schema_version": 1,
                "dataset": str(scan["dataset_root"]),
                "class_schema": schema_payload(schema),
                "class_counts": dict(scan["counts"]),
                "split_counts": split_counts,
                "excluded_count": int(scan["summary"]["excluded_count"]),
                "unreviewed_count": int(scan["summary"]["unreviewed_count"]),
                "review_state_counts": dict(scan["summary"]["review_state_counts"]),
                "source_kind_counts": dict(scan["summary"]["source_kind_counts"]),
                "waste_source_counts": dict(scan["summary"]["waste_source_counts"]),
                "waste_source_selection": scan["waste_source_selection"],
                "balancing": {"mode": args.balancing, "class_weights": weights},
                "warnings": warnings,
            }
            summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
            balance_rows = [
                {"class": class_id, "count": scan["counts"].get(class_id, 0), "weight": "" if weights is None else weights[class_id]}
                for class_id in schema.classes
            ]
            write_csv(balance_path, balance_rows, ["class", "count", "weight"])
            artifacts["dataset_summary_json"] = str(summary_path)
            artifacts["class_balance_csv"] = str(balance_path)
            if args.write_manifest:
                manifest_path = output / "metadata" / "dataset_manifest.json"
                for item in scan["items"]:
                    if "hash_sha256" not in item:
                        item["hash_sha256"] = sha256_file(Path(item["source_path"]))
                write_manifest(manifest_path, args.dataset_id or scan["dataset_root"].name, scan["dataset_root"], schema, scan["items"], int(args.seed), split)
                artifacts["dataset_manifest_json"] = str(manifest_path)
        if args.prepare:
            prepared = prepare_dataset(args, schema, scan["items"], split)
            artifacts["prepared_dataset"] = str(prepared)
            artifacts["dataset_manifest_json"] = str(prepared / "metadata" / "dataset_manifest.json")

    status = "error" if errors else "ok"
    payload = {
        "schema_version": 1,
        "command": "dataset-validate",
        "status": status,
        "timestamp": utc_now(),
        "dataset": {"path": str(scan["dataset_root"]), "mode": scan["mode"], "manifest_path": str(scan["manifest_path"]) if scan["manifest_path"] else None},
        "class_schema": schema_payload(schema),
        "class_counts": dict(scan["counts"]),
        "excluded_count": int(scan["summary"]["excluded_count"]),
        "unreviewed_count": int(scan["summary"]["unreviewed_count"]),
        "review_state_counts": dict(scan["summary"]["review_state_counts"]),
        "source_kind_counts": dict(scan["summary"]["source_kind_counts"]),
        "waste_source_counts": dict(scan["summary"]["waste_source_counts"]),
        "waste_source_selection": scan["waste_source_selection"],
        "split": split,
        "split_counts": split_counts,
        "balancing": {"mode": args.balancing, "class_weights": weights},
        "artifacts": artifacts,
        "dataset_source_references": discover_dataset_references(limit=25),
        "warnings": warnings,
        "errors": errors,
    }
    return payload, EXIT_SPLIT_INVALID if errors else 0
