from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .errors import CliError, EXIT_SCHEMA_MISMATCH


DEFAULT_BINARY_CLASSES = ["0", "1"]
DEFAULT_BINARY_DISPLAY_LABELS = {"0": "Non-target", "1": "Target"}
DEFAULT_BINARY_ALIASES = {
    "0": ["0", "Non-target", "non-target", "Nontarget", "Empty", "empty", "Waste", "waste"],
    "1": ["1", "Target", "target", "Single", "single", "Hit", "Hits", "hit", "hits"],
}
DEFAULT_TERNARY_CLASSES = ["0", "1", "2"]
DEFAULT_TERNARY_DISPLAY_LABELS = {"0": "Non-target A", "1": "Target", "2": "Non-target B"}
DEFAULT_TERNARY_ALIASES = {
    "0": ["0", "Non-target A", "non-target a", "NonTargetA", "Empty", "empty", "Waste", "waste"],
    "1": ["1", "Target", "target", "Single", "single", "Hit", "Hits", "hit", "hits"],
    "2": ["2", "Non-target B", "non-target b", "NonTargetB", "MoreThanTwo", "MoreThan2", "More than two", ">2", "Multiple"],
}
DEFAULT_EXCLUDED_LABELS = ["Reject", "reject", "Rejected", "rejected", "exclude", "Exclude", ""]

LEGACY_CLASSES = ["Empty", "Single", "MoreThanTwo"]
LEGACY_DISPLAY_LABELS = {"Empty": "Empty / Waste", "Single": "Single", "MoreThanTwo": "More Than Two"}
LEGACY_ALIASES = {
    "Empty": ["Empty", "empty", "Waste", "waste", "0"],
    "Single": ["Single", "single", "1"],
    "MoreThanTwo": ["MoreThanTwo", "MoreThan2", ">2", "2", "Multiple"],
}


@dataclass
class ClassSchema:
    classes: list[str]
    display_labels: dict[str, str]
    aliases: dict[str, list[str]] = field(default_factory=dict)
    excluded_labels: list[str] = field(default_factory=lambda: list(DEFAULT_EXCLUDED_LABELS))
    schema_id: str = "droplet-labels-binary-v1"

    @property
    def class_to_idx(self) -> dict[str, int]:
        return {class_id: index for index, class_id in enumerate(self.classes)}

    def alias_to_class(self) -> dict[str, str]:
        mapping: dict[str, str] = {}
        for class_id in self.classes:
            mapping[class_id] = class_id
        for class_id, aliases in self.aliases.items():
            for alias in aliases:
                if alias in mapping and mapping[alias] != class_id:
                    raise CliError(
                        "DUPLICATE_ALIAS",
                        f"Alias '{alias}' maps to multiple classes.",
                        EXIT_SCHEMA_MISMATCH,
                        {"alias": alias, "first": mapping[alias], "second": class_id},
                    )
                mapping[alias] = class_id
        return mapping

    def normalize_label(self, label: str) -> tuple[str | None, str]:
        if label in self.excluded_labels:
            return None, "excluded"
        mapping = self.alias_to_class()
        if label in mapping:
            return mapping[label], "canonical" if mapping[label] == label else "alias"
        return None, "unknown"

    def to_manifest_classes(self) -> list[dict[str, Any]]:
        return [
            {
                "id": class_id,
                "index": index,
                "display_name": self.display_labels.get(class_id, class_id),
                "folder": f"labeled/{class_id}",
            }
            for index, class_id in enumerate(self.classes)
        ]


def default_display_labels_for_classes(classes: list[str]) -> dict[str, str]:
    if classes == DEFAULT_BINARY_CLASSES:
        return dict(DEFAULT_BINARY_DISPLAY_LABELS)
    if classes == DEFAULT_TERNARY_CLASSES:
        return dict(DEFAULT_TERNARY_DISPLAY_LABELS)
    if classes == LEGACY_CLASSES:
        return dict(LEGACY_DISPLAY_LABELS)
    return {class_id: class_id for class_id in classes}


def default_aliases_for_classes(classes: list[str]) -> dict[str, list[str]]:
    if classes == DEFAULT_BINARY_CLASSES:
        return {key: list(value) for key, value in DEFAULT_BINARY_ALIASES.items()}
    if classes == DEFAULT_TERNARY_CLASSES:
        return {key: list(value) for key, value in DEFAULT_TERNARY_ALIASES.items()}
    if classes == LEGACY_CLASSES:
        return {key: list(value) for key, value in LEGACY_ALIASES.items()}
    return {class_id: [class_id] for class_id in classes}


def default_schema_id_for_classes(classes: list[str]) -> str:
    if classes == DEFAULT_BINARY_CLASSES:
        return "droplet-labels-target-nontarget-binary-v1"
    if classes == DEFAULT_TERNARY_CLASSES:
        return "droplet-labels-target-nontarget-3class-v1"
    if classes == LEGACY_CLASSES:
        return "droplet-labels-legacy-v1"
    return "custom-inline"


