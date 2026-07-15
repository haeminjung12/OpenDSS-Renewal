from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .errors import CliError, EXIT_SCHEMA_MISMATCH


DEFAULT_BINARY_CLASSES = ["0", "1"]
DEFAULT_BINARY_DISPLAY_LABELS = {"0": "Non-target", "1": "Target"}
DEFAULT_BINARY_ALIASES = {
    "0": ["0", "Non-target", "non-target", "Nontarget", "Empty", "empty", "Waste", "waste", "MoreThanTwo", "MoreThan2", "More than two", ">2", "2", "Multiple"],
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
