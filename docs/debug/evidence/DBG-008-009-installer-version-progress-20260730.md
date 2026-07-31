# DBG-008 / DBG-009 installer version and progress evidence

Date: 2026-07-30

> Superseded: this first compile fixture used a legacy GUI build
> (`BUILD_QT_GUI=ON`, `BUILD_QT_GUI_V2=OFF`) and contained a malformed
> provisioner argument boundary. It is not accepted for installation or HIL.
> The corrected real-v2 evidence is
> `docs/debug/evidence/DBG-008-009-013-final-installer-20260730.md`.

## Accepted scope

- `DBG-008`: derive installer metadata from the v2 application's authoritative version so releases do not require a second manual version edit.
- `DBG-009`: expose raw Training provisioner progress in a visible console window.

## Reproduction

- `Desktop_app_v2/App/main.cpp` reported version `2.0`.
- `installer.iss` independently hard-coded version `0.9.0`.
- `RunTrainingProvisioner` launched Windows PowerShell with `SW_HIDE`, hiding all provisioner and package output behind one static installer message.

## Fix

- `build_installer.ps1` extracts the one numeric value passed to `QCoreApplication::setApplicationVersion` in the v2 application and passes it to Inno Setup as `AppVersion`.
- `installer.iss` no longer has a version fallback; compilation fails unless the build supplies the application-derived value.
- The Training provisioner launches with `SW_SHOW`, titles its console `OpenDSS Training Setup`, and reports explicit dependency-download, environment-creation, dependency-installation, verification, and publication milestones.
- The installer tells the user to follow that console for live progress.

## Verification

- PowerShell parser: pass for `build_installer.ps1`, `preflight_training_installer.ps1`, and `provision-training-runtime.ps1`.
- Focused installer preflight: lifecycle/version/progress contract passes with zero errors. External package inputs remain provisional.
- Fresh Plan Guardian: `PASS`.
- Inno Setup 6.7.0: successful compile.
- Compiled installer: `C:\b\odss-debug-lead-hil\DBG-008-009-installer-final\OpenDSSSetup.exe`.
- Compiled installer ProductVersion: `2.0`.
- Compiled installer SHA-256: `72A6112E42D4D820F61137AFF42BFE2DC86A6207F4D4E559F7680D9C13FF3912`.
- Source and compiled-payload provisioner SHA-256 both equal `EAE5F0218FA38F11DCF4057EA0FD311B522D946E09BD6677B975E0BFE29180F6`.
- Preflight evidence: `C:\b\odss-debug-lead-hil\DBG-008-009-preflight.json`.
- Compile log: `C:\b\odss-debug-lead-hil\DBG-008-009-inno-final.log`.
- C++ compilation and CTest were not repeated because this batch changes only installer/provisioning scripts; the preceding integrated Release gate remains 53/53.

## Remaining risk and rollback

- `DBG-008` metadata behavior is verified in the compiled executable.
- `DBG-009` still requires a real installer run to confirm the console opens visibly, remains usable throughout the long provisioning operation, and closes appropriately on completion/failure.
- Rollback is reverting the four installer/provisioner source changes. It does not modify qualified trainer, model, detector, camera, ONNX, or persistence behavior.