def default_binary_schema() -> ClassSchema:
    return ClassSchema(
        classes=list(DEFAULT_BINARY_CLASSES),
        display_labels=default_display_labels_for_classes(DEFAULT_BINARY_CLASSES),
        aliases=default_aliases_for_classes(DEFAULT_BINARY_CLASSES),
        schema_id=default_schema_id_for_classes(DEFAULT_BINARY_CLASSES),
    )


def legacy_schema() -> ClassSchema:
    return ClassSchema(
        classes=list(LEGACY_CLASSES),
        display_labels=default_display_labels_for_classes(LEGACY_CLASSES),
        aliases=default_aliases_for_classes(LEGACY_CLASSES),
        schema_id=default_schema_id_for_classes(LEGACY_CLASSES),
    )


def _manifest_path_for_dataset(dataset_arg: str | None) -> Path | None:
    if not dataset_arg:
        return None
    path = Path(dataset_arg).expanduser().resolve()
    if not path.exists():
        return None
    if path.is_file():
        return path
    manifest = path / "metadata" / "dataset_manifest.json"
    return manifest if manifest.is_file() else None


def _ordered_manifest_class_entries(raw_classes: Any) -> list[Any]:
    if not isinstance(raw_classes, list):
        return []
    entries = list(raw_classes)
    indexed: list[tuple[int, Any]] = []
    for entry in entries:
        if not isinstance(entry, dict) or entry.get("index") is None:
            return entries
        try:
            indexed.append((int(entry["index"]), entry))
        except (TypeError, ValueError):
            return entries
    return [entry for _, entry in sorted(indexed, key=lambda item: item[0])]


def _class_ids_and_labels(raw_classes: Any) -> tuple[list[str], dict[str, str]]:
    classes: list[str] = []
    display_labels: dict[str, str] = {}
    for entry in _ordered_manifest_class_entries(raw_classes):
        if isinstance(entry, dict):
            class_id = str(entry.get("id", entry.get("class_id", entry.get("label", "")))).strip()
            if not class_id:
                continue
            display_name = str(entry.get("display_name", entry.get("display_label", entry.get("label", "")))).strip()
        else:
            class_id = str(entry).strip()
            display_name = ""
        if not class_id or class_id in classes:
            continue
        classes.append(class_id)
        if display_name:
            display_labels[class_id] = display_name
    return classes, display_labels


def _manifest_display_labels(raw: Any, classes: list[str]) -> dict[str, str]:
    if not isinstance(raw, dict):
        return {}
    return {class_id: str(raw[class_id]).strip() for class_id in classes if raw.get(class_id) is not None and str(raw[class_id]).strip()}


def _manifest_aliases(raw: Any) -> dict[str, list[str]]:
    if not isinstance(raw, dict):
        return {}
    aliases: dict[str, list[str]] = {}
    for class_id, values in raw.items():
        if isinstance(values, list):
            aliases[str(class_id)] = [str(value) for value in values]
        elif values is not None:
            aliases[str(class_id)] = [str(values)]
    return aliases


def _target_policy_display_labels(raw: Any, classes: list[str]) -> dict[str, str]:
    if not isinstance(raw, dict):
        return {}
    labels: dict[str, str] = {}
    target_id = str(raw.get("target_class_id", "")).strip()
    target_display = str(raw.get("target_display_label", "")).strip()
    if target_id in classes and target_display:
        labels[target_id] = target_display
    waste_id = str(raw.get("waste_class_id", "")).strip()
    waste_display = str(raw.get("waste_display_label", "")).strip()
    if waste_id in classes and waste_display:
        labels[waste_id] = waste_display
    return labels


def _schema_from_manifest(manifest: dict[str, Any]) -> ClassSchema | None:
    class_schema = manifest.get("class_schema", {})
    if not isinstance(class_schema, dict):
        class_schema = {}

    classes, display_labels = _class_ids_and_labels(class_schema.get("classes"))
    if not classes:
        classes, display_labels = _class_ids_and_labels(manifest.get("classes"))
    if not classes and isinstance(manifest.get("class_semantics"), dict):
        semantics = manifest["class_semantics"]
        classes = sorted((str(class_id) for class_id in semantics), key=lambda value: int(value) if value.isdigit() else value)
        display_labels = {class_id: str(semantics[class_id]) for class_id in classes}
    if not classes:
        records = manifest.get("items") or manifest.get("records") or []
        if isinstance(records, list):
            observed = {str(item.get("class_id", "")).strip() for item in records if isinstance(item, dict)}
            observed.discard("")
            if observed:
                classes = sorted(observed, key=lambda value: int(value) if value.isdigit() else value)
                display_labels = default_display_labels_for_classes(classes)
    if not classes:
        return None

    top_classes, top_display_labels = _class_ids_and_labels(manifest.get("classes"))
    merged_display_labels: dict[str, str] = {}
    if top_classes == classes:
        merged_display_labels.update(top_display_labels)
    merged_display_labels.update(display_labels)
    display_labels = {**default_display_labels_for_classes(classes), **merged_display_labels}
    display_labels.update(_target_policy_display_labels(manifest.get("target_policy"), classes))
    display_labels.update(_target_policy_display_labels(class_schema.get("target_policy"), classes))
    display_labels.update(_manifest_display_labels(manifest.get("display_labels"), classes))
    display_labels.update(_manifest_display_labels(class_schema.get("display_labels"), classes))
    for class_id in classes:
        display_labels.setdefault(class_id, class_id)

    aliases = default_aliases_for_classes(classes)
    aliases.update(_manifest_aliases(manifest.get("aliases")))
    aliases.update(_manifest_aliases(class_schema.get("aliases")))

    excluded = list(DEFAULT_EXCLUDED_LABELS)
    excluded_label = class_schema.get("excluded_label")
    if isinstance(excluded_label, dict):
        excluded_id = str(excluded_label.get("id", "")).strip()
        if excluded_id and excluded_id not in excluded:
            excluded.append(excluded_id)
    excluded_raw = class_schema.get("excluded_labels", manifest.get("excluded_labels"))
    if isinstance(excluded_raw, list):
        for value in excluded_raw:
            text = str(value)
            if text not in excluded:
                excluded.append(text)

    schema_id = str(
        manifest.get("label_schema_version")
        or class_schema.get("label_schema_version")
        or class_schema.get("schema_id")
        or class_schema.get("kind")
        or default_schema_id_for_classes(classes)
    )
    return ClassSchema(classes=classes, display_labels=display_labels, aliases=aliases, excluded_labels=excluded, schema_id=schema_id)


