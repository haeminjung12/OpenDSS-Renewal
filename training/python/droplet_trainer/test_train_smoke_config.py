from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from droplet_trainer.cli import build_parser
from droplet_trainer.schema import default_binary_schema
from droplet_trainer.train import DEFAULT_CONFIG, load_training_config


class TrainSmokeConfigTests(unittest.TestCase):
    def test_train_parser_accepts_smoke_without_changing_existing_options(self) -> None:
        args = build_parser().parse_args(
            [
                "train",
                "--dataset",
                "dataset",
                "--output",
                "output",
                "--config",
                "config.json",
                "--device",
                "cpu",
                "--run-name",
                "smoke-run",
                "--jsonl",
                "--smoke",
            ]
        )

        self.assertTrue(args.smoke)
        self.assertEqual(args.config, "config.json")
        self.assertEqual(args.device, "cpu")
        self.assertEqual(args.run_name, "smoke-run")
        self.assertTrue(args.jsonl)

    def test_default_training_config_is_unchanged_without_smoke(self) -> None:
        config = load_training_config(None, default_binary_schema())

        self.assertEqual(config["stages"], DEFAULT_CONFIG["stages"])
        self.assertEqual(config["epochs"], DEFAULT_CONFIG["epochs"])
        self.assertTrue(config["export_onnx"])
        self.assertEqual(config["onnx_opset"], DEFAULT_CONFIG["onnx_opset"])

    def test_smoke_overrides_loaded_config_to_remain_bounded(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "training_config.json"
            config_path.write_text(
                json.dumps(
                    {
                        "batch_size": 64,
                        "epochs": 99,
                        "patience": 10,
                        "export_onnx": True,
                        "stages": [
                            {"name": "long", "epochs": 99, "learning_rate": 0.1, "trainable": "all"},
                        ],
                    }
                ),
                encoding="utf-8",
            )

            config = load_training_config(str(config_path), default_binary_schema(), smoke=True)

        self.assertEqual(config["batch_size"], 8)
        self.assertEqual(config["epochs"], 1)
        self.assertEqual(config["patience"], 1)
        self.assertFalse(config["export_onnx"])
        self.assertEqual(len(config["stages"]), 1)
        self.assertEqual(config["stages"][0]["name"], "smoke")
        self.assertEqual(config["stages"][0]["epochs"], 1)


if __name__ == "__main__":
    unittest.main()
