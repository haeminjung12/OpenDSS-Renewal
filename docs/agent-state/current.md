# OpenDSS Agent State

State format: `1`

- Policy revision: `ODSS-2026-07-30.4`
- Mode: `debug`
- User-facing Lead: `Debug Lead`
- Checkpoint branch: `codex/debug-lead`
- Checkpoint commit: `eadd305760924acad25a403d4bd2a8dbd8755ccb`
- Active ID: `DBG-014`
- Status: `DBG-014 through DBG-018 implemented, verified, and committed for push; awaiting acceptance`
- Dirty paths at checkpoint: `20 paths enumerated in docs/debug/evidence/DBG-003-012-integrated-gate-20260730.md, plus two intentionally Git-ignored migrated checkpoints`
- Updated: `2026-07-31T03:29:23-05:00`

## Accepted decisions

- The Debug Lead is the sole user-facing agent for OpenDSS debugging.
- Workers and validators report only to the Debug Lead and do not edit canonical state or the bug ledger.
- Normally use no more than two internal workers and one validator concurrently.
- Every accepted bug receives one stable `DBG-*` ID before production changes.
- Fresh-agent handoff is preferred over repeated compaction; this file and verified Git evidence are authoritative.
- Verification uses coherent integration batches, avoids unchanged reruns, and reserves full builds for the integrated batch unless a named early-build risk applies.
- A fresh read-only Plan Guardian checks canonical plan fidelity at integration, closure, merge, and material-scope-change gates.
- `DBG-001` and `DBG-002` form the first integration batch; run the full build and required tests only after both fixes are complete, while allowing narrow reproduction checks needed to resolve named uncertainties.
- Shared policy `G-2026-07-30.3` and delegation rule `G-MODEL-001` are adopted for all remaining work.
- User-authoritative detector invariant: every acquired image must pass through the event detector in order before first/last occurrence, trajectory, crop, or ONNX-routing decisions. Sampled preview delivery is not valid detector input; optional sequence persistence is a separate concern.
- The first batch will use a current-code headless characterization harness, informed by the proven legacy `sequence_headless` pipeline, to measure acquisition, detector, and persistence throughput independently; it must not become a parallel production implementation.
- `DBG-010` and `DBG-011` were discovered by the combined Release gate and are not silently added to the `DBG-001`/`DBG-002` production-fix scope.
- Shared policy `G-2026-07-30.4` and repository policy `ODSS-2026-07-30.4` are adopted at policy commits `6fb552e`, `8291810`, and `eadd305`.
- Repository `.codex/config.toml` preserves `[features]`, `[agents]`, `mcp_servers.qtDocumentation`, and `mcp_servers.qtCreator` as required by `O-QT-TOOLS-001`.
- Workers may investigate `DBG-010` and `DBG-011` independently, but the Debug Lead must present their evidence and options to the user and receive confirmation before choosing or implementing a resolution.
- The user authorized `DBG-011` Option A: freshly audit the current scientifically coherent dataset without overwriting it or restoring the stale historical label.
- The user accepted renewal of the `DBG-011` test-contract hash after the re-audit confirmed that all included image identities and hashes remain stable and `legacy-row-000208` correctly adopts its recorded reviewed label `2`.
- The user authorized `DBG-010` as a one-time migration of every bundled legacy checkpoint to the single current `class_ids` schema, plus correction of all future converter/export paths. Runtime compatibility fallback is rejected.
- For `DBG-010`, the user explicitly waived prediction comparison because weights are not being changed. Verification will instead prove identical model-state tensor contents, unchanged ONNX artifacts, canonical schema readiness, updated package hashes/provenance, and focused/full test results.
- The user accepted `DBG-008` deriving installer version metadata automatically from the v2 application's authoritative version rather than maintaining a second version value.
- The user accepted `DBG-009` showing the provisioner's raw console progress during Training setup so users can see the active operation.
- The user declined the security findings from the 2026-07-31 review and authorized only the simple non-security fixes: bounded Model Test/Training output, Model Library virtualization, legacy post-processing thread cleanup, full-size viewer image-error presentation, and dataset-label role-map caching.
- Camera acquisition behavior is not part of this batch. Existing physical USB HIL already proves every acquired frame reached detector completion at 710.858 fps ROI and 63.309 fps full-frame with zero gaps, duplicates, ordering errors, or DCAM overrun; the zero-interval timer is therefore only a profiling target unless new evidence shows processing harm.
- `DBG-014` through `DBG-018` use the bounded plan in `docs/debug/DBG-014-018-simple-fix-plan.md`; security findings and camera/DCAM/detector changes remain excluded.

## Accepted evidence

- Debug authority and safety rules: `AGENTS.md`.
- Canonical bug records: `docs/debug/bug-ledger.md`.
- Product authority and protected assets: `docs/v2/CONTEXT.md` and the protected-asset section of `AGENTS.md`.

## Verification