def infer_schema_from_dataset(dataset_arg: str | None) -> ClassSchema | None:
    manifest_path = _manifest_path_for_dataset(dataset_arg)
    if manifest_path is None:
        return None
    try:
        with manifest_path.open("r", encoding="utf-8-sig") as handle:
            manifest = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise CliError("DATASET_CLASS_METADATA_INVALID", "Dataset class metadata could not be read.",
                       EXIT_SCHEMA_MISMATCH, {"path": str(manifest_path), "error": str(exc)}) from exc
    if not isinstance(manifest, dict):
        raise CliError("DATASET_CLASS_METADATA_INVALID", "Dataset class metadata must be a JSON object.",
                       EXIT_SCHEMA_MISMATCH, {"path": str(manifest_path)})
    schema = _schema_from_manifest(manifest)
    if schema is None:
        raise CliError("DATASET_CLASS_METADATA_MISSING",
                       "Dataset metadata does not declare class IDs. Add class_schema, classes, class_semantics, or record class_id values.",
                       EXIT_SCHEMA_MISMATCH, {"path": str(manifest_path)})
    return schema


def parse_schema(args: Any) -> ClassSchema:
    if getattr(args, "class_schema", None):
        return load_schema(Path(args.class_schema))
    if getattr(args, "legacy_schema", False):
        return legacy_schema()
    classes_arg = getattr(args, "classes", None)
    if classes_arg:
        classes = [part.strip() for part in classes_arg.split(",") if part.strip()]
        if not classes:
            raise CliError("EMPTY_CLASS_SCHEMA", "--classes did not contain any class ids.", EXIT_SCHEMA_MISMATCH)
        return ClassSchema(
            classes=classes,
            display_labels=default_display_labels_for_classes(classes),
            aliases=default_aliases_for_classes(classes),
            schema_id=default_schema_id_for_classes(classes),
        )
    inferred = infer_schema_from_dataset(getattr(args, "dataset", None))
    if inferred is not None:
        return inferred
    return default_binary_schema()


def load_schema(path: Path) -> ClassSchema:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    classes = [str(item) for item in data.get("classes", [])]
    if not classes:
        raise CliError("EMPTY_CLASS_SCHEMA", "Class schema file has no classes.", EXIT_SCHEMA_MISMATCH, {"path": str(path)})
    display_labels = default_display_labels_for_classes(classes)
    display_labels.update({str(key): str(value) for key, value in data.get("display_labels", {}).items()})
    for class_id in classes:
        display_labels.setdefault(class_id, class_id)
    aliases_raw = data.get("aliases", {})
    aliases = default_aliases_for_classes(classes)
    aliases.update({str(key): [str(value) for value in values] for key, values in aliases_raw.items()})
    excluded = [str(value) for value in data.get("excluded_labels", DEFAULT_EXCLUDED_LABELS)]
    return ClassSchema(
        classes=classes,
        display_labels=display_labels,
        aliases=aliases,
        excluded_labels=excluded,
        schema_id=str(data.get("label_schema_version", default_schema_id_for_classes(classes))),
    )


def compute_inverse_count_weights(counts: dict[str, int], classes: list[str]) -> dict[str, float]:
    raw: list[float] = []
    for class_id in classes:
        count = int(counts.get(class_id, 0))
        if count <= 0:
            raise CliError(
                "INSUFFICIENT_CLASS_EXAMPLES",
                f"Class '{class_id}' has no included examples.",
                EXIT_SCHEMA_MISMATCH,
                {"class": class_id, "count": count},
            )
        raw.append(1.0 / float(count))
    min_raw = min(raw)
    return {class_id: raw[index] / min_raw for index, class_id in enumerate(classes)}
