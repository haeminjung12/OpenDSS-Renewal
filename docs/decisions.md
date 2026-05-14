# Decisions

## 2026-05-14 Clean Repo First Cut

- Keep only the active runtime codebase in this repo.
- Keep historical agent work outside the repo in `1. Agent work space - archieve`.
- Preserve runtime-relative paths under `app/runtime/` for the first cut.
- Defer Python trainer source until readiness is accepted.
- Defer packaging execution until after runtime build and no-DAQ smoke.
- Do not fire DAQ output during migration verification.

## 2026-05-14 Model Runtime

- Older three-class model visible label: `Cell aggregate model V1 (2026-05-14)`.
- Target class for the legacy three-class model: `Single`.
- ONNX Runtime DLL must be deployed locally from the selected `ONNXRUNTIME_DIR` and validated by package checks.
