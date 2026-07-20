from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import random
import shutil
import sys
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

CLASS_NAMES = {"0": "Empty", "1": "Single", "2": "MoreThanOne"}
SCREENING_SEEDS = (1729, 2718)
FINALIST_SEEDS = (1729, 2718, 3141, 5772, 8119)
CONDITION_CATALOG = (
    "source_baseline", "historical_full_5e-4", "staged_relu_1e-4_1e-5", "staged_signed_1e-4_1e-5",
    "unweighted_ce", "inverse_weighted_ce_control", "effective_number_capped", "balanced_batches",
    "balanced_batches_effective_number", "focal_loss", "input96_batch64_amp", "input96_batch32_amp",
    "input64_batch64_amp", "no_amp", "plateau_scheduler", "minority_augmentation",
)
MAX_RUNS = 46

@dataclass(frozen=True)
class Sample:
    path: str
    class_id: str
    sha256: str
    width: int
    height: int
    channels: str

@dataclass(frozen=True)
class DerivedSample:
    record_id: str
    source_dataset_id: str
    source_root: str
    source_item_id: str
    source_path: str
    class_id: str
    sha256: str
    width: int
    height: int
    channels: str
    trainer_eligible: bool
    provenance: dict

    def fold_sample(self) -> Sample:
        return Sample(self.record_id, self.class_id, self.sha256, self.width, self.height, self.channels)

def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

def stable_json_hash(value: object) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()

def scan_dataset(root: Path) -> tuple[list[Sample], list[str]]:
    from PIL import Image
    samples, unreadable = [], []
    for class_id in CLASS_NAMES:
        folder = root / "images" / class_id
        if not folder.is_dir():
            raise ValueError(f"Missing class folder: {folder}")
        for path in sorted(folder.glob("*.png")):
            try:
                digest = sha256_file(path)
                with Image.open(path) as image:
                    image.verify()
                with Image.open(path) as image:
                    width, height = image.size
                    channels = image.mode
                samples.append(Sample(path.relative_to(root).as_posix(), class_id, digest, width, height, channels))
            except Exception as exc:
                unreadable.append(f"{path.relative_to(root).as_posix()}: {type(exc).__name__}: {exc}")
    return samples, unreadable

def perceptual_duplicate_audit(root: Path, samples: Iterable[Sample], threshold: int = 4) -> dict:
    from PIL import Image
    tree: dict = {}
    pair_count = cross_class = 0
    examples: list[list[str]] = []
    cross_class_candidates: list[dict] = []
    def query(node: dict, value: int) -> list[tuple[int, Sample]]:
        found = []
        while node:
            distance = (value ^ node["value"]).bit_count()
            if distance <= threshold:
                found.append((distance, node["sample"]))
            candidates = [child for edge, child in node["children"].items() if distance - threshold <= edge <= distance + threshold]
            found.extend(item for child in candidates for item in query(child, value))
            break
        return found
    def insert(value: int, sample: Sample) -> None:
        nonlocal tree
        if not tree:
            tree = {"value": value, "sample": sample, "children": {}}; return
        node = tree
        while True:
            distance = (value ^ node["value"]).bit_count()
            if distance not in node["children"]:
                node["children"][distance] = {"value": value, "sample": sample, "children": {}}; return
            node = node["children"][distance]
    for sample in samples:
        with Image.open(root / sample.path) as image:
            gray = image.convert("L").resize((9, 8))
            pixels = list(gray.getdata())
            value = sum((pixels[y * 9 + x] > pixels[y * 9 + x + 1]) << (y * 8 + x) for y in range(8) for x in range(8))
        for distance, prior in query(tree, value):
            if prior.sha256 == sample.sha256:
                continue
            pair_count += 1; cross_class += prior.class_id != sample.class_id
            if prior.class_id != sample.class_id:
                cross_class_candidates.append({"left_path": prior.path, "left_class_id": prior.class_id,
                                               "left_sha256": prior.sha256, "right_path": sample.path,
                                               "right_class_id": sample.class_id, "right_sha256": sample.sha256,
                                               "hamming_distance": distance, "adjudication": "pending", "notes": ""})
            if len(examples) < 25:
                examples.append([prior.path, sample.path, str(distance)])
        insert(value, sample)
    return {"method": "64-bit horizontal dHash; BK-tree Hamming radius <=4; exact hashes excluded",
            "candidate_pairs": pair_count, "cross_class_candidate_pairs": cross_class, "examples": examples,
            "cross_class_candidates": cross_class_candidates}

