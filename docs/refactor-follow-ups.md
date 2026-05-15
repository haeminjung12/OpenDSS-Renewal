# Refactor Follow-ups

Date: 2026-05-15

## Later

### CI dependency hardening

The current CI workflow is hardware-free because it configures with `ENABLE_NIDAQMX=OFF` and only runs the no-hardware
CTest target. It is not SDK-free: the accepted desktop target still links against DCAM, ONNX Runtime, OpenCV, and Qt.

Later work should decide whether to add a smaller SDK-free CI target, provide runner setup scripts, or split hardware
adapter linkage further.

### Deferred project scope

These remain outside the completed refactor:

- Python trainer/exporter migration.
- Packaging, installer, and release bundle work.
- Manual interactive app test pass.
- Sequence-summary CSV coverage from the current sequence verifier path.

## Formatting

`.clang-format` is now present. Source-wide formatting was not applied in this session because `clang-format` was not
available on `PATH` or in the common LLVM/Visual Studio install locations checked locally. Run formatting as its own
commit once the formatter is installed.
