from __future__ import annotations

import hashlib
import io
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from droplet_trainer.model_test_batch_backend import (
    BatchAllocationError,
    EvaluationItem,
    LoadedCheckpoint,
    TorchBatchBackend,
    automatic_torch_device,
    evaluate_ordered_batches,
    load_active_checkpoint,
    run_model_test_process,
)


def items(count: int) -> list[EvaluationItem]:
    return [
        EvaluationItem(
            sequence=index,
            image_path=f"crop-{index}.tiff",
            true_class_id=str(index % 2),
            record_id=f"record-{index}",
        )
        for index in range(count)
    ]


class FakeBackend:
    def __init__(self, allocation_limit: int) -> None:
        self.allocation_limit = allocation_limit
        self.prepare_sizes: list[int] = []
        self.infer_sizes: list[int] = []

    def prepare(self, batch):
        self.prepare_sizes.append(len(batch))
        if len(batch) > self.allocation_limit:
            raise BatchAllocationError("test allocation limit")
        return list(batch)

    def infer(self, prepared):
        self.infer_sizes.append(len(prepared))
        return [
            [0.8, 0.2] if item.sequence % 2 == 0 else [0.1, 0.9]
            for item in prepared
        ]


class FakeModel:
    def __init__(self) -> None:
        self.loaded = None
        self.strict = None
        self.eval_called = False

    def load_state_dict(self, state, strict):
        self.loaded = state
        self.strict = strict

    def eval(self):
        self.eval_called = True


