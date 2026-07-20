from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path

from droplet_trainer.publication_experiments.architecture_comparison import weight_provenance
from droplet_trainer.train import _build_model, _export_onnx, _seed_everything

ARCHITECTURES = {
    "mobilenet_v3_small": ("MobileNetV3-Small", "Faster", 1729),
    "efficientnet_b0": ("EfficientNet-B0", "More Accurate", 2718),
}
CLASSES = ["0", "1", "2"]
LABELS = {"0": "Empty", "1": "Single", "2": "MoreThanOne"}
NORMALIZATION = {"mean": [0.485, 0.456, 0.406], "std": [0.229, 0.224, 0.225]}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def artifact(model: Path) -> dict:
    sidecars = []
    for path in sorted(model.parent.glob("*.onnx.data")):
        sidecars.append({"filename": path.name, "sha256": sha256(path), "byte_size": path.stat().st_size, "required": True})
    return {
        "onnx_file": model.name,
        "onnx_sha256": sha256(model),
        "byte_size": model.stat().st_size,
        "external_data_files": sidecars,
        "format": "onnx",
        "opset": 18,
        "input_tensor": {"name": "input", "shape": ["batch", 3, 96, 96], "dtype": "float32", "layout": "NCHW"},
        "output_tensor": {"name": "logits", "shape": ["batch", 3], "dtype": "float32", "score_type": "signed_logits"},
    }


def metadata(arch: str, origin: str, model: Path, provenance: dict, source_manifest: str | None = None) -> dict:
    family, qualifier, seed = ARCHITECTURES[arch]
    blank = origin == "blank"
    return {
        "schema_version": "model-metadata-v2",
        "model_id": f"opendss-{origin}-{arch}-3class",
        "model_name": f"{family} — {qualifier}",
        "status": "imagenet_transfer_start" if blank else "trained",
        "architecture": {"id": arch, "family": family, "num_classes": 3},
        "origin": origin,
        "user_facing_label": f"{family} — {qualifier}",
        "recommended": arch == "mobilenet_v3_small",
        "classes": CLASSES,
        "class_to_idx": {key: int(key) for key in CLASSES},
        "display_labels": LABELS,
        "label_schema_version": "opendss-droplet-3class-v1",
        "sorting_policy": {"target_class_id": "1", "target_display_label": "Single", "waste_class_ids": ["0", "2"], "trigger_rule": "trigger_on_target_class"},
        "input_size": [96, 96, 3],
        "normalization": NORMALIZATION,
        "artifact": artifact(model),
        "initialization": {
            "source": "official torchvision ImageNet weights",
            "weight_id": provenance["weight_id"],
            "source_checkpoint_sha256": provenance["sha256"],
            "classifier_head_seed": seed,
            "classifier_head_policy": "deterministic_new_signed_three_logit_head",
            "not_droplet_trained": blank,
        },
        "training_config": {"architecture": arch, "pretrained": True, "classifier_output": "signed_logits", "seed": seed},
        "validation_summary": {"image_validation": {"status": "not_run" if blank else "cross_validation_evidence_external"}},
        "provenance": {"source_manifest": source_manifest, "performance_report": "docs/worker-reports/publication-gpu-training-experiments-2026-07-18/phase-3c-architecture-comparison.md"},
        "limitations": (["ImageNet-start template; not droplet-trained or live-sorting ready."] if blank else ["All-data fit; performance claims come from the separate accepted cross-validation report."]),
    }


