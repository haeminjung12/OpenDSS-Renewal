from pathlib import Path
from tempfile import TemporaryDirectory
import json

from PIL import Image

from .prep import (CONDITION_CATALOG, FINALIST_SEEDS, SCREENING_SEEDS, Sample,
                   compose_datasets, dry_run, dry_run_manifest, generate_folds,
                   selection_key, stable_json_hash, validate_folds)

def test_folds_are_deterministic_and_keep_duplicate_hashes_together():
    samples = [Sample(f"images/{c}/{i}.png", c, f"{c}-{i // 2}", 8, 8, "RGB") for c in "012" for i in range(10)]
    first = generate_folds(samples); second = generate_folds(samples)
    assert first == second
    location = {}
    for fold in first:
        for path in fold["evaluation"]:
            sample = next(s for s in samples if s.path == path)
            assert location.setdefault(sample.sha256, fold["fold"]) == fold["fold"]

def test_generator_and_list_folds_are_identical_complete_and_exactly_once():
    samples = [Sample(f"id-{c}-{i}", c, f"hash-{c}-{i}", 64, 64, "RGB") for c in "012" for i in range(10)]
    listed = generate_folds(samples)
    generated = generate_folds(sample for sample in samples)
    assert generated == listed
    invariants = validate_folds(samples, generated)
    assert invariants["identities"] == 30
    assert invariants["evaluation_coverage_exactly_once"] is True
    assert all(row["development"] == 24 and row["evaluation"] == 6 for row in invariants["folds"])

def test_hash_and_catalog_are_stable():
    assert stable_json_hash({"b": 2, "a": 1}) == stable_json_hash({"a": 1, "b": 2})
    assert len(CONDITION_CATALOG) == len(set(CONDITION_CATALOG)) == 16
    assert set(SCREENING_SEEDS).issubset(FINALIST_SEEDS)
    assert selection_key({"balanced_accuracy": .9, "macro_f1": .8, "per_class_recall": {"0": .9, "1": .8, "2": .69}}) == (-1, -1)
    assert selection_key({"balanced_accuracy": .9, "macro_f1": .8, "per_class_recall": {"0": .9, "1": .8, "2": .7}}) == (.9, .8)

def test_dry_run_is_isolated_and_never_trains():
    with TemporaryDirectory() as temp:
        root = Path(temp) / "dataset"
        for class_id in "012":
            folder = root / "images" / class_id; folder.mkdir(parents=True)
            Image.new("RGB", (8, 8), (int(class_id) * 30, 0, 0)).save(folder / "x.png")
        metadata = root / "metadata"; metadata.mkdir()
        (metadata / "dataset_manifest.json").write_text(json.dumps({"items": [
            {"path": f"images/{class_id}/x.png", "trainer_eligible": True} for class_id in "012"]}), encoding="utf-8")
        result = dry_run(root, Path(temp) / "run", CONDITION_CATALOG[0], 1729)
        assert result["optimizer_steps"] == result["model_updates"] == 0
        assert result["training_authorized"] is False

def _dataset(root: Path, dataset_id: str, rows: list[tuple[str, str, bool, tuple[int, int, int]]]) -> None:
    items = []
    for index, (name, class_id, eligible, color) in enumerate(rows):
        relative = f"images/{class_id}/{name}.png"
        path = root / relative; path.parent.mkdir(parents=True, exist_ok=True)
        Image.new("RGB", (64, 64), color).save(path)
        items.append({"image_id": f"{dataset_id}-{index}", "path": relative, "class_id": class_id,
                      "reviewed_label": class_id, "trainer_eligible": eligible, "split": "train"})
    metadata = root / "metadata"; metadata.mkdir()
    (metadata / "dataset_manifest.json").write_text(json.dumps({"dataset_id": dataset_id, "items": items}), encoding="utf-8")

def test_compose_precedence_filtering_provenance_folds_and_isolation():
    with TemporaryDirectory() as temp:
        root = Path(temp); base = root / "base"; additions = root / "additions"
        _dataset(base, "starter", [("base0", "0", True, (1, 1, 1)), ("base1", "1", True, (2, 2, 2)),
                                    ("base2", "2", True, (3, 3, 3)), ("excluded", "1", False, (4, 4, 4))])
        _dataset(additions, "wellplate", [("duplicate", "1", True, (2, 2, 2)), ("new1", "1", True, (5, 5, 5)),
                                          ("new2", "2", True, (6, 6, 6)), ("not-added", "0", True, (7, 7, 7))])
        out = root / "prep"; result = compose_datasets(base, additions, out)
        assert result["counts"] == {"0": 1, "1": 2, "2": 2}
        manifest = json.loads((out / "derived_manifest.json").read_text())
        assert all(record["trainer_eligible"] for record in manifest["records"])
        assert all(record["source_root"] in {str(base.resolve()), str(additions.resolve())} for record in manifest["records"])
        duplicate = next(record for record in manifest["records"] if record["sha256"] == next(r["sha256"] for r in manifest["records"] if r["source_path"].endswith("base1.png")))
        assert duplicate["provenance"]["composition_role"] == "base"
        assert not any(r["provenance"]["composition_role"] == "addition" and r["class_id"] == "0" for r in manifest["records"])
        first = json.loads((out / "fold_manifest.json").read_text())["folds"]
        second_out = root / "prep2"; compose_datasets(base, additions, second_out)
        assert first == json.loads((second_out / "fold_manifest.json").read_text())["folds"]
        dry = dry_run_manifest(out / "derived_manifest.json", root / "dry", CONDITION_CATALOG[0], 1729)
        assert dry["training_authorized"] is False and dry["optimizer_steps"] == dry["model_updates"] == 0
        assert dry["production_registry_access"] is False

def test_compose_blocks_exact_content_label_conflict():
    with TemporaryDirectory() as temp:
        root = Path(temp); base = root / "base"; additions = root / "additions"
        _dataset(base, "starter", [("base", "1", True, (9, 9, 9)), ("zero", "0", True, (1, 2, 3)), ("two", "2", True, (3, 2, 1))])
        _dataset(additions, "wellplate", [("conflict", "2", True, (9, 9, 9))])
        out = root / "prep"; result = compose_datasets(base, additions, out)
        assert result["ready"] is False and result["conflicts"] == 1
        try:
            dry_run_manifest(out / "derived_manifest.json", root / "dry", CONDITION_CATALOG[0], 1729)
            assert False, "unresolved label conflict must block dry run"
        except ValueError as exc:
            assert "not ready" in str(exc)
