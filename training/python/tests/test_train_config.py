from __future__ import annotations

import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

from droplet_trainer.schema import default_binary_schema
from droplet_trainer.train import (
    MIN_ONNX_OPSET,
    JsonlEmitter,
    _export_with_protocol_stdout_protected,
    _make_cuda_amp_scaler,
    _onnx_state_key_candidates,
    load_training_config,
)


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

    def test_legacy_onnx_opset_is_normalized_to_export_floor(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "training_config.json"
            config_path.write_text(json.dumps({"classes": ["0", "1"], "onnx_opset": 17}), encoding="utf-8")

            config = load_training_config(str(config_path), default_binary_schema())

        self.assertEqual(config["onnx_requested_opset"], 17)
        self.assertEqual(config["onnx_opset"], MIN_ONNX_OPSET)

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

    def test_onnx_state_key_candidates_accept_direct_and_parameterized_names(self) -> None:
        self.assertEqual(
            _onnx_state_key_candidates("features.3.expand1x1.weight"),
            ["features.3.expand1x1.weight"],
        )
        self.assertEqual(
            _onnx_state_key_candidates("p_features_3_expand1x1_weight"),
            ["p_features_3_expand1x1_weight", "features.3.expand1x1.weight"],
        )

    def test_onnx_exporter_chatter_cannot_contaminate_jsonl_stdout(self) -> None:
        protocol_stdout = io.StringIO()
        diagnostics_stderr = io.StringIO()
        original_stdout = sys.stdout
        original_stderr = sys.stderr
        try:
            sys.stdout = protocol_stdout
            sys.stderr = diagnostics_stderr
            _export_with_protocol_stdout_protected(
                lambda: print("exporter diagnostic")
            )
            JsonlEmitter("train", "run_export").emit("export_finished")
        finally:
            sys.stdout = original_stdout
            sys.stderr = original_stderr

        lines = protocol_stdout.getvalue().splitlines()
        self.assertEqual(len(lines), 1)
        self.assertEqual(json.loads(lines[0])["event"], "export_finished")
        self.assertEqual(diagnostics_stderr.getvalue().splitlines(),
                         ["exporter diagnostic"])


if __name__ == "__main__":
    unittest.main()
