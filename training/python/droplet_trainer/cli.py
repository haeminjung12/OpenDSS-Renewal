from __future__ import annotations

import argparse
import json
from typing import Any

from .dataset import inspect_dataset, validate_dataset
from .env import run_env_check
from .errors import EXIT_USAGE, handle_exception, json_error
from .metadata import metadata_scaffold
from .schema import parse_schema
from .train import run_train
from .validate_images import run_validate_images
from .validate_sequence import run_validate_sequence
from .validate_sequence_runner import run_validate_sequence_runner


def add_common_dataset_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--class-schema", help="Path to a JSON class schema.")
    parser.add_argument("--classes", help="Comma-separated ordered class ids. Defaults to binary 0,1.")
    parser.add_argument("--legacy-schema", action="store_true", help="Use legacy Empty,Single,MoreThanTwo schema.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="python -m droplet_trainer")
    sub = parser.add_subparsers(dest="command", required=True)

    env = sub.add_parser("env-check")
    env.add_argument("--device", choices=["auto", "cpu", "cuda"], default="auto")
    env.add_argument("--check-output")
    env.add_argument("--require-training", action="store_true")
    env.add_argument("--require-onnx", action="store_true")
    env.add_argument("--allow-untested-python", action="store_true")
    env.add_argument("--json", action="store_true")

    inspect = sub.add_parser("dataset-inspect")
    inspect.add_argument("--dataset", required=True)
    add_common_dataset_args(inspect)
    inspect.add_argument("--json", action="store_true")

    validate = sub.add_parser("dataset-validate")
    validate.add_argument("--dataset", required=True)
    validate.add_argument("--output")
    validate.add_argument("--min-per-class", type=int, default=10)
    validate.add_argument("--split", default="train=0.70,val=0.15,test=0.15")
    validate.add_argument("--seed", type=int, default=42)
    validate.add_argument("--write-manifest", action="store_true")
    validate.add_argument("--prepare", action="store_true", help="Copy images into datasets/prepared/<dataset_id>/ style layout.")
    validate.add_argument("--prepared-root", help="Prepared dataset parent folder. Defaults to repo datasets/prepared.")
    validate.add_argument("--dataset-id", help="Prepared/finalized dataset id.")
    validate.add_argument("--force", action="store_true")
    validate.add_argument("--balancing", choices=["inverse-count-min-normalized", "none"], default="inverse-count-min-normalized")
    validate.add_argument(
        "--waste-source-mode",
        choices=["new-reviewed-only", "existing-reviewed-only", "mixed-reviewed"],
        default="new-reviewed-only",
        help="Select which reviewed waste examples are eligible: new current-session waste, existing manifest-backed waste, or both.",
    )
    add_common_dataset_args(validate)
    validate.add_argument("--json", action="store_true")

    train = sub.add_parser("train")
    train.add_argument("--dataset", required=True)
    train.add_argument("--output", required=True)
    train.add_argument("--config")
    train.add_argument("--smoke", action="store_true", help="Apply a bounded one-epoch smoke-training configuration after loading any config file.")
    train.add_argument("--device", choices=["auto", "cpu", "cuda"], default="auto")
    train.add_argument("--run-name")
    train.add_argument(
        "--waste-source-mode",
        choices=["new-reviewed-only", "existing-reviewed-only", "mixed-reviewed"],
        default="new-reviewed-only",
        help="Select which reviewed waste examples are eligible: new current-session waste, existing manifest-backed waste, or both.",
    )
    add_common_dataset_args(train)
    train.add_argument("--jsonl", action="store_true")

    val_images = sub.add_parser("validate-images")
    val_images.add_argument("--model", required=True)
    val_images.add_argument("--metadata", required=True)
    val_images.add_argument("--dataset", required=True)
    val_images.add_argument("--output", required=True)
    val_images.add_argument("--device", choices=["auto", "cpu", "cuda"], default="auto")
    val_images.add_argument("--promotion-gate", action="store_true")
    add_common_dataset_args(val_images)
    val_images.add_argument("--json", action="store_true")
    val_images.add_argument("--jsonl", action="store_true")

    val_sequence = sub.add_parser("validate-sequence")
    sequence_source = val_sequence.add_mutually_exclusive_group(required=True)
    sequence_source.add_argument("--event-decisions", help="Path to sequence_event_decisions_<timestamp>.csv.")
    sequence_source.add_argument("--sequence-runner", help="Path to sequence_headless executable for runner-wrapped validation.")
    val_sequence.add_argument("--sequence", help="Path to ordered sequence frame folder for runner-wrapped validation.")
    val_sequence.add_argument("--fps", help="Positive replay FPS for runner-wrapped validation.")
    val_sequence.add_argument("--model", help="Path to ONNX model for runner-wrapped validation.")
    val_sequence.add_argument("--metadata", help="Path to model/runtime metadata JSON for runner-wrapped validation.")
    val_sequence.add_argument("--target-class-id", help="Target class id that should produce a trigger decision.")
    val_sequence.add_argument("--detector-settings", help="Optional detector settings snapshot JSON. If omitted in runner-wrapped mode, current hardcoded defaults are snapshotted.")
    val_sequence.add_argument("--settings-snapshot", help="Optional full runtime settings snapshot JSON to preserve with the run.")
    val_sequence.add_argument("--provider", choices=["cpu", "cuda"], default="cpu", help="ONNX execution provider passed to sequence runner.")
    val_sequence.add_argument("--batch-size", type=int, default=0, help="Batch size passed to sequence runner. 0 lets the runner choose.")
    val_sequence.add_argument("--ground-truth", help="Path to sequence_ground_truth.json. Required for artifact-driven comparison; optional for runner-wrapped descriptive replay.")
    val_sequence.add_argument("--review-checklist", help="Optional adjudication checklist CSV from sequence ground-truth review.")
    val_sequence.add_argument("--output", required=True)
    add_common_dataset_args(val_sequence)
    val_sequence.add_argument("--json", action="store_true")
    val_sequence.add_argument("--jsonl", action="store_true")

    meta = sub.add_parser("metadata-scaffold")
    add_common_dataset_args(meta)
    meta.add_argument("--json", action="store_true")
    return parser


def print_payload(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, indent=2, sort_keys=False))


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    try:
        args = parser.parse_args(argv)
    except SystemExit as exc:
        if int(exc.code) == 0:
            return 0
        return json_error("unknown", "CLI_USAGE_ERROR", "Invalid command-line arguments.", EXIT_USAGE, {"exit_code": int(exc.code)})

    command = args.command
    try:
        if command == "env-check":
            payload, exit_code = run_env_check(args)
        elif command == "dataset-inspect":
            payload = inspect_dataset(args, parse_schema(args))
            exit_code = 0
        elif command == "dataset-validate":
            payload, exit_code = validate_dataset(args, parse_schema(args))
        elif command == "train":
            return run_train(args, parse_schema(args))
        elif command == "validate-images":
            payload, exit_code = run_validate_images(args, parse_schema(args))
        elif command == "validate-sequence":
            if args.event_decisions:
                missing_runner_args = [
                    name
                    for name in ("sequence", "fps", "model", "metadata", "target_class_id", "detector_settings", "settings_snapshot")
                    if getattr(args, name, None)
                ]
                if missing_runner_args:
                    return json_error(command, "CLI_USAGE_ERROR", "Runner-wrapped options cannot be used with --event-decisions.", EXIT_USAGE, {"options": missing_runner_args})
                if not args.ground_truth:
                    return json_error(command, "CLI_USAGE_ERROR", "--ground-truth is required with artifact-driven --event-decisions.", EXIT_USAGE, {})
                payload, exit_code = run_validate_sequence(args, parse_schema(args))
            else:
                missing = [name for name in ("sequence", "fps", "model", "metadata", "target_class_id") if not getattr(args, name, None)]
                if missing:
                    return json_error(command, "CLI_USAGE_ERROR", "Runner-wrapped validate-sequence is missing required options.", EXIT_USAGE, {"missing": missing})
                payload, exit_code = run_validate_sequence_runner(args, parse_schema(args))
        elif command == "metadata-scaffold":
            schema = parse_schema(args)
            payload = {"schema_version": 1, "command": "metadata-scaffold", "status": "ok", "metadata": metadata_scaffold(schema.classes, schema.display_labels)}
            exit_code = 0
        else:
            return json_error(command, "CLI_USAGE_ERROR", "Unknown command.", EXIT_USAGE, {"command": command})
        print_payload(payload)
        return exit_code
    except BaseException as exc:
        return handle_exception(command, exc)
