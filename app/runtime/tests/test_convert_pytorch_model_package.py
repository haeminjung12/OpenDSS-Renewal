from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import torch


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
TRAINING_PYTHON = REPOSITORY_ROOT / "training" / "python"
sys.path.insert(0, str(TRAINING_PYTHON))

from droplet_trainer.train import _build_model


class UnsafePayload:
    pass


class ConvertPytorchModelPackageTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.converter = (
            REPOSITORY_ROOT
            / "app"
            / "runtime"
            / "scripts"
            / "convert_pytorch_model_package.py"
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def run_converter(self, checkpoint: Path, output: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                "-I",
                str(self.converter),
                "--checkpoint",
                str(checkpoint),
                "--architecture",
                "mobilenet_v3_small",
                "--name",
                "Converter Test",
                "--output",
                str(output),
            ],
            capture_output=True,
            check=False,
            text=True,
            timeout=90,
        )

    def test_safe_valid_checkpoint_publishes_complete_package(self) -> None:
        checkpoint = self.root / "valid.pth"
        model = _build_model(
            {
                "architecture": "mobilenet_v3_small",
                "initialization": {"mode": "checkpoint"},
                "seed": 42,
            },
            3,
        )
        torch.save({"model_state": model.state_dict()}, checkpoint)
        output = self.root / "valid-package"

        result = self.run_converter(checkpoint, output)

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((output / "model.onnx").is_file())
        self.assertTrue((output / "checkpoint.pth").is_file())
        metadata = json.loads((output / "metadata.json").read_text(encoding="utf-8"))
        self.assertEqual(metadata["schema_version"], "model-metadata-v2")
        self.assertEqual(metadata["status"], "trained")
        self.assertEqual(metadata["architecture"]["id"], "mobilenet_v3_small")

    def test_unsafe_custom_pickle_fails_closed_without_output(self) -> None:
        checkpoint = self.root / "unsafe.pth"
        torch.save({"model_state": {}, "payload": UnsafePayload()}, checkpoint)
        output = self.root / "unsafe-package"

        result = self.run_converter(checkpoint, output)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("safe weights-only loader", result.stderr)
        self.assertFalse(output.exists())
        self.assertEqual(list(self.root.glob(".unsafe-package.staging-*")), [])


if __name__ == "__main__":
    unittest.main()
