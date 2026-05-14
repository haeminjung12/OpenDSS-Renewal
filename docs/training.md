# Training

Python training/export is deferred from this first clean repo cut.

Reason: before migration, the trainer CLI existed and exposed a real `train` command, but the current Python environment was not ready for reliable train/export. The fixed readiness check reports missing required packages:

- `torchvision`
- `onnx`
- `onnxscript`

The trainer/export source should be added only after the separate Python trainer readiness work is accepted and the root/internal-release trainer source trees are reconciled.

Current runtime models are under `app/runtime/models/`.