def write_cross_class_review(root: Path, output: Path, candidates: list[dict]) -> None:
    from PIL import Image, ImageDraw
    review = output / "cross_class_near_duplicate_review"
    review.mkdir(parents=True, exist_ok=True)
    (review / "review_index.json").write_text(json.dumps({"schema_version": 1, "candidates": candidates}, indent=2), encoding="utf-8")
    rows = ["pair,left_class,left_path,left_sha256,right_class,right_path,right_sha256,hamming_distance,adjudication,notes"]
    for index, item in enumerate(candidates, 1):
        rows.append(f'{index},{item["left_class_id"]},"{item["left_path"]}",{item["left_sha256"]},{item["right_class_id"]},"{item["right_path"]}",{item["right_sha256"]},{item["hamming_distance"]},pending,""')
    (review / "review_index.csv").write_text("\n".join(rows) + "\n", encoding="utf-8")
    per_page = 7
    for page_start in range(0, len(candidates), per_page):
        page = Image.new("RGB", (1000, 150 * per_page + 50), "white")
        draw = ImageDraw.Draw(page)
        draw.text((10, 10), "Cross-class dHash candidates (adjudication: pending)", fill="black")
        for row, item in enumerate(candidates[page_start:page_start + per_page]):
            y = 45 + row * 150
            left = Image.open(root / item["left_path"]).convert("RGB").resize((128, 128))
            right = Image.open(root / item["right_path"]).convert("RGB").resize((128, 128))
            page.paste(left, (10, y)); page.paste(right, (150, y))
            pair = page_start + row + 1
            text = (f'Pair {pair}: class {item["left_class_id"]} vs {item["right_class_id"]}; Hamming={item["hamming_distance"]}\n'
                    f'{item["left_path"]}\n{item["right_path"]}\nAdjudication: [ ] duplicate [ ] distinct [ ] uncertain')
            draw.multiline_text((295, y + 10), text, fill="black", spacing=6)
        page.save(review / f"contact_sheet_{page_start // per_page + 1:02d}.png")

def generate_folds(samples: Iterable[Sample], folds: int = 5, seed: int = 20260718) -> list[dict]:
    samples = list(samples)
    by_class_hash: dict[str, dict[str, list[Sample]]] = defaultdict(lambda: defaultdict(list))
    for sample in samples:
        by_class_hash[sample.class_id][sample.sha256].append(sample)
    assignments = [[] for _ in range(folds)]
    for class_id in CLASS_NAMES:
        groups = list(by_class_hash[class_id].values())
        random.Random(seed + int(class_id)).shuffle(groups)
        groups.sort(key=len, reverse=True)
        loads = [0] * folds
        for group in groups:
            target = min(range(folds), key=lambda i: (loads[i], i))
            assignments[target].extend(group)
            loads[target] += len(group)
    result = []
    all_paths = {s.path for s in samples}
    for index, held_out in enumerate(assignments):
        evaluation = sorted(s.path for s in held_out)
        result.append({"fold": index, "development": sorted(all_paths - set(evaluation)), "evaluation": evaluation,
                       "evaluation_counts": dict(Counter(s.class_id for s in held_out))})
    return result

