# Training

Python trainer source is now present in the clean repo under `training/python`.

Accepted target source boundary after migration:

- `training/python/droplet_trainer/`
- `training/python/pyproject.toml`
- `training/python/README-windows-training.md`
- `training/python/requirements/`
- `training/python/scripts/windows/`

CPU validation remains a separate verification step, and packaging remains deferred.

## CPU Setup And Validation Flow

Use a local-only Windows venv at:

`%LOCALAPPDATA%\OpenVisualDropletSorter\training-venv`

Recommended CPU flow:

```powershell
cd training\python
.\scripts\windows\create-training-venv.ps1 -Python py -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv"
.\scripts\windows\install-training-cpu.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -TrainerSourcePath .
.\scripts\windows\verify-training-env.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -Device cpu -CheckOutput "$PWD\..\..\outputs\trainer_env_check"
```

Local dataset validation for this CPU wave uses:

`C:\Users\goals\Codex\CNN for Droplet Sorting\datasets\prepared\droplet_binary_2026-04-30`

This dataset path is local-only validation input. Do not copy it into the clean repo or commit derived data/crops.

Expected local validation commands:

```powershell
cd training\python
.\scripts\windows\inspect-dataset.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -Dataset "C:\Users\goals\Codex\CNN for Droplet Sorting\datasets\prepared\droplet_binary_2026-04-30"
.\scripts\windows\validate-dataset.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -Dataset "C:\Users\goals\Codex\CNN for Droplet Sorting\datasets\prepared\droplet_binary_2026-04-30" -Output "<local-output-path>"
.\scripts\windows\train-model.ps1 -VenvPath "$env:LOCALAPPDATA\OpenVisualDropletSorter\training-venv" -Dataset "C:\Users\goals\Codex\CNN for Droplet Sorting\datasets\prepared\droplet_binary_2026-04-30" -Output "<local-model-output>" -Device cpu
```

## Deferred Follow-Up

- GPU validation is a separate follow-up wave and should use a separate GPU-focused venv.
- Packaging remains deferred from the first clean repo cut.
- Generated trainer outputs such as checkpoints, exports, and datasets remain local-only.

Current runtime models are under `app/runtime/models/`.
