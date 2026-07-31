# DBG-003–DBG-012 Integrated Gate

Date: 2026-07-30

## Result

- Fresh Plan Guardian: `PASS`
- Release build: `PASS`
- Full CTest: `53/53 PASS` in 153.06 seconds
- Build root: `C:\b\odss-debug-lead-hil`
- Build log: `C:\b\odss-debug-lead-hil\final-integrated-build.log`
- Final CTest log: `C:\b\odss-debug-lead-hil\final-integrated-ctest-rerun.log`

## Migrated packaged checkpoints

- MobileNet checkpoint: `4A8F704BF77A51900A4EFCD9EA2FAFD54D91D51F08DEE684DB0437DA38D953EA`
- EfficientNet checkpoint: `7496BB0D2959E456ED4BB9AE77B03FD187FE5D75E0A86E40B5A18296DA7B4135`
- The deployed Release copies match those hashes.
- Original checkpoint and metadata files are recoverable under `C:\b\odss-debug-lead-hil\artifact-backups\DBG-010`.

## Checkpoint verification

- Both migrated checkpoints add only top-level `class_ids=["0","1","2"]`.
- MobileNet: 244 model-state tensors retain content hash `6A8837FA06A7453F0B031BC279418E5942C07970938D7333985E3C15AA7C2E2E`.
- EfficientNet: 360 model-state tensors retain content hash `E04D1B4BF249593CB83D0048D12CDEB4936924EAEDEF74DA137ABFE7FC88544E`.
- ONNX artifacts and Python backend source are unchanged.
- Per explicit user direction, prediction comparison was not run.

## Installer/path verification

- Five changed PowerShell scripts parse.
- Installer preflight reports zero errors; only external wheel, executable, and staged-package inputs remain provisional.
- Inno Setup compilation passes.
- Focused Release `desktop_app` build passes.
- CPU lock SHA-256: `02CC27647524B693F2F53D13331C00943BA22444093D89D8E23B16A0D428A0CC`
- CPU inventory SHA-256: `D76236DA4724079D83329F62E5A24745582C2559B007B693B0DBB6E081675B09`

## Dirty paths at checkpoint

- `app/runtime/Desktop_app_v2/App/main.cpp`
- `app/runtime/installer/build_installer.ps1`
- `app/runtime/installer/installer.iss`
- `app/runtime/installer/preflight_training_installer.ps1`
- `app/runtime/models/templates/pretrained/efficientnet_b0/metadata.json`
- `app/runtime/models/templates/pretrained/mobilenet_v3_small/metadata.json`
- `app/runtime/scripts/check_package.ps1`
- `app/runtime/scripts/convert_pytorch_model_package.py`
- `app/runtime/scripts/package_portable.ps1`
- `app/runtime/tests/CMakeLists.txt`
- `app/runtime/tests/model_test_controller_test.cpp`
- `app/runtime/tests/model_test_production_model_probe.cpp`
- `app/runtime/tests/test_convert_pytorch_model_package.py`
- `docs/agent-state/current.md`
- `docs/debug/bug-ledger.md`
- `docs/debug/evidence/DBG-011-dataset-reaudit-20260730.md`
- `docs/debug/evidence/DBG-003-012-integrated-gate-20260730.md`
- `training/python/requirements/windows-py312-cpu-inventory.json`
- `training/python/requirements/windows-py312-cpu.lock`
- `training/python/scripts/windows/provision-training-runtime.ps1`

The two migrated `checkpoint.pth` files are intentionally Git-ignored local release assets and therefore do not appear in ordinary Git status.

## Remaining risk

- Online CPU and CUDA provisioning have not been executed from a rebuilt installer.
- Real `nvidia-smi` Auto-selection and installer rollback failure injection remain HIL work.
- The rebuilt installer has not yet been observed for live per-stage status updates.
- Live droplet recall and GUI/persistence throughput remain separate hardware acceptance risks.