def validate_folds(samples: Iterable[Sample], folds_doc: list[dict]) -> dict:
    samples = list(samples)
    identities = {sample.path for sample in samples}
    if len(identities) != len(samples):
        raise ValueError("Frozen identities are not unique")
    record_by_id = {sample.path: sample for sample in samples}
    evaluation_coverage = Counter()
    fold_results = []
    for fold in folds_doc:
        development = list(fold["development"]); evaluation = list(fold["evaluation"])
        dev_set, eval_set = set(development), set(evaluation)
        if not development or not evaluation:
            raise ValueError(f"Fold {fold['fold']} has an empty partition")
        if len(dev_set) != len(development) or len(eval_set) != len(evaluation):
            raise ValueError(f"Fold {fold['fold']} repeats an identity")
        if dev_set & eval_set or dev_set | eval_set != identities:
            raise ValueError(f"Fold {fold['fold']} is overlapping or incomplete")
        dev_hashes = {record_by_id[item].sha256 for item in dev_set}
        eval_hashes = {record_by_id[item].sha256 for item in eval_set}
        if dev_hashes & eval_hashes:
            raise ValueError(f"Fold {fold['fold']} leaks an exact-content hash")
        evaluation_coverage.update(evaluation)
        actual_counts = dict(Counter(record_by_id[item].class_id for item in evaluation))
        if actual_counts != fold["evaluation_counts"]:
            raise ValueError(f"Fold {fold['fold']} class counts disagree")
        fold_results.append({"fold": fold["fold"], "development": len(development), "evaluation": len(evaluation),
                             "evaluation_counts": actual_counts, "disjoint": True, "complete": True,
                             "exact_hash_isolated": True})
    if set(evaluation_coverage) != identities or any(count != 1 for count in evaluation_coverage.values()):
        raise ValueError("Five-fold evaluation coverage is not exactly once per identity")
    return {"identities": len(identities), "folds": fold_results, "evaluation_coverage_exactly_once": True}

def correct_fold_artifacts(prep: Path, correction_version: str = "fold-manifest-v2") -> dict:
    derived_path = prep / "derived_manifest.json"
    active_fold = prep / "fold_manifest.json"
    protocol_path = prep / "experiment_protocol.json"
    conditions_path = prep / "condition_specs.json"
    derived = json.loads(derived_path.read_text(encoding="utf-8"))
    samples = [Sample(item["record_id"], item["class_id"], item["sha256"], item["width"], item["height"], item["channels"])
               for item in derived["records"]]
    old_hashes = {name: sha256_file(prep / name) for name in ("fold_manifest.json", "experiment_protocol.json", "condition_specs.json")}
    correction = prep / "corrections" / correction_version
    correction.mkdir(parents=True, exist_ok=False)
    for name in ("fold_manifest.json", "experiment_protocol.json", "condition_specs.json"):
        shutil.copy2(prep / name, correction / f"prior_{name}")
    folds = generate_folds(samples)
    invariants = validate_folds(samples, folds)
    fold_manifest = {"schema_version": 2, "artifact_version": correction_version,
                     "dataset_id": derived["dataset_id"], "derived_manifest": str(derived_path.resolve()),
                     "derived_manifest_sha256": sha256_file(derived_path), "dataset_hash": stable_json_hash(derived["records"]),
                     "fold_seed": 20260718, "folds": folds, "samples": [asdict(s) for s in samples],
                     "correction": {"defect": "generator iterator exhausted before development identity construction",
                                    "disposition": "corrected", "prior_fold_manifest_sha256": old_hashes["fold_manifest.json"]}}
    corrected_fold_path = correction / "fold_manifest.json"
    corrected_fold_path.write_text(json.dumps(fold_manifest, indent=2), encoding="utf-8")
    fold_hash = sha256_file(corrected_fold_path)
    protocol = json.loads(protocol_path.read_text(encoding="utf-8")); protocol.update(
        {"artifact_version": correction_version, "fold_manifest": str(active_fold.resolve()), "fold_manifest_sha256": fold_hash})
    conditions = json.loads(conditions_path.read_text(encoding="utf-8")); conditions.update(
        {"artifact_version": correction_version, "fold_manifest": str(active_fold.resolve()), "fold_manifest_sha256": fold_hash})
    (correction / "experiment_protocol.json").write_text(json.dumps(protocol, indent=2), encoding="utf-8")
    (correction / "condition_specs.json").write_text(json.dumps(conditions, indent=2), encoding="utf-8")
    provenance = {"schema_version": 1, "artifact_version": correction_version, "defect_disposition": "corrected",
                  "old_hashes": old_hashes, "new_hashes": {"fold_manifest.json": fold_hash,
                  "experiment_protocol.json": sha256_file(correction / "experiment_protocol.json"),
                  "condition_specs.json": sha256_file(correction / "condition_specs.json")},
                  "derived_manifest_sha256": sha256_file(derived_path), "invariants": invariants,
                  "training_authorized": False, "optimizer_steps": 0, "model_updates": 0}
    (correction / "correction_provenance.json").write_text(json.dumps(provenance, indent=2), encoding="utf-8")
    shutil.copy2(corrected_fold_path, active_fold); shutil.copy2(correction / "experiment_protocol.json", protocol_path)
    shutil.copy2(correction / "condition_specs.json", conditions_path)
    return provenance