class ModelTestBatchBackendTest(unittest.TestCase):
    def setUp(self) -> None:
        def reject_network(*_args, **_kwargs):
            raise AssertionError("Model Test backend tests must not use the network.")

        self._network_patchers = [
            patch("socket.create_connection", side_effect=reject_network),
            patch("urllib.request.urlopen", side_effect=reject_network),
        ]
        for patcher in self._network_patchers:
            patcher.start()
            self.addCleanup(patcher.stop)

    def test_allocation_backoff_preserves_order_and_completed_batch_commits(self) -> None:
        backend = FakeBackend(allocation_limit=2)
        commits = []
        events = []
        processed = evaluate_ordered_batches(
            items(5),
            ["0", "1"],
            backend,
            lambda index, facts: commits.append((index, list(facts))),
            events.append,
            lambda: False,
            qualified_batch_ceiling=8,
        )

        self.assertEqual(processed, 5)
        self.assertEqual(backend.prepare_sizes, [5, 2, 2, 1])
        self.assertEqual(backend.infer_sizes, [2, 2, 1])
        self.assertEqual(
            [fact.sequence for _, batch in commits for fact in batch],
            [0, 1, 2, 3, 4],
        )
        self.assertEqual(
            [event["sequence"] for event in events if event["event"] == "image_evaluated"],
            [0, 1, 2, 3, 4],
        )
        self.assertEqual(
            [event["processed"] for event in events if event["event"] == "batch_completed"],
            [2, 4, 5],
        )

    def test_stop_begins_no_later_batch_and_retains_completed_batch(self) -> None:
        backend = FakeBackend(allocation_limit=10)
        commits = []
        events = []
        stopped = False

        def commit(index, facts):
            nonlocal stopped
            commits.append((index, list(facts)))
            stopped = True

        processed = evaluate_ordered_batches(
            items(5),
            ["0", "1"],
            backend,
            commit,
            events.append,
            lambda: stopped,
            qualified_batch_ceiling=2,
        )

        self.assertEqual(processed, 2)
        self.assertEqual(backend.infer_sizes, [2])
        self.assertEqual(len(commits), 1)
        self.assertEqual(
            [event["sequence"] for event in events if event["event"] == "image_evaluated"],
            [0, 1],
        )

    def test_failed_commit_publishes_no_image_or_progress_facts(self) -> None:
        events = []

        def fail_commit(_index, _facts):
            raise OSError("checkpoint failed")

        with self.assertRaises(OSError):
            evaluate_ordered_batches(
                items(2),
                ["0", "1"],
                FakeBackend(allocation_limit=2),
                fail_commit,
                events.append,
                lambda: False,
                qualified_batch_ceiling=2,
            )
        self.assertEqual(events, [])

    def test_batch_one_allocation_failure_is_truthful(self) -> None:
        with self.assertRaises(BatchAllocationError):
            evaluate_ordered_batches(
                items(1),
                ["0", "1"],
                FakeBackend(allocation_limit=0),
                lambda _index, _facts: None,
                lambda _event: None,
                lambda: False,
                qualified_batch_ceiling=8,
            )

    def test_automatic_device_is_gpu_first_with_cpu_fallback(self) -> None:
        fake_torch = types.SimpleNamespace(
            cuda=types.SimpleNamespace(is_available=lambda: True)
        )
        with patch.dict(sys.modules, {"torch": fake_torch}):
            self.assertEqual(automatic_torch_device(), "cuda")
        fake_torch.cuda.is_available = lambda: False
        with patch.dict(sys.modules, {"torch": fake_torch}):
            self.assertEqual(automatic_torch_device(), "cpu")

    def test_active_package_checkpoint_load_is_strict_and_offline(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            checkpoint = Path(temporary) / "checkpoint.pth"
            checkpoint.write_bytes(b"checkpoint fixture")
            payload = {
                "model_state": {"classifier.weight": object()},
                "class_count": 2,
                "class_ids": ["0", "1"],
                "config": {
                    "architecture": "mobilenet_v3_small",
                    "initialization": {"mode": "imagenet"},
                },
            }
            fake_torch = types.SimpleNamespace(
                load=lambda path, **kwargs: payload,
                cuda=types.SimpleNamespace(is_available=lambda: False),
            )
            model = FakeModel()
            seen_config = {}

            def build(config, class_count):
                seen_config.update(config)
                self.assertEqual(class_count, 2)
                return model

            with patch.dict(sys.modules, {"torch": fake_torch}):
                loaded = load_active_checkpoint(checkpoint, build)

            self.assertEqual(loaded.class_ids, ("0", "1"))
            self.assertEqual(
                loaded.sha256,
                hashlib.sha256(b"checkpoint fixture").hexdigest(),
            )
            self.assertEqual(seen_config["initialization"], {"mode": "checkpoint"})
            self.assertIs(model.loaded, payload["model_state"])
            self.assertTrue(model.strict)
            self.assertTrue(model.eval_called)

    def test_checkpoint_rejects_class_metadata_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            checkpoint = Path(temporary) / "checkpoint.pth"
            checkpoint.write_bytes(b"bad checkpoint fixture")
            fake_torch = types.SimpleNamespace(
                load=lambda path, **kwargs: {
                    "model_state": {},
                    "class_count": 3,
                    "class_ids": ["0", "1"],
                    "config": {"architecture": "mobilenet_v3_small"},
                }
            )
            with patch.dict(sys.modules, {"torch": fake_torch}):
                with self.assertRaisesRegex(ValueError, "class count"):
                    load_active_checkpoint(checkpoint, lambda _config, _count: FakeModel())

    def test_process_protocol_stops_before_later_batch_after_commit_ack(self) -> None:
        request = {
            "schema": "opendss.model_test.request.v1",
            "class_ids": ["0", "1"],
            "items": [
                {
                    "sequence": index,
                    "record_id": f"record-{index}",
                    "image_path": f"crop-{index}.png",
                    "true_class_id": str(index),
                }
                for index in range(2)
            ],
        }
        protocol_input = io.StringIO(
            "\n".join(
                (
                    json.dumps(request),
                    json.dumps({"command": "start"}),
                    json.dumps(
                        {"command": "committed", "batch_index": 0, "stop": True}
                    ),
                )
            )
            + "\n"
        )
        protocol_output = io.StringIO()
        checkpoint = LoadedCheckpoint(
            model=FakeModel(),
            class_ids=("0", "1"),
            config={},
            sha256="a" * 64,
        )
        backend = FakeBackend(allocation_limit=1)
        backend.device = "cpu"
        with (
            patch(
                "droplet_trainer.model_test_batch_backend.load_active_checkpoint",
                return_value=checkpoint,
            ),
            patch(
                "droplet_trainer.model_test_batch_backend.TorchBatchBackend",
                return_value=backend,
            ),
            patch("sys.stdin", protocol_input),
            patch("sys.stdout", protocol_output),
        ):
            exit_code = run_model_test_process(
                SimpleNamespace(checkpoint="checkpoint.pth")
            )

        events = [
            json.loads(line)
            for line in protocol_output.getvalue().splitlines()
            if line
        ]
        self.assertEqual(exit_code, 0)
        self.assertEqual([event["event"] for event in events],
                         ["ready", "batch_ready", "run_finished"])
        self.assertEqual(events[1]["batch_index"], 0)
        self.assertEqual([fact["sequence"] for fact in events[1]["facts"]], [0])
        self.assertEqual(events[2]["status"], "stopped")
        self.assertEqual(events[2]["processed"], 1)
        self.assertEqual(backend.infer_sizes, [1])

    def test_torch_adapter_batches_real_cpu_tensors_when_available(self) -> None:
        try:
            import torch
            from PIL import Image
        except ImportError as exc:
            self.skipTest(str(exc))

        with tempfile.TemporaryDirectory() as temporary:
            image_paths = []
            for index, value in enumerate((0, 255)):
                path = Path(temporary) / f"{index}.png"
                Image.new("RGB", (1, 1), (value, value, value)).save(path)
                image_paths.append(path)

            class TinyModel(torch.nn.Module):
                def forward(self, tensor):
                    value = tensor.reshape(tensor.shape[0], -1).mean(dim=1)
                    return torch.stack((1.0 - value, value), dim=1)

            checkpoint = LoadedCheckpoint(
                model=TinyModel(),
                class_ids=("0", "1"),
                config={},
                sha256="0" * 64,
            )
            with patch(
                "droplet_trainer.model_test_batch_backend.automatic_torch_device",
                return_value="cpu",
            ):
                backend = TorchBatchBackend(
                    checkpoint,
                    transform=lambda image: torch.tensor(
                        image.getpixel((0, 0)), dtype=torch.float32
                    )
                    / 255.0,
                )
            prepared = backend.prepare(
                [
                    EvaluationItem(index, str(path), str(index), f"record-{index}")
                    for index, path in enumerate(image_paths)
                ]
            )
            self.assertEqual(backend.device, "cpu")
            self.assertEqual(backend.infer(prepared), [[1.0, 0.0], [0.0, 1.0]])


if __name__ == "__main__":
    unittest.main()
