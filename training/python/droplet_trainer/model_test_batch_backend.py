from __future__ import annotations

import hashlib
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Protocol, Sequence


class BatchAllocationError(RuntimeError):
    """Raised only when a batch cannot be prepared before inference starts."""


AUTOMATIC_BATCH_CEILING = 256


@dataclass(frozen=True)
class EvaluationItem:
    sequence: int
    image_path: str
    true_class_id: str
    record_id: str


@dataclass(frozen=True)
class EvaluationFact:
    sequence: int
    image_path: str
    true_class_id: str
    predicted_class_id: str
    class_scores: tuple[float, ...]
    record_id: str


@dataclass(frozen=True)
class LoadedCheckpoint:
    model: Any
    class_ids: tuple[str, ...]
    config: dict[str, Any]
    sha256: str


class BatchBackend(Protocol):
    def prepare(self, items: Sequence[EvaluationItem]) -> Any:
        """Allocate and preprocess a batch without starting inference."""

    def infer(self, prepared_batch: Any) -> Sequence[Sequence[float]]:
        """Run one already-prepared batch and return one score vector per item."""


class TorchBatchBackend:
    """PyTorch adapter for an already validated active package checkpoint."""

    def __init__(
        self,
        checkpoint: LoadedCheckpoint,
        *,
        transform: Callable[[Any], Any] | None = None,
    ) -> None:
        import torch

        self._torch = torch
        self._device = automatic_torch_device()
        try:
            self._model = checkpoint.model.to(self._device)
        except (torch.cuda.OutOfMemoryError, RuntimeError):
            if self._device != "cuda":
                raise
            torch.cuda.empty_cache()
            self._device = "cpu"
            self._model = checkpoint.model.to(self._device)
        self._model.eval()
        if transform is None:
            from .train import _build_transforms

            _, transform = _build_transforms(checkpoint.config)
        self._transform = transform

    @property
    def device(self) -> str:
        return self._device

    def prepare(self, items: Sequence[EvaluationItem]) -> Any:
        from PIL import Image

        try:
            tensors = []
            for item in items:
                with Image.open(item.image_path) as image:
                    tensors.append(self._transform(image.convert("RGB")))
            return self._torch.stack(tensors).to(self._device)
        except (self._torch.cuda.OutOfMemoryError, MemoryError) as exc:
            if self._device == "cuda":
                self._torch.cuda.empty_cache()
            raise BatchAllocationError(str(exc)) from exc

    def infer(self, prepared_batch: Any) -> Sequence[Sequence[float]]:
        with self._torch.inference_mode():
            output = self._model(prepared_batch)
        if getattr(output, "ndim", 0) != 2:
            raise ValueError("Model Test checkpoint must return a batch of class scores.")
        return output.detach().cpu().tolist()


CommitBatch = Callable[[int, Sequence[EvaluationFact]], None]
EmitEvent = Callable[[dict[str, Any]], None]
StopRequested = Callable[[], bool]


def automatic_torch_device() -> str:
    import torch

    return "cuda" if torch.cuda.is_available() else "cpu"


def load_active_checkpoint(
    checkpoint_path: str | Path,
    model_builder: Callable[[dict[str, Any], int], Any] | None = None,
) -> LoadedCheckpoint:
    """Load the current trainer package checkpoint without downloading weights."""
    import torch

    path = Path(checkpoint_path).expanduser().resolve()
    payload = torch.load(path, map_location="cpu", weights_only=False)
    if not isinstance(payload, dict) or not isinstance(payload.get("model_state"), dict):
        raise ValueError("Active checkpoint does not contain model_state.")

    raw_classes = payload.get("class_ids")
    config = payload.get("config")
    if (
        not isinstance(raw_classes, list)
        or not raw_classes
        or any(not isinstance(value, str) or not value for value in raw_classes)
        or len(set(raw_classes)) != len(raw_classes)
        or not isinstance(config, dict)
    ):
        raise ValueError("Active checkpoint class/config metadata is invalid.")
    if payload.get("class_count") not in (None, len(raw_classes)):
        raise ValueError("Active checkpoint class count does not match class_ids.")

    build = model_builder
    if build is None:
        from .train import _build_model

        build = _build_model
    build_config = dict(config)
    build_config["initialization"] = {"mode": "checkpoint"}
    model = build(build_config, len(raw_classes))
    model.load_state_dict(payload["model_state"], strict=True)
    model.eval()
    return LoadedCheckpoint(
        model=model,
        class_ids=tuple(raw_classes),
        config=dict(config),
        sha256=_sha256_file(path),
    )