def condition_specs() -> list[dict]:
    family = (["stability"] * 4 + ["balancing"] * 6 + ["efficiency"] * 6)
    return [{"id": name, "family": family[i], "training_authorized": False} for i, name in enumerate(CONDITION_CATALOG)]

def dry_run(dataset: Path, output: Path, condition: str, seed: int) -> dict:
    if condition not in CONDITION_CATALOG:
        raise ValueError(f"Unknown condition: {condition}")
    all_samples, unreadable = scan_dataset(dataset)
    manifest = json.loads((dataset / "metadata" / "dataset_manifest.json").read_text(encoding="utf-8-sig"))
    eligible = {item.get("path") or item.get("crop_path") for item in manifest.get("items", []) if bool(item.get("trainer_eligible", False))}
    samples = [sample for sample in all_samples if sample.path in eligible]
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    payload = {"status": "dry-run-complete", "training_authorized": False, "optimizer_steps": 0,
               "model_updates": 0, "dataset": str(dataset.resolve()), "samples": len(samples),
               "unreadable": len(unreadable), "excluded_ineligible": len(all_samples) - len(samples), "condition": condition, "seed": seed,
               "output": str(output), "config_hash": stable_json_hash({"condition": condition, "seed": seed})}
    (output / "dry_run.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return payload

def _manifest_candidates(root: Path, source_id: str, allowed_classes: set[str]) -> tuple[list[DerivedSample], list[dict]]:
    from PIL import Image
    manifest_path = root / "metadata" / "dataset_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    dataset_id = str(manifest.get("dataset_id") or source_id)
    accepted, excluded = [], []
    for index, item in enumerate(manifest.get("items", [])):
        relative = item.get("path") or item.get("crop_path") or ""
        class_id = str(item.get("class_id", item.get("reviewed_label", item.get("label", ""))))
        eligible = bool(item.get("trainer_eligible", False))
        if not eligible or class_id not in allowed_classes or not relative:
            excluded.append({"source_dataset_id": dataset_id, "source_item_id": item.get("image_id") or str(index),
                             "source_path": relative, "class_id": class_id, "trainer_eligible": eligible,
                             "reason": "ineligible" if not eligible else "class_not_selected" if class_id not in allowed_classes else "missing_path"})
            continue
        path = root / relative
        digest = sha256_file(path)
        with Image.open(path) as image:
            image.load(); width, height, source_channels = image.width, image.height, image.mode
            image.convert("RGB").load()
        if (width, height) != (64, 64):
            raise ValueError(f"Expected readable 64x64 input convertible to RGB: {path} ({width}x{height}:{source_channels})")
        item_id = str(item.get("image_id") or item.get("image_index") or index)
        accepted.append(DerivedSample(
            record_id=f"{source_id}:{item_id}:{digest[:12]}", source_dataset_id=dataset_id,
            source_root=str(root.resolve()), source_item_id=item_id, source_path=relative,
            class_id=class_id, sha256=digest, width=width, height=height, channels="RGB",
            trainer_eligible=True, provenance={"composition_role": "base" if source_id == "starter" else "addition",
                                                "source_manifest": str(manifest_path.resolve()),
                                                "source_split": item.get("split"), "source_role": item.get("role"),
                                                "source_image_mode": source_channels, "normalized_image_mode": "RGB",
                                                "source_original_label": item.get("original_label"),
                                                "source_provenance": item.get("provenance")},
        ))
    return accepted, excluded

def _perceptual_value(path: Path) -> int:
    from PIL import Image
    with Image.open(path) as image:
        pixels = list(image.convert("L").resize((9, 8)).getdata())
    return sum((pixels[y * 9 + x] > pixels[y * 9 + x + 1]) << (y * 8 + x) for y in range(8) for x in range(8))

def compose_datasets(base: Path, additions: Path, output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    base_manifest = base / "metadata" / "dataset_manifest.json"
    additions_manifest = additions / "metadata" / "dataset_manifest.json"
    source_hashes_before = {"starter_manifest": sha256_file(base_manifest), "wellplate_manifest": sha256_file(additions_manifest)}
    starter, starter_excluded = _manifest_candidates(base, "starter", set(CLASS_NAMES))
    wellplate, wellplate_excluded = _manifest_candidates(additions, "wellplate", {"1", "2"})
    nominal = Counter(s.class_id for s in starter + wellplate)
    by_hash: dict[str, list[DerivedSample]] = defaultdict(list)
    for sample in starter + wellplate:
        by_hash[sample.sha256].append(sample)
    selected, duplicate_decisions, conflicts = [], [], []
    for digest, group in sorted(by_hash.items()):
        labels = sorted({s.class_id for s in group})
        if len(labels) > 1:
            conflicts.append({"sha256": digest, "labels": labels, "records": [asdict(s) for s in group], "decision": "unresolved"})
            continue
        authoritative = sorted(group, key=lambda s: (s.provenance["composition_role"] != "base", s.record_id))[0]
        selected.append(authoritative)
        if len(group) > 1:
            duplicate_decisions.append({"sha256": digest, "class_id": labels[0], "kept_record_id": authoritative.record_id,
                                        "discarded_record_ids": [s.record_id for s in group if s != authoritative],
                                        "rule": "starter authoritative; otherwise deterministic first record"})
    # Cross-source perceptual candidates only; exact hashes were already resolved.
    base_values = [(s, _perceptual_value(Path(s.source_root) / s.source_path)) for s in selected if s.provenance["composition_role"] == "base"]
    addition_values = [(s, _perceptual_value(Path(s.source_root) / s.source_path)) for s in selected if s.provenance["composition_role"] == "addition"]
    near = []
    for added, avalue in addition_values:
        for prior, pvalue in base_values:
            distance = (avalue ^ pvalue).bit_count()
            if distance <= 4:
                near.append({"base_record_id": prior.record_id, "base_class_id": prior.class_id,
                             "addition_record_id": added.record_id, "addition_class_id": added.class_id,
                             "hamming_distance": distance, "decision": "pending"})
    folds = generate_folds(s.fold_sample() for s in selected)
    readiness = not conflicts
    counts = Counter(s.class_id for s in selected)
    class_weights = {class_id: (max(counts.values()) / count if count else None) for class_id, count in sorted(counts.items())}
    manifest = {"schema_version": 1, "dataset_id": "organoid_wellplate_combined", "class_semantics": CLASS_NAMES,
                "immutable_source_references": True, "ready": readiness, "records": [asdict(s) for s in sorted(selected, key=lambda s: s.record_id)],
                "unresolved_exact_label_conflicts": conflicts}
    audit = {"schema_version": 1, "ready": readiness, "source_hashes_before": source_hashes_before,
             "source_hashes_after": {"starter_manifest": sha256_file(base_manifest), "wellplate_manifest": sha256_file(additions_manifest)},
             "nominal_counts": dict(nominal), "deduplicated_counts": dict(counts),
             "starter_selected_counts": dict(Counter(s.class_id for s in selected if s.provenance["composition_role"] == "base")),
             "wellplate_addition_counts": dict(Counter(s.class_id for s in selected if s.provenance["composition_role"] == "addition")),
             "exact_duplicate_groups": len(duplicate_decisions), "exact_duplicate_decisions": duplicate_decisions,
             "exact_label_conflicts": conflicts, "cross_source_near_duplicate_candidates": near,
             "near_duplicate_state": "pending_review" if near else "none", "excluded": starter_excluded + wellplate_excluded,
             "excluded_counts_by_reason": dict(Counter(item["reason"] for item in starter_excluded + wellplate_excluded)),
             "normalized_source_mode_counts": dict(Counter(s.provenance["source_image_mode"] for s in selected)),
             "fold_evaluation_counts": {str(fold["fold"]): fold["evaluation_counts"] for fold in folds},
             "inverse_frequency_max_normalized_weights": class_weights,
             "imbalance_note": "Class weights are descriptive only; later experiments compare controlled balancing policies."}
    fold_manifest = {"schema_version": 1, "dataset_id": manifest["dataset_id"], "derived_manifest": str((output / "derived_manifest.json").resolve()),
                     "dataset_hash": stable_json_hash(manifest["records"]), "fold_seed": 20260718,
                     "folds": folds, "samples": [asdict(s.fold_sample()) for s in selected]}
    protocol = {"primary_metric": "balanced_accuracy", "screening_min_recall": .70, "finalist_target_recall": .80,
                "tie_breaker": "macro_f1", "screening_seeds": SCREENING_SEEDS, "finalist_seeds": FINALIST_SEEDS,
                "maximum_runs": MAX_RUNS, "training_authorized": False, "conditions": condition_specs(),
                "derived_manifest": str((output / "derived_manifest.json").resolve())}
    for name, value in (("derived_manifest.json", manifest), ("composition_audit.json", audit),
                        ("fold_manifest.json", fold_manifest), ("experiment_protocol.json", protocol),
                        ("condition_specs.json", {"training_authorized": False, "conditions": condition_specs()})):
        (output / name).write_text(json.dumps(value, indent=2), encoding="utf-8")
    return {"ready": readiness, "counts": dict(counts), "conflicts": len(conflicts), "near_duplicate_candidates": len(near),
            "manifest_hash": stable_json_hash(manifest), "output": str(output.resolve())}

def dry_run_manifest(manifest_path: Path, output: Path, condition: str, seed: int) -> dict:
    if condition not in CONDITION_CATALOG:
        raise ValueError(f"Unknown condition: {condition}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not manifest.get("ready", False):
        raise ValueError("Derived manifest is not ready")
    records = manifest.get("records", [])
    for record in records:
        path = Path(record["source_root"]) / record["source_path"]
        if sha256_file(path) != record["sha256"]:
            raise ValueError(f"Source content changed: {path}")
    output = output.resolve(); output.mkdir(parents=True, exist_ok=False)
    payload = {"status": "dry-run-complete", "training_authorized": False, "optimizer_steps": 0, "model_updates": 0,
               "production_registry_access": False, "derived_manifest": str(manifest_path.resolve()), "samples": len(records),
               "condition": condition, "seed": seed, "output": str(output),
               "config_hash": stable_json_hash({"manifest": stable_json_hash(records), "condition": condition, "seed": seed})}
    (output / "dry_run.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return payload

def prepare(dataset: Path, output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    all_samples, unreadable = scan_dataset(dataset)
    manifest_path = dataset / "metadata" / "dataset_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig")) if manifest_path.exists() else {}
    items = manifest.get("items", [])
    item_by_path = {(item.get("path") or item.get("crop_path")): item for item in items}
    excluded_items = [{"image_id": item.get("image_id"), "path": item.get("path") or item.get("crop_path"),
                       "class_id": str(item.get("class_id")), "review_state": item.get("review_state"),
                       "reviewed_label": item.get("reviewed_label"), "exclude_reason": item.get("exclude_reason")}
                      for item in items if not bool(item.get("trainer_eligible", False))]
    samples = [sample for sample in all_samples if bool(item_by_path.get(sample.path, {}).get("trainer_eligible", False))]
    folds = generate_folds(samples)
    hashes = Counter(s.sha256 for s in samples)
    near_duplicates = perceptual_duplicate_audit(dataset, samples)
    write_cross_class_review(dataset, output, near_duplicates["cross_class_candidates"])
    inconsistencies = {"manifest_display_names": {str(c.get("id")): c.get("display_name") for c in manifest.get("classes", [])},
                       "non_eligible_items": sum(not bool(item.get("trainer_eligible", False)) for item in items),
                       "status_included_but_not_eligible": sum(item.get("status") == "included" and not bool(item.get("trainer_eligible", False)) for item in items)}
    audit = {"schema_version": 1, "class_semantics": CLASS_NAMES,
             "source_manifest_labels_preserved": True,
             "frozen_sample_policy": "Only dataset_manifest items with trainer_eligible=true are included; all others are excluded before audit folds and dry-run experiment loading.",
             "source_folder_counts": dict(Counter(s.class_id for s in all_samples)),
             "counts": dict(Counter(s.class_id for s in samples)), "excluded_manifest_items": excluded_items,
             "total": len(samples), "unreadable": unreadable,
             "dimensions": {f"{s.width}x{s.height}:{s.channels}": 0 for s in samples},
             "exact_duplicate_groups": sum(1 for count in hashes.values() if count > 1),
             "exact_duplicate_files": sum(count for count in hashes.values() if count > 1),
             "near_duplicates": near_duplicates, "manifest_review_inconsistencies": inconsistencies,
             "rare_class_caveat": "Class 2 has very few samples; uncertainty must be reported from out-of-fold predictions and confidence intervals."}
    audit["dimensions"] = dict(Counter(f"{s.width}x{s.height}:{s.channels}" for s in samples))
    fold_manifest = {"schema_version": 1, "dataset_root": str(dataset.resolve()), "dataset_hash": stable_json_hash([asdict(s) for s in samples]),
                "fold_seed": 20260718, "folds": folds, "samples": [asdict(s) for s in samples]}
    protocol = {"primary_metric": "balanced_accuracy", "screening_min_recall": .70, "finalist_target_recall": .80,
                "tie_breaker": "macro_f1", "screening_seeds": SCREENING_SEEDS, "finalist_seeds": FINALIST_SEEDS,
                "maximum_runs": MAX_RUNS, "conditions": condition_specs(), "test_leakage_rule": "Select conditions from cross-validation only; evaluate locked winner once."}
    environment = {"python": sys.version, "platform": platform.platform(), "executable": sys.executable}
    try:
        import torch
        environment.update({"torch": torch.__version__, "cuda_available": torch.cuda.is_available(),
                            "cuda_version": torch.version.cuda, "gpu": torch.cuda.get_device_name(0) if torch.cuda.is_available() else None})
    except Exception as exc:
        environment["torch_error"] = str(exc)
    for name, value in (("dataset_audit.json", audit), ("fold_manifest.json", fold_manifest),
                        ("experiment_protocol.json", protocol), ("environment.json", environment),
                        ("result_schema.json", result_schema())):
        (output / name).write_text(json.dumps(value, indent=2), encoding="utf-8")
    return {"audit": audit, "manifest_hash": stable_json_hash(fold_manifest), "environment": environment, "output": str(output.resolve())}

def result_schema() -> dict:
    return {"required": ["run_id", "condition", "seed", "fold", "status", "epochs", "per_class", "confusion_matrix", "timing", "gpu"],
            "epoch_fields": ["stage", "stage_epoch", "global_epoch", "loss", "balanced_accuracy", "macro_f1", "learning_rate", "prediction_distribution", "logit_range", "logit_variance", "gradient_norm", "parameter_delta_norm"],
            "aggregate_fields": ["mean", "standard_deviation", "confidence_interval_95", "failures", "runtime", "peak_gpu_memory"]}

def selection_key(result: dict) -> tuple[float, float]:
    recalls = result["per_class_recall"]
    if min(recalls.values()) < result.get("recall_gate", .70):
        return (-1.0, -1.0)
    return (float(result["balanced_accuracy"]), float(result["macro_f1"]))

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    prep = sub.add_parser("prepare"); prep.add_argument("--dataset", type=Path, required=True); prep.add_argument("--output", type=Path, required=True)
    dry = sub.add_parser("dry-run"); dry.add_argument("--dataset", type=Path, required=True); dry.add_argument("--output", type=Path, required=True); dry.add_argument("--condition", required=True); dry.add_argument("--seed", type=int, required=True)
    compose = sub.add_parser("compose"); compose.add_argument("--base", type=Path, required=True); compose.add_argument("--additions", type=Path, required=True); compose.add_argument("--output", type=Path, required=True)
    derived_dry = sub.add_parser("dry-run-manifest"); derived_dry.add_argument("--manifest", type=Path, required=True); derived_dry.add_argument("--output", type=Path, required=True); derived_dry.add_argument("--condition", required=True); derived_dry.add_argument("--seed", type=int, required=True)
    correction = sub.add_parser("correct-folds"); correction.add_argument("--prep", type=Path, required=True); correction.add_argument("--version", default="fold-manifest-v2")
    args = parser.parse_args(argv)
    if args.command == "prepare": payload = prepare(args.dataset, args.output)
    elif args.command == "dry-run": payload = dry_run(args.dataset, args.output, args.condition, args.seed)
    elif args.command == "compose": payload = compose_datasets(args.base, args.additions, args.output)
    elif args.command == "dry-run-manifest": payload = dry_run_manifest(args.manifest, args.output, args.condition, args.seed)
    else: payload = correct_fold_artifacts(args.prep, args.version)
    print(json.dumps(payload, indent=2)); return 0

if __name__ == "__main__":
    raise SystemExit(main())
