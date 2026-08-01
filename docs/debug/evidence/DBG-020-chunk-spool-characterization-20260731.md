# DBG-020 bounded 8-bit chunk-spool characterization — 2026-07-31

## Scope and decision

The user selected 8-bit persistence output regardless of camera input depth and requested an actual-camera test of bounded in-memory chunks appended to one sequential `.partial` spool, followed after Stop by individual TIFF finalization. This characterization is test-only. Production camera, DCAM, detector, sequence persistence, ONNX, training/export, and application UI files are unchanged.

The test uses the real `DcamCameraDevice`, current `CameraService`, the qualified detector on an independent ordered worker, a bounded four-buffer spool pool, and 8-bit persistence conversion. A chunk is queued at 32 MiB or 50 ms, whichever occurs first. The writer appends self-describing frame records to one spool file. After capture and detector drain, optional finalization streams one record at a time through the existing no-probe `QImageWriter` temporary-file, flush, and no-replace publication mechanics, then independently reads every TIFF.

Runner: `app/runtime/tests/run_dbg020_chunk_spool_hil.ps1`.

## Real-camera observations

| Metric | 144×144 input 8 / output 8 | 2304×2304 input 16 / output 8 | 2304×2304 input 8 / output 8 |
|---|---:|---:|---:|
| Acquired | 2,951 | 94 | 189 |
| Acquisition rate | 992.454 fps | 31.601 fps | 63.309 fps |
| Detector completed | 2,951 | 94 | 189 |
| Source gaps / duplicates / acquisition order faults | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 |
| Detector order faults | 0 | 0 | 0 |
| Spool persisted | 2,951 | 94 | 189 |
| Spool rejected / failed | 0 / 0 | 0 / 0 | 0 / 0 |
| Sequential spool rate | 20.269 MB/s | 154.745 MB/s | 327.918 MB/s |
| Peak buffered bytes | 1,142,240 | 10,616,896 | 53,084,480 |
| Chunk pool high-water / capacity | 2 / 4 | 2 / 4 | 2 / 4 |
| Chunk queue high-water | 0 | 0 | 1 |
| Mean / max chunk write | 0.741 / 2.472 ms | 3.226 / 5.817 ms | 16.259 / 37.311 ms |
| Finalized TIFFs | 2,951 | 94 | 189 |
| Independently readable TIFFs | 2,951 | 94 | 189 |
| TIFF writer mean service | 2.500 ms | 6.898 ms | 6.231 ms |
| Full finalization plus diagnostic reread | 52.014 s | 3.246 s | 6.162 s |
| Result | PASS | PASS | PASS |

The failed user recording `Sequence-2026-07-31_15-22-04` reported 4,016 persistence attempts at 1,518.38 nominal fps, 778 saves at 294.15 fps, and 3,238 queue rejections. Its file timestamps and the new physical HIL indicate that the manifest rate was not a reliable physical average; the current actual 144×144 source measured 992.454 fps. This remains more than three times the prior separate-TIFF save rate and is therefore a valid high-rate stress case. The chunk spool retained every acquired frame without rejection.

The finalization wall time includes an independent full TIFF read after every write. That reread is diagnostic-only and is not proposed for production. The writer's own mean service measurements correspond to about 400 fps for 144×144 and 145–160 fps for the two full-frame cases, so post-stop production finalization should be materially shorter than the full diagnostic duration while remaining slower than high-rate ROI acquisition.

## Conclusions

1. A bounded, sequential 8-bit spool removes the measured per-file persistence bottleneck for the available real 144×144 Fast stream and both available full-frame USB streams.
2. Detector delivery remains independent and exact in all three runs: acquired count equals detector-completion count with zero ordering faults.
3. The design does not require buffering a complete recording. The four-buffer pool peaked at two buffers; measured peak memory remained 1.14–53.08 MB.
4. Post-stop creation of ordinary TIFF files is feasible and lossless, but high-rate ROI capture requires a visible Finalizing phase because individual TIFF creation remains slower than acquisition.
5. Camera input may be 8 or 16 bits, but the accepted persistence output is always 8-bit. Production manifests must distinguish camera input settings from persisted image depth truthfully.

## Remaining qualification risks

- These were bounded three-second runs. `QFile::flush()` submits data to the operating-system cache but does not prove durable media throughput. A longer sustained full-frame gate is required before claiming support for a faster transport or a slowest-supported computer/storage tier.
- The available USB camera produced 63.309 fps full-frame 8-bit and 31.601 fps full-frame 16-bit input. No physical 89.1 fps CoaXPress/GigE run was available.
- Production implementation needs bounded backpressure, Finalizing UI/progress, atomic final manifest publication, and spool cleanup/failure tests. On 2026-07-31 the user explicitly declined per-frame spool checksums and crash/failure resume behavior as unnecessary complexity; a failed or interrupted finalization may remain failed.
- A disk that cannot sustain the 8-bit stream indefinitely cannot be repaired by increasing RAM. At 2304×2304 and 89.1 fps, 8-bit payload remains about 473 MB/s.