def write_metadata(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime-models", type=Path, required=True)
    parser.add_argument("--production-candidates", type=Path, required=True)
    args = parser.parse_args()
    runtime = args.runtime_models.resolve()
    candidates = args.production_candidates.resolve()
    runtime.mkdir(parents=True, exist_ok=True)

    registry_entries = []
    for arch, (_, _, seed) in ARCHITECTURES.items():
        provenance = weight_provenance(arch)
        blank_dir = runtime / "templates" / "blank" / arch
        blank_dir.mkdir(parents=True, exist_ok=True)
        _seed_everything(seed)
        config = {"architecture": arch, "pretrained": True, "pretrained_weight_id": provenance["weight_id"], "pretrained_weight_path": provenance["cache_path"], "pretrained_weight_sha256": provenance["sha256"], "classifier_output": "signed_logits", "seed": seed, "input_size": [96, 96, 3]}
        model = _build_model(config, 3).cpu().eval()
        blank_model, _ = _export_onnx(model, blank_dir, {**config, "onnx_opset": 18})
        write_metadata(blank_dir / "metadata.json", metadata(arch, "blank", blank_model, provenance))

        source = candidates / arch
        trained_dir = runtime / "templates" / "pretrained" / arch
        trained_dir.mkdir(parents=True, exist_ok=True)
        for name in ("model.onnx", "model.onnx.data"):
            shutil.copy2(source / name, trained_dir / name)
        source_manifest = source / "bundle_candidate_manifest.json"
        shutil.copy2(source_manifest, trained_dir / "bundle_candidate_manifest.json")
        write_metadata(trained_dir / "metadata.json", metadata(arch, "pretrained", trained_dir / "model.onnx", provenance, str(source_manifest)))

        for origin, folder in (("blank", blank_dir), ("pretrained", trained_dir)):
            family, qualifier, _ = ARCHITECTURES[arch]
            is_blank = origin == "blank"
            registry_entries.append({
                "registry_entry_id": f"opendss_{origin}_{arch}_3class", "model_id": f"opendss-{origin}-{arch}-3class",
                "display_name": f"{'Blank' if is_blank else 'Pre-trained'} {family} — {qualifier}", "user_facing_label": f"{family} — {qualifier}",
                "architecture_id": arch, "origin": origin, "recommended": arch == "mobilenet_v3_small", "legacy": False,
                "state": "available", "model_status": "Untrained" if is_blank else "Trained",
                "live_use_mode": "blocked" if is_blank else "normal", "selectable_for_normal_live_sorting": False,
                "model_path": f"app/runtime/models/templates/{origin}/{arch}/model.onnx", "model_sha256": sha256(folder / "model.onnx"),
                "metadata_path": f"app/runtime/models/templates/{origin}/{arch}/metadata.json", "metadata_sha256": sha256(folder / "metadata.json"),
                "metadata_schema_version": "model-metadata-v2", "metadata_status": "ImageNet-start template" if is_blank else "Verified production candidate",
                "validation_status": "Not droplet-trained" if is_blank else "Two-fold cross-validation evidence accepted",
                "promotion_status": "Starter only" if is_blank else "Available", "classes": CLASSES, "display_labels": LABELS,
                "label_schema_version": "opendss-droplet-3class-v1",
                "target_policy": {"target_class_id": "1", "target_display_label": "Single", "waste_class_id": "0", "waste_display_label": "Empty", "trigger_rule": "trigger_on_target_class"},
                "model_sidecars": [{"path": f"app/runtime/models/templates/{origin}/{arch}/model.onnx.data", "required": True, "sha256": sha256(folder / "model.onnx.data")}],
                "blockers": ([{"blocker": "Training required", "required_next_action": "Train and validate this template."}] if is_blank else []),
            })

    package_options = registry_entries
    seeded_entries = [entry for entry in registry_entries if entry["architecture_id"] == "mobilenet_v3_small"]
    registry = {"schema_version": "model-registry-v2", "registry_id": "installed-modern-model-templates",
                "description": "MobileNetV3-Small and EfficientNet-B0 Blank/Pre-trained options. Legacy SqueezeNet entries remain loadable when their external assets are valid.",
                "entries": seeded_entries,
                "package_options": package_options}
    (runtime / "model_registry.json").write_text(json.dumps(registry, indent=2) + "\n", encoding="utf-8")

    print(json.dumps({"status": "complete", "architectures": list(ARCHITECTURES), "runtime_models": str(runtime)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
