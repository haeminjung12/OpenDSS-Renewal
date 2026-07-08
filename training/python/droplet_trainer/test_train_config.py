from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from droplet_trainer.schema import default_binary_schema
from droplet_trainer.train import _make_cuda_amp_scaler, load_training_config


class TrainingConfigTests(unittest.TestCase):
    def test_plain_utf8_config_loads(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "training_config.json"
            config_path.write_text(json.dumps({"batch_size": 4, "classes": ["0", "1"]}), encoding="utf-8")

            config = load_training_config(str(config_path), default_binary_schema())

        self.assertEqual(config["batch_size"], 4)
        self.assertEqual(config["classes"], ["0", "1"])

    def test_utf8_bom_config_loads(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "training_config.json"
            config_path.write_text(json.dumps({"batch_size": 8, "classes": ["0", "1"]}), encoding="utf-8-sig")

            config = load_training_config(str(config_path), default_binary_schema())

        self.assertEqual(config["batch_size"], 8)
        self.assertEqual(config["classes"], ["0", "1"])

    def test_cuda_amp_scaler_uses_current_torch_amp_api(self) -> None:
        calls: list[tuple[str, tuple[object, ...], dict[str, object]]] = []

        class FakeAmp:
            @staticmethod
            def GradScaler(*args: object, **kwargs: object) -> str:
                calls.append(("torch.amp", args, kwargs))
                return "scaler"

        class DeprecatedCudaAmp:
            @staticmethod
            def GradScaler(*args: object, **kwargs: object) -> str:
                calls.append(("torch.cuda.amp", args, kwargs))
                return "deprecated"

        class FakeCuda:
            amp = DeprecatedCudaAmp()

        class FakeTorch:
            amp = FakeAmp()
            cuda = FakeCuda()

        scaler = _make_cuda_amp_scaler(FakeTorch())

        self.assertEqual(scaler, "scaler")
        self.assertEqual(calls, [("torch.amp", ("cuda",), {})])


if __name__ == "__main__":
    unittest.main()