## Durable artifacts

- 144×144 input-8/output-8 report: `C:\b\odss-debug-lead-hil\dbg020-chunk-spool-20260731\roi-144-input8-output8\report.json`, SHA-256 `F8C1832923188CF31E07C417414300EBB652ED7AB1C296E2EC94040998B545DC`.
- 144×144 spool: `...\roi-144-input8-output8\DBG-020-20260731_155717.partial`, 61,286,368 bytes, SHA-256 `612E923D02CFC692B5AC753F0086B6F11C858E53197227A273E730386FB788E6`.
- 2304×2304 input-16/output-8 report: `C:\b\odss-debug-lead-hil\dbg020-chunk-spool-20260731\full-2304-input16-output8\report.json`, SHA-256 `5716F1F5EB602AFE2E00DC5187E2B73B01E657EBB91E8B8D31CE49121349351A`.
- 2304×2304 input-16/output-8 spool: `...\full-2304-input16-output8\DBG-020-20260731_155835.partial`, 498,994,112 bytes, SHA-256 `B84848F97D00677587AAD34E2781747DCD41CDEC1DFB4BCBABF90859B9FC0049`.
- 2304×2304 input-8/output-8 report: `C:\b\odss-debug-lead-hil\dbg020-chunk-spool-20260731\full-2304-input8-output8\report.json`, SHA-256 `9CD86EC46BCAB492C681B2BB3E381A9397EC162D8A9FA4572F16521762B63729`.
- 2304×2304 input-8/output-8 spool: `...\full-2304-input8-output8\DBG-020-20260731_155903.partial`, 1,003,296,672 bytes, SHA-256 `6A9C1ACE12CFDD788F4EA26632AA4DE9CE1363107A1700F9FF8BB29FE7C887F8`.

## Smallest evidence-backed production plan — confirmation required

1. Add a bounded byte/time chunk pool to Image Sequence persistence only; keep camera acquisition and the ordered detector path unchanged.
2. Append self-describing 8-bit frame records to one sequence-local `.partial` spool. Use 32 MiB or 50 ms thresholds and a small fixed pool, then tune only from sustained evidence. Per-frame checksums are explicitly out of scope by user decision.
3. On Stop, transition to visible `Finalizing`, stream the spool into the existing individual-TIFF publication path, and publish `sequence.json` only after every expected TIFF succeeds.
4. Preserve truthful failure: retain the spool when finalization fails and never report complete when frame, spool, or TIFF counts differ. Automatic crash/failure resume is explicitly out of scope by user decision.
5. Record acquisition, detector completion, spool persistence, and finalized TIFF counts independently. Persisted image depth is 8-bit even when camera input is 16-bit.
6. Before integration, add focused count/order, failure-retention, cleanup, and manifest tests; run real-camera high-rate and full-frame gates plus the required Plan Guardian/full Release gates.

The user subsequently authorized this bounded production design as the final fix, and explicitly narrowed it to exclude per-frame checksums and crash/failure resume behavior.

## Production implementation and final verification

The production Image Sequence path now appends bounded 8-bit records to one sequential `sequence.frames.partial` spool during acquisition, then creates the ordinary individual TIFF files after Stop while the UI shows Finalizing progress. Camera/DCAM acquisition, ordered detector delivery, ONNX, training/export, and unrelated persistence behavior remain unchanged.

Production-service actual-camera HIL passed with exact acquired = detector-completed = persistence-accepted = saved counts and zero gaps, duplicates, ordering faults, queue rejections, or consumer failures:

- 44×144 input8/output8: 4,240 frames at 1,416.814 fps; persistence handoff high-water 3/16.
- 2304×2304 input16/output8: 94 frames at 31.601 fps.
- 2304×2304 input8/output8: 189 frames at 63.309 fps.

The renewed Plan Guardian returned `PASS`. The primary Release build and CTest passed 53/53; the actual-v2 Release build and CTest passed 2/2, including 56/56 QML checks.

Final artifacts:

- Accepted executable: `C:\b\d13\Desktop_app_v2\Release\Desktop_app_v2App.exe`, SHA-256 `3A5BFA2913E0223B2C02F053779D4A45829864BD0ED74916BBA2B899A959363E`.
- Guarded standalone directory: `C:\b\d13\manual-test-20260731\dbg020-final-portable\OpenDSS_20260731_164312`.
- Final ZIP: `C:\b\d13\manual-test-20260731\OpenDSS_Standalone_2.0_DBG-020-FINAL.zip`, 1,236,506,451 bytes, SHA-256 `64BF5037E97BEB0DCBBB95696B9A98C373E839E003732AE0FBB7E0926F0486D8`.
- Streamed archive verification found 8,848 entries and confirmed that `OpenDSS.exe` exactly matches the accepted executable. The archive also contains `HOW-TO-REPLACE-EXISTING-OPENDSS.md`, `diagnostics/run-every-frame-camera-test.ps1`, and its actual-camera harness executable.
