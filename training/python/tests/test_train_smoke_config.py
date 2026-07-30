from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from droplet_trainer.cli import build_parser
from droplet_trainer.env import run_env_check
from droplet_trainer.schema import default_binary_schema
from droplet_trainer.train import DEFAULT_CONFIG, _device_warnings, load_training_config


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
                "--preflight-only",
            ]
        )

        self.assertTrue(args.smoke)
        self.assertTrue(args.preflight_only)
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

    def test_cuda_request_cpu_fallback_warning_is_reported(self) -> None:
        class CpuDevice:
            type = "cpu"

        warnings = _device_warnings("cuda", CpuDevice())

        self.assertEqual(warnings[0]["code"], "REQUESTED_DEVICE_FALLBACK_CPU")

    def test_env_check_cuda_request_falls_back_to_cpu_without_error(self) -> None:
        class FakeCuda:
            @staticmethod
            def is_available() -> bool:
                return False

            @staticmethod
            def device_count() -> int:
                return 0

            @staticmethod
            def get_device_name(index: int) -> str:
                raise IndexError(index)

        class FakeTorch:
            cuda = FakeCuda()
            version = SimpleNamespace(cuda=None)

        class FakeOnnxRuntime:
            @staticmethod
            def get_available_providers() -> list[str]:
                return ["CPUExecutionProvider"]

        args = SimpleNamespace(
            require_training=False,
            require_onnx=False,
            device="cuda",
            check_output=None,
            allow_untested_python=True,
        )

        with patch("droplet_trainer.env._import_status", return_value={"status": "ok", "version": "test"}), patch.dict(
            sys.modules, {"torch": FakeTorch, "onnxruntime": FakeOnnxRuntime}
        ):
            payload, exit_code = run_env_check(args)

        self.assertEqual(exit_code, 0)
        self.assertEqual(payload["status"], "ok")
        self.assertEqual(payload["devices"]["selected"], "cpu")
        self.assertEqual(payload["warnings"][0]["code"], "REQUESTED_DEVICE_FALLBACK_CPU")


if __name__ == "__main__":
    unittest.main()
