from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from PIL import Image

from droplet_trainer.schema import parse_schema
from droplet_trainer.validate_images import run_validate_images

try:
    import numpy as np
    import onnx
    from onnx import TensorProto, helper, numpy_helper
    import onnxruntime  # noqa: F401
except ImportError:  # pragma: no cover - environment capability gate
    onnx = None


@unittest.skipIf(onnx is None, "ONNX integration dependencies are unavailable")
class ValidateImagesSchemaContractTests(unittest.TestCase):
    def _run_case(self, class_count: int) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dataset = root / "dataset"
            items = []
            semantics = {}
            for class_index in range(class_count):
                class_id = str(class_index)
                semantics[class_id] = f"Class {class_id}"
                image_path = dataset / "images" / class_id / f"{class_id}.png"
                image_path.parent.mkdir(parents=True, exist_ok=True)
                Image.new("RGB", (96, 96), (class_index * 30, 0, 0)).save(image_path)
                items.append({"path": str(image_path), "class_id": class_id, "reviewed_label": class_id})
            manifest = dataset / "metadata" / "dataset_manifest.json"
            manifest.parent.mkdir(parents=True)
            manifest.write_text(json.dumps({"class_semantics": semantics, "items": items}), encoding="utf-8")

            package = root / "package"
            package.mkdir()
            model_path = package / "model.onnx"
            input_info = helper.make_tensor_value_info("input", TensorProto.FLOAT, [None, 3, 96, 96])
            output_info = helper.make_tensor_value_info("logits", TensorProto.FLOAT, [None, class_count])
            weights = numpy_helper.from_array(np.zeros((class_count, 3), dtype=np.float32), "weights")
            bias = numpy_helper.from_array(np.zeros(class_count, dtype=np.float32), "bias")
            graph = helper.make_graph(
                [helper.make_node("GlobalAveragePool", ["input"], ["pooled"]),
                 helper.make_node("Flatten", ["pooled"], ["flat"], axis=1),
                 helper.make_node("Gemm", ["flat", "weights", "bias"], ["logits"], transB=1)],
                "schema-contract", [input_info], [output_info], [weights, bias])
            model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 18)])
            model.ir_version = 10
            onnx.save(model, model_path)
            metadata_path = package / "metadata.json"
            metadata_path.write_text(json.dumps({
                "schema_version": "simple-model-package-v1",
                "classes": list(semantics),
                "class_to_idx": {class_id: index for index, class_id in enumerate(semantics)},
                "display_labels": semantics,
                "input_size": [96, 96, 3],
                "normalization": {"mean": [0.485, 0.456, 0.406], "std": [0.229, 0.224, 0.225]},
            }), encoding="utf-8")
            args = SimpleNamespace(model=str(model_path), metadata=str(metadata_path), dataset=str(manifest),
                                   output=str(root / "output"), device="cpu", promotion_gate=False,
                                   class_schema=None, classes=None, legacy_schema=False, json=False, jsonl=False)
            schema = parse_schema(args)
            payload, exit_code = run_validate_images(args, schema)
            self.assertEqual(exit_code, 0)
            self.assertEqual(payload["dataset"]["class_counts"], {str(i): 1 for i in range(class_count)})
            self.assertEqual(payload["metadata"]["classes"], [str(i) for i in range(class_count)])
            return payload

    def test_two_class_command_infers_dataset_metadata_without_override(self) -> None:
        self._run_case(2)

    def test_three_class_command_preserves_class_two_without_override(self) -> None:
        payload = self._run_case(3)
        self.assertEqual(payload["dataset"]["class_counts"]["2"], 1)


if __name__ == "__main__":
    unittest.main()