def evaluate_ordered_batches(
    items: Sequence[EvaluationItem],
    class_ids: Sequence[str],
    backend: BatchBackend,
    commit_batch: CommitBatch,
    emit_event: EmitEvent,
    stop_requested: StopRequested,
    *,
    qualified_batch_ceiling: int,
) -> int:
    """
    Evaluate in input order.

    `commit_batch` is the persistence boundary: image/progress events are emitted
    only after it returns successfully for the whole completed batch.
    """
    if qualified_batch_ceiling <= 0:
        raise ValueError("qualified_batch_ceiling must be positive.")
    classes = tuple(class_ids)
    if not classes or len(set(classes)) != len(classes):
        raise ValueError("class_ids must be non-empty and unique.")

    processed = 0
    batch_index = 0
    batch_size = min(qualified_batch_ceiling, len(items))
    while processed < len(items):
        if stop_requested():
            break

        current_size = min(batch_size, len(items) - processed)
        while True:
            batch_items = items[processed : processed + current_size]
            try:
                prepared = backend.prepare(batch_items)
                break
            except BatchAllocationError:
                if current_size == 1:
                    raise
                current_size = max(1, current_size // 2)
                batch_size = current_size

        raw_scores = backend.infer(prepared)
        if len(raw_scores) != len(batch_items):
            raise ValueError("Inference result count does not match the prepared batch.")

        facts: list[EvaluationFact] = []
        for item, scores_value in zip(batch_items, raw_scores):
            scores = tuple(float(value) for value in scores_value)
            if len(scores) != len(classes) or any(
                not math.isfinite(value) for value in scores
            ):
                raise ValueError(
                    "Inference class scores must match class_ids and be finite."
                )
            predicted_index = max(range(len(scores)), key=scores.__getitem__)
            facts.append(
                EvaluationFact(
                    sequence=item.sequence,
                    image_path=item.image_path,
                    true_class_id=item.true_class_id,
                    predicted_class_id=classes[predicted_index],
                    class_scores=scores,
                    record_id=item.record_id,
                )
            )

        commit_batch(batch_index, facts)
        processed += len(facts)
        for fact in facts:
            emit_event(
                {
                    "schema_version": 1,
                    "event": "image_evaluated",
                    "sequence": fact.sequence,
                    "record_id": fact.record_id,
                    "image_path": fact.image_path,
                    "true_class_id": fact.true_class_id,
                    "predicted_class_id": fact.predicted_class_id,
                    "class_scores": list(fact.class_scores),
                }
            )
        emit_event(
            {
                "schema_version": 1,
                "event": "batch_completed",
                "batch_index": batch_index,
                "batch_size": len(facts),
                "processed": processed,
                "total": len(items),
            }
        )
        batch_index += 1
    return processed


def emit_jsonl(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, separators=(",", ":"), ensure_ascii=False), flush=True)


def run_model_test_process(args: Any) -> int:
    """Run the line-delimited process protocol used by the C++ Model Test owner."""
    try:
        request = _read_protocol_line()
        if set(request) != {"schema", "class_ids", "items"} or request.get(
            "schema"
        ) != "opendss.model_test.request.v1":
            raise ValueError("Model Test request schema is invalid.")
        class_ids = request["class_ids"]
        raw_items = request["items"]
        if (
            not isinstance(class_ids, list)
            or not class_ids
            or any(not isinstance(value, str) or not value for value in class_ids)
            or len(set(class_ids)) != len(class_ids)
            or not isinstance(raw_items, list)
            or not raw_items
        ):
            raise ValueError("Model Test request classes/items are invalid.")
        evaluation_items = []
        for index, raw in enumerate(raw_items):
            if not isinstance(raw, dict) or set(raw) != {
                "sequence",
                "record_id",
                "image_path",
                "true_class_id",
            }:
                raise ValueError("Model Test item schema is invalid.")
            if (
                raw["sequence"] != index
                or any(
                    not isinstance(raw[key], str) or not raw[key]
                    for key in ("record_id", "image_path", "true_class_id")
                )
            ):
                raise ValueError("Model Test item sequence is not contiguous.")
            evaluation_items.append(
                EvaluationItem(
                    sequence=index,
                    record_id=raw["record_id"],
                    image_path=raw["image_path"],
                    true_class_id=raw["true_class_id"],
                )
            )

        checkpoint = load_active_checkpoint(args.checkpoint)
        if tuple(class_ids) != checkpoint.class_ids:
            raise ValueError("Dataset classes do not match the active checkpoint.")
        backend = TorchBatchBackend(checkpoint)
        emit_jsonl(
            {
                "schema_version": 1,
                "event": "ready",
                "checkpoint_sha256": checkpoint.sha256,
                "device": backend.device,
                "total": len(evaluation_items),
            }
        )
        start = _read_protocol_line()
        if start != {"command": "start"}:
            raise ValueError("Model Test start acknowledgement is invalid.")

        stopped = False

        def commit(batch_index: int, facts: Sequence[EvaluationFact]) -> None:
            nonlocal stopped
            emit_jsonl(
                {
                    "schema_version": 1,
                    "event": "batch_ready",
                    "batch_index": batch_index,
                    "facts": [
                        {
                            "sequence": fact.sequence,
                            "record_id": fact.record_id,
                            "image_path": fact.image_path,
                            "true_class_id": fact.true_class_id,
                            "predicted_class_id": fact.predicted_class_id,
                            "class_scores": list(fact.class_scores),
                        }
                        for fact in facts
                    ],
                }
            )
            acknowledgement = _read_protocol_line()
            if (
                set(acknowledgement) != {"command", "batch_index", "stop"}
                or acknowledgement.get("command") != "committed"
                or acknowledgement.get("batch_index") != batch_index
                or not isinstance(acknowledgement.get("stop"), bool)
            ):
                raise ValueError("Model Test batch acknowledgement is invalid.")
            stopped = acknowledgement["stop"]

        processed = evaluate_ordered_batches(
            evaluation_items,
            checkpoint.class_ids,
            backend,
            commit,
            lambda _payload: None,
            lambda: stopped,
            qualified_batch_ceiling=AUTOMATIC_BATCH_CEILING,
        )
        emit_jsonl(
            {
                "schema_version": 1,
                "event": "run_finished",
                "status": "stopped" if stopped else "completed",
                "processed": processed,
                "total": len(evaluation_items),
            }
        )
        return 0
    except BaseException as exc:
        emit_jsonl(
            {
                "schema_version": 1,
                "event": "run_failed",
                "error": str(exc),
            }
        )
        return 1


def _read_protocol_line() -> dict[str, Any]:
    line = sys.stdin.readline()
    if not line:
        raise EOFError("Model Test process input closed unexpectedly.")
    value = json.loads(line)
    if not isinstance(value, dict):
        raise ValueError("Model Test process message must be a JSON object.")
    return value


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()