- Debug worktree contains the ordered unread-frame acquisition fix at commit `eda6c768795aecdf258ccd34290eac7e703928a2`.
- Branch is `codex/debug-lead`.
- A fresh Codex session loaded `G-2026-07-30.2`, `ODSS-2026-07-30.2`, `Debug Lead`, `DBG-INTAKE`, `G-VERIFY-001`, `O-WORKER-BUILD-001`, `O-GATE-001`, and `$opendss-plan-guardian`; `$opendss-debug-init` reported ready.
- Bug intake evidence was reviewed from `E:\OpenDSS\OpenDSS-problem-summary.md`, `C:\Users\goals\Downloads\DataCapture`, and `C:\Users\goals\Downloads\New folder`.
- Nine stable bugs were triaged as `DBG-001` through `DBG-009`; the first integration batch is awaiting its single full build/test gate.
- `DBG-001` evidence reconciles 1,087 retained TIFF frames and 16 crop records. Approximately 90.5% of raw camera delivery IDs are absent before the dataset writer; its queue rejection and consumer failure counters are zero.
- `DBG-002` is reproduced: Image Sequence queue rejection is logged and counted, but the rejected offer returns success and a nonempty sequence can still persist `status: completed`.
- A fresh read-only Plan Guardian passed the protected camera-boundary investigation. It confirmed `CameraController::acceptFrame` emits `frameReady` before preview coalescing and that Dataset Capture processes each received frame, so the next boundary is how acquisition frames reach `CameraController`.
- `DBG-001` root cause is accepted: a 16 ms `CameraService` timer requests only DCAM's newest frame, so a roughly 662 fps source is sampled at roughly 62.5 fps. The observed 9.45% delivery ratio matches both datasets; the omitted images never reach any detector consumer.
- Vendor HCImageLive evidence establishes 2304×2304, 8-bit, Fast, 1.0 ms exposure at 63.33 fps as the `DBG-002` equivalent-settings attempted-throughput baseline.
- The `DBG-001` headless Release characterization fails before production modification: 259/270 full frames and 257/2,800 ROI frames reached the detector, matching the accepted newest-only polling defect.
- After the fix, Release service-path characterization passes exact acquired/service/detector counts: 270/270/270 at 2304×2304 and 2,800/2,800/2,800 at 1152×288, with zero gaps, ordering errors, pixel-ID errors, or pre-service coalescing.
- Physical USB HIL drained and detector-completed 3,554/3,554 ROI frames at 710.858 fps and 316/316 full frames at 63.309 fps with zero gaps, duplicates, ordering errors, or DCAM overrun. Average full-frame detector service time was 7.804 ms, mathematically supporting 128.140 fps versus the 89.1 fps CoaXPress specification and 100 fps headroom target.
- The isolated `DBG-002` fix passes its focused Release build and regression test; it reports queue rejection immediately and finalizes the affected sequence as Failed with factual attempted/saved/rejected counts and rates.
- `DBG-002` is integrated at commit `c17e72e393116f05430644c293450cba1921ea74`.
- Integration build repairs are committed at `ebf1a0e3a59058bb8baa5f990f2a93a981760ffd` (preserve the camera CLI and DAQ test seams) and `94067b759e631e4e965ba433ad9cf8e497dec9d0` (propagate the NI-DAQmx runtime link).
- The previously started combined build was stopped at the user's request before storage consolidation; no completed full-build or full-test result is accepted from that attempt.
- A fresh combined Plan Guardian returned `PASS` against `eda6c76`, `c17e72e`, `ebf1a0e`, and `94067b7`.
- A fresh post-gate Plan Guardian returned `PASS` for the test-only repairs and separation of `DBG-010`/`DBG-011`.
- The integrated Release build at `C:\b\odss-debug-lead-hil` passed, including `OpenDSS.exe`, camera/DCAM, sequence, detector, ONNX, and DAQ-linked targets.
- The first full CTest run passed 44/53 tests and exposed nine failures unrelated to the `DBG-001`/`DBG-002` commits. Narrow test-contract repairs and restoration of ignored model assets from exact locked hashes resolved seven failures.
- The final full CTest run passed 51/53 tests in 128.39 seconds. All camera, detector, crop, dataset capture, sequence capture/reporting, Live, Sequence, DAQ, ONNX conversion, Model Library, Training, and repaired Results tests passed.
- Qualified ignored assets were restored locally with exact metadata hashes: pretrained EfficientNet checkpoint `242ED863...1C5`, pretrained MobileNet checkpoint `0542C495...E35`, ImageNet EfficientNet weights `7F5810BC...4934`, and ImageNet MobileNet weights `047DCFF4...F4E1F`.
- The preserved Python 3.12.10 runtime and GPU training environment were restored as ordinary canonical directories at `%LOCALAPPDATA%\OpenDSS\python-3.12.10` and `%LOCALAPPDATA%\OpenDSS\training-venv-gpu`; both interpreters report Python 3.12.10 and no validation junction remains.
- `DBG-011` re-audit accepted current `dataset.json` SHA-256 `58500D72...9233B`. Dataset identity, schema, 3,620 included record/image hashes, and 5 rejected items remain stable; only `legacy-row-000208` changes from stale label `1` to its already-recorded reviewed label `2`. Evidence: `docs/debug/evidence/DBG-011-dataset-reaudit-20260730.md`.
- `DBG-012` was reproduced after the renewed dataset hash advanced the representative test: the probe incorrectly depended on the user's mutable Active Model selection even though its qualified model remained present. The test-only fix selects the pinned entry by stable ID and activates it only in the disposable registry.
- The full Release build completed after the Python directories were restored. Full CTest then passed 52/53 in 132.38 seconds: `DBG-011` and `DBG-012` pass, and the sole remaining failure is the pre-existing `DBG-010` readiness contract.
- Restoring the preserved canonical Python/runtime directories did not resolve `DBG-010`; this excludes the removed junctions and backup-directory names as its root cause. No Python source was changed.
- `DBG-010` migration is implemented for both bundled checkpoints. Only canonical `class_ids=["0","1","2"]` was added; all 244 MobileNet and 360 EfficientNet model-state tensors retain identical content hashes, ONNX assets are unchanged, strict backend loading passes for both, and qualified converter/backend focused tests pass 2/2 each. Original artifacts are recoverable under `C:\b\odss-debug-lead-hil\artifact-backups\DBG-010`.
- The permanent installer/runtime path repair is implemented across provisioning, payload/preflight, v2 runtime discovery, installer verification, build scripts, and CTest discovery. Auto selects CUDA only for reported CUDA 13-compatible hardware, otherwise CPU; both canonical environments are supported without junctions, a validated selection marker is atomically published/restored, alternate valid environments are preserved, and unowned global Python is never modified.
- Installer-path focused evidence passes: five PowerShell parsers, zero-error provisional installer preflight, Inno Setup compile, focused Release `desktop_app` build, and scoped diff check. Online CPU/CUDA provisioning and installer failure-injection HIL remain acceptance risks.
- The fresh integration Plan Guardian returned `PASS`. The final integrated Release build passed, deployed both migrated checkpoint hashes, and the final full CTest run passed 53/53 in 153.06 seconds. Evidence: `docs/debug/evidence/DBG-003-012-integrated-gate-20260730.md`.
- `DBG-008` and `DBG-009` were reproduced from independent version literals and a hidden provisioner console. The fresh Plan Guardian passed their fixes; focused preflight has zero errors, and Inno Setup compiled the updated installer successfully.
- The compiled installer reports ProductVersion `2.0` and SHA-256 `72A6112E...F3912`. Its packaged provisioner hash exactly matches source at `EAE5F021...80F6`. Evidence: `docs/debug/evidence/DBG-008-009-installer-version-progress-20260730.md`.
- `DBG-013` intake initially found subsystem `2` on the available Release executable, but its cache later proved it was the legacy GUI, not v2. The actual v2 target lacked an explicit GUI classification; the corrected v2 build and package now prove subsystem `2`. The machine's stale Start Menu shortcut still targets a deleted temporary installer-smoke directory and must be replaced by installer HIL.
- The first DBG-008/009 compile fixture was invalidated after its CMake cache proved it contained the legacy GUI rather than v2, and an independent validator found a malformed provisioner argument boundary. It is explicitly superseded.
- The corrected actual-v2 Release build at `C:\b\d13` passes with `BUILD_QT_GUI=OFF`, `BUILD_QT_GUI_V2=ON`, executable SHA-256 `1C0463C8...B8EB`, and PE subsystem `2`.
- The guarded portable package preserves that exact executable identity and passes its GUI-subsystem check. The corrected installer reports ProductVersion `2.0`, SHA-256 `342C401E...455E`, and complete preflight passes with zero errors/remaining inputs. Evidence: `docs/debug/evidence/DBG-008-009-013-final-installer-20260730.md`.
- `DBG-014` through `DBG-018` were reproduced before production edits, implemented within the authorized non-security scope, and passed focused Release/service/QML checks.
- A fresh corrected-path Plan Guardian returned `PASS` for the `DBG-014` through `DBG-018` diff; the earlier truncated-path invocation was invalid and is not accepted evidence.
- The final non-overlapped integrated gates pass: `C:\b\odss-debug-lead-hil` Release plus 53/53 CTest in 150.67 seconds, and `C:\b\d13` actual-v2 Release plus 2/2 CTest in 4.10 seconds.
- Parallel gate attempts exposed only resource contention: `model_test_controller_test` passed directly, isolated, and in the final full suite; the v2 tracking-file lock cleared when the build ran alone; an unrelated intermittent label QML assertion passed its required rerun and final full gate.
- Evidence: `docs/debug/evidence/DBG-014-018-simple-fix-gate-20260731.md`.

## Blockers

- Live droplet recall and GUI/persistence throughput remain unverified; the short HIL scene contained no detected droplets. CoaXPress capacity is a user-authorized mathematical check, not a physical CoaXPress HIL run.
- Online CPU/CUDA provisioning, hardware Auto selection, installer rollback failure injection, and rebuilt GUI stage reporting remain installer HIL risks.
- `DBG-010`, `DBG-011`, and `DBG-012` pass the integrated Release/53-test gate and are not blockers.

## Exact next action

Review the verified and pushed `DBG-014` through `DBG-018` batch for acceptance. Do not change camera behavior or implement declined security findings.
