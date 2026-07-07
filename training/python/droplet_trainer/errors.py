from __future__ import annotations

import json
import sys
from dataclasses import dataclass, field
from typing import Any


EXIT_SUCCESS = 0
EXIT_USAGE = 2
EXIT_SEQUENCE_RUNNER_FAILED = 5
EXIT_SEQUENCE_ARTIFACT_MISSING = 6
EXIT_UNSUPPORTED_PYTHON = 10
EXIT_MISSING_PACKAGE = 11
EXIT_DEVICE_UNAVAILABLE = 13
EXIT_DATASET_MISSING = 20
EXIT_MANIFEST_INVALID = 21
EXIT_SCHEMA_MISMATCH = 22
EXIT_SPLIT_INVALID = 23
EXIT_OUTPUT_INVALID = 30
EXIT_TRAINING_FAILED = 40
EXIT_ONNX_EXPORT_FAILED = 41
EXIT_ARTIFACT_VALIDATION_FAILED = 42
EXIT_IMAGE_VALIDATION_FAILED = 50
EXIT_INTERNAL = 70


@dataclass
class CliError(Exception):
    code: str
    message: str
    exit_code: int
    details: dict[str, Any] = field(default_factory=dict)
    command: str = "unknown"


def json_error(command: str, code: str, message: str, exit_code: int, details: dict[str, Any] | None = None) -> int:
    payload = {
        "schema_version": 1,
        "command": command,
        "status": "error",
        "error": {
            "code": code,
            "message": message,
            "details": details or {},
        },
    }
    print(json.dumps(payload, indent=2, sort_keys=False))
    return exit_code


def handle_exception(command: str, exc: BaseException) -> int:
    if isinstance(exc, CliError):
        return json_error(exc.command or command, exc.code, exc.message, exc.exit_code, exc.details)
    print(
        json.dumps(
            {
                "schema_version": 1,
                "command": command,
                "status": "error",
                "error": {
                    "code": "INTERNAL_BACKEND_ERROR",
                    "message": str(exc),
                    "details": {"exception_type": exc.__class__.__name__},
                },
            },
            indent=2,
        ),
        file=sys.stdout,
    )
    return EXIT_INTERNAL
