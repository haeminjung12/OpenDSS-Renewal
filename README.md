# OpenDSS

OpenDSS is a Windows desktop application for working with droplet-sorting
datasets, models, and experiments.

## Prerequisites

- A Windows computer.
- The current `OpenDSSSetup.exe` installer from the official project download.

Camera and DAQ hardware setup is separate from basic software installation.

## Download

Download the current Windows installer:

[Download OpenDSSSetup.exe](https://github.com/haeminjung12/OpenDSS-Renewal/releases/download/v0.9.1/OpenDSSSetup.exe)

The accepted installer details are:

- File name: `OpenDSSSetup.exe`
- Size: 1,131,305,601 bytes
- SHA-256:
  `25DFCA689472DBFC0F0905B895684D58FC3364E70345AD0DF28697CA46F6A211`

## Verify the download

After downloading the installer, open PowerShell in the folder containing it
and run:

```powershell
Get-FileHash .\OpenDSSSetup.exe -Algorithm SHA256
```

Confirm that the reported hash exactly matches:

```text
25DFCA689472DBFC0F0905B895684D58FC3364E70345AD0DF28697CA46F6A211
```

Do not install the file if the name or hash differs.

## Install

1. Verify the installer as described above.
2. Open `OpenDSSSetup.exe`.
3. Follow the installer prompts to complete installation.

The accepted installer is unsigned, so Windows may display a security warning.
Review the warning and continue only when the file came from the official
project download and its SHA-256 matches the value above. If either check is
uncertain, cancel the installation and report the problem.

## First launch

Launch OpenDSS after installation. The installer includes starter content, so
you can explore the software without separately downloading datasets or model
weights. Hardware-dependent workflows require separate camera and DAQ setup.

## Bundled content

The installer contains two starter datasets and exactly four accepted local
weights:

- MobileNetV3-Small ImageNet
- MobileNetV3-Small Pretrained
- EfficientNet-B0 ImageNet
- EfficientNet-B0 Pretrained

These datasets and weights are intentionally excluded from the Git repository.

## Basic workflow

1. Launch OpenDSS and begin with a bundled starter dataset.
2. Select an appropriate bundled model weight for the work you are performing.
3. Review the resulting data and model output in the application.
4. Configure camera and DAQ hardware separately before starting a workflow that
   requires physical devices.

## Troubleshooting

- **The hash does not match:** do not run the installer. Download it again from
  the official project location.
- **Windows shows a security warning:** verify the source and exact SHA-256
  above. Cancel if either cannot be confirmed.
- **Hardware is unavailable:** complete the separate camera or DAQ setup before
  using a hardware-dependent workflow.

For installation problems or unexpected behavior, open an
[issue](https://github.com/haeminjung12/OpenDSS-Renewal/issues).

License: [LICENSE](LICENSE) · [Third-party notices](THIRD_PARTY_NOTICES.md)
