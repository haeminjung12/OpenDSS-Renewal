from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from droplet_trainer.errors import CliError
from droplet_trainer.schema import ClassSchema, default_binary_schema
from droplet_trainer.train import (
    StageEarlyStopping,
    _classification_metrics,
    _build_model,
    _freeze_for_stage,
    _trainable_parameter_summary,
    _effective_number_weights,
    _logit_diagnostics,
    _config_binding_hash,
    _make_optimizer,
    _make_scheduler,
    _parameter_delta_l2,
    _run_epoch,
    _step_scheduler,
    _merge_partial_classifier_tensor,
    _checkpoint_output_count,
    _load_source_checkpoint,
    _load_source_artifact,
    _seed_everything,
    load_training_config,
)


class TrainerCorrectionTests(unittest.TestCase):
    def _load(self, payload: dict) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "config.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            return load_training_config(str(path), default_binary_schema())

    def test_accepted_modern_staged_defaults(self) -> None:
        config = load_training_config(None, default_binary_schema())
        self.assertEqual([s["learning_rate"] for s in config["stages"]], [1e-4, 1e-5])
        self.assertEqual([s["epochs"] for s in config["stages"]], [20, 15])
        self.assertEqual(config["batch_size"], 64)
        self.assertEqual(config["classifier_output"], "signed_logits")
        self.assertEqual(config["initialization"], {"mode": "imagenet"})
        self.assertEqual(config["imbalance"]["sampler_alpha"], 0.65)
        self.assertEqual(config["split_mode"], "auto")

    def test_signed_logits_is_explicit_and_serializable(self) -> None:
        config = self._load({"classifier_output": "signed_logits"})
        self.assertEqual(config["classifier_output"], "signed_logits")
        json.dumps(config)

    def test_gui_class_objects_are_normalized_to_ordered_ids(self) -> None:
        payload = {
            "classes": [
                {"id": "0", "index": 0, "display_name": "Empty", "folder": "images/0"},
                {"id": "1", "index": 1, "display_name": "Single", "folder": "images/1"},
                {"id": "2", "index": 2, "display_name": "More than one", "folder": "images/2"},
            ]
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "config.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            config = load_training_config(
                str(path),
                ClassSchema(classes=["0", "1", "2"], display_labels={"0": "Empty", "1": "Single", "2": "More than one"}),
            )
        self.assertEqual(config["classes"], ["0", "1", "2"])

    def test_simple_string_class_ids_remain_supported(self) -> None:
        self.assertEqual(self._load({"classes": ["0", "1"]})["classes"], ["0", "1"])

    def test_invalid_stage_is_rejected(self) -> None:
        with self.assertRaises(CliError):
            self._load({"stages": [{"name": "bad", "epochs": 0, "learning_rate": 1e-4}]})

    def test_frozen_mode_requires_explicit_authorization(self) -> None:
        with self.assertRaises(CliError):
            self._load({"split_mode": "frozen_external"})

    def test_config_binding_detects_mutation(self) -> None:
        config = load_training_config(None, default_binary_schema())
        first = _config_binding_hash(config)
        config["batch_size"] = 999
        self.assertNotEqual(first, _config_binding_hash(config))

    def test_partial_classifier_import_preserves_source_rows_and_initializes_third(self) -> None:
        import torch
        source = torch.tensor([[1.0, 3.0], [5.0, 7.0]])
        expected = torch.zeros(3, 2)
        mean = _merge_partial_classifier_tensor(expected, source, "partial_mean_existing")
        copied = _merge_partial_classifier_tensor(expected, source, "partial_copy_class1")
        self.assertTrue(torch.equal(mean[:2], source))
        self.assertTrue(torch.equal(mean[2], torch.tensor([3.0, 5.0])))
        self.assertTrue(torch.equal(copied[2], source[1]))

    def test_alternative_architectures_have_deterministic_three_class_heads(self) -> None:
        import importlib.util
        if importlib.util.find_spec("torchvision") is None:
            self.skipTest("torchvision is covered in the managed training environment")
        import torch
        for architecture in ("mobilenet_v3_small", "efficientnet_b0"):
            config = load_training_config(None, default_binary_schema())
            config.update({"architecture": architecture, "initialization": {"mode": "checkpoint", "checkpoint_path": "unused"}, "seed": 1729})
            first = _build_model(config, 3)
            second = _build_model(config, 3)
            self.assertEqual(first(torch.zeros(1, 3, 96, 96)).shape, (1, 3))
            self.assertTrue(torch.equal(first.classifier[-1].weight, second.classifier[-1].weight))
            _freeze_for_stage(first, "classifier_and_last_blocks")
            early = _trainable_parameter_summary(first)
            _freeze_for_stage(first, "controlled_fine_tune")
            late = _trainable_parameter_summary(first)
            self.assertLess(early["trainable_parameter_count"], late["trainable_parameter_count"])

    def test_modern_architectures_follow_dataset_class_count(self) -> None:
        import importlib.util
        if importlib.util.find_spec("torchvision") is None:
            self.skipTest("torchvision is covered in the managed training environment")
        import torch
        for architecture in ("mobilenet_v3_small", "efficientnet_b0"):
            config = load_training_config(None, default_binary_schema())
            config.update({"architecture": architecture, "initialization": {"mode": "checkpoint", "checkpoint_path": "unused"}})
            for class_count in (2, 3):
                model = _build_model(config, class_count)
                self.assertEqual(model(torch.zeros(1, 3, 96, 96)).shape, (1, class_count))

    def test_checkpoint_continuation_is_gated_only_by_output_count(self) -> None:
        import importlib.util
        if importlib.util.find_spec("torchvision") is None:
            self.skipTest("torchvision is covered in the managed training environment")
        import torch
        config = load_training_config(None, default_binary_schema())
        config.update({"architecture": "mobilenet_v3_small", "initialization": {"mode": "checkpoint", "checkpoint_path": "unused"}})
        source = _build_model(config, 2)
        target = _build_model(config, 2)
        with tempfile.TemporaryDirectory() as tmp:
            checkpoint = Path(tmp) / "checkpoint.pth"
            torch.save({"model_state": source.state_dict(), "class_ids": ["old-a", "old-b"]}, checkpoint)
            warnings = _load_source_checkpoint(target, str(checkpoint))
            self.assertIn("continued_from_checkpoint", warnings[0])
            self.assertTrue(torch.equal(source.classifier[-1].weight, target.classifier[-1].weight))

            mismatch = _build_model(config, 3)
            with self.assertRaises(CliError) as error:
                _load_source_checkpoint(mismatch, str(checkpoint))
            self.assertEqual(error.exception.code, "SOURCE_CHECKPOINT_CLASS_COUNT_MISMATCH")

    def test_onnx_is_never_used_as_a_training_source(self) -> None:
        with self.assertRaises(CliError) as error:
            _load_source_artifact(object(), "model.onnx", None)
        self.assertEqual(error.exception.code, "ONNX_NOT_TRAINABLE")

    def test_checkpoint_output_count_supports_both_modern_heads(self) -> None:
        import torch
        self.assertEqual(_checkpoint_output_count({"classifier.3.weight": torch.zeros(2, 4)}), 2)
        self.assertEqual(_checkpoint_output_count({"classifier.1.weight": torch.zeros(3, 4)}), 3)

    def test_exact_gui_checkpoint_shape_normalizes_to_one_initialization_contract(self) -> None:
        checkpoint = str(Path(tempfile.gettempdir()) / "packaged" / "checkpoint.pth")
        config = self._load({
            "architecture": "mobilenet_v3_small",
            "pretrained": False,
            "source_checkpoint_path": checkpoint,
            "classifier_initialization": "deterministic_new_head",
            "classes": [
                {"id": "0", "index": 0, "display_name": "Empty"},
                {"id": "1", "index": 1, "display_name": "Single"},
            ],
        })
        self.assertEqual(config["initialization"], {"mode": "checkpoint", "checkpoint_path": checkpoint})
        for ambiguous in ("pretrained", "source_checkpoint", "source_checkpoint_path", "classifier_initialization"):
            self.assertNotIn(ambiguous, config)

    def test_legacy_gui_scheduler_and_augmentation_are_truthfully_normalized(self) -> None:
        config = self._load({"scheduler": "StepLR", "augmentation": {"random_flip": True, "random_crop": False}})
        self.assertEqual(config["scheduler"]["name"], "step")
        self.assertTrue(config["augmentation"]["horizontal_flip"])
        self.assertFalse(config["augmentation"]["random_resized_crop"])

    def test_patience_is_stage_local(self) -> None:
        first = StageEarlyStopping(2, 0.0)
        self.assertEqual(first.update(0.5, 1), (True, False))
        self.assertEqual(first.update(0.4, 2), (False, False))
        self.assertEqual(first.update(0.4, 3), (False, True))
        second = StageEarlyStopping(2, 0.0)
        self.assertEqual(second.update(0.45, 1), (True, False))

    def test_metrics_include_balanced_accuracy_and_per_class_values(self) -> None:
        metrics = _classification_metrics([0, 0, 1, 1], [0, 1, 1, 1], ["0", "1"])
        self.assertAlmostEqual(metrics["balanced_accuracy"], 0.75)
        self.assertEqual(len(metrics["class_metrics"]), 2)
        self.assertEqual(metrics["confusion_matrix"], [[1, 1], [0, 2]])

    def test_collapse_diagnostics(self) -> None:
        diag = _logit_diagnostics([[0, 0], [0, 0]], [0, 0], 2)
        self.assertIn("EFFECTIVELY_CONSTANT_LOGITS", diag["failure_reasons"])
        self.assertIn("MISSING_PREDICTION_CLASSES", diag["failure_reasons"])

    def test_effective_number_weights_are_capped(self) -> None:
        weights = _effective_number_weights({"0": 1000, "1": 10}, ["0", "1"], cap=5.0)
        self.assertEqual(weights["0"], 1.0)
        self.assertLessEqual(weights["1"], 5.0)

    def test_seed_is_deterministic(self) -> None:
        import torch
        _seed_everything(1729)
        first = torch.rand(3)
        _seed_everything(1729)
        self.assertTrue(torch.equal(first, torch.rand(3)))

    def test_optimizer_updates_weights_and_scheduler_changes_lr(self) -> None:
        import torch
        from torch.utils.data import DataLoader, TensorDataset
        model = torch.nn.Linear(2, 2)
        before = {name: value.detach().clone() for name, value in model.state_dict().items()}
        config = load_training_config(None, default_binary_schema())
        config["scheduler"] = {"name": "step", "step_size": 1, "gamma": 0.5}
        optimizer = _make_optimizer(model, config, 0.01)
        scheduler = _make_scheduler(optimizer, config)
        loader = DataLoader(TensorDataset(torch.tensor([[1.0, 0.0], [0.0, 1.0]]), torch.tensor([0, 1])), batch_size=2)
        result = _run_epoch(model, loader, torch.nn.CrossEntropyLoss(), optimizer, torch.device("cpu"))
        self.assertGreater(_parameter_delta_l2(before, model), 0.0)
        self.assertTrue(result["loss"] > 0.0)
        _step_scheduler(scheduler, config, 0.5)
        self.assertAlmostEqual(optimizer.param_groups[0]["lr"], 0.005)


if __name__ == "__main__":
    unittest.main()
