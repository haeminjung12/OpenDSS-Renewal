# DBG-019 Image Sequence persistence characterization — 2026-07-31

## Scope and authority

- Bug: `DBG-019`.
- Production camera, DCAM, detector, ONNX, training/export, and persistence files remained unchanged during characterization.
- The test-only executable uses the real `DcamCameraDevice`, current `CameraService` on the same dedicated camera-thread boundary as the v2 application, the qualified fast detector on an independent ordered worker, and current `ImageSequenceCaptureService`.
- Image Sequence persistence remains independent of detector decisions. Every acquired frame entered the detector worker in order; persistence rejection did not remove detector input.
- Save root: `C:\Users\goals\OneDrive\Documents\OpenDropletSortingSuite\datasets`.
- Camera profile for every accepted run: 2304×2304, Mono8, Fast, 1.000897 ms readback exposure. The prior 2304×2304 Mono16 profile was restored after each run.

## Method

Four bounded three-second real-camera modes used the existing `ImageSequenceCaptureService::FrameWriter` diagnostic seam:

1. `exact`: the production write sequence with per-stage timing—temporary file creation, `QImageWriter` TIFF encode/write, flush, no-replace `MOVEFILE_WRITE_THROUGH` publication, and `QImageReader::canRead()`.
2. `split`: actual camera frames encoded to TIFF in memory before separately timing temporary-file write/flush, write-through publication, and the same readability probe. This mode identifies boundaries but is not a production-equivalent throughput result.
3. `no-probe`: the exact write sequence with only the post-publication readability probe omitted. This is an isolation experiment, not a production change.
4. `direct`: a test-only classic little-endian uncompressed Mono8 TIFF serializer writing the actual camera image bytes to the same temporary-file, flush, and no-replace write-through publication path, without the hot-path readability probe. Camera acquisition, the ordered detector worker, `CameraFrame`-to-`QImage` conversion, the bounded persistence queue, and its image copy remained unchanged. This mode compares only the TIFF writer implementation and is not a production change.

The first single-thread harness attempt is excluded from conclusions because it did not reproduce the production camera-thread boundary and caused a DCAM ring overrun. Two later serial-detector harness attempts are also excluded from final counts because detector work throttled persistence offers instead of measuring the paths independently. Their artifacts are retained only as failed diagnostic evidence.

## Observations

| Metric | Exact | Split | No probe | Direct Mono8 |
|---|---:|---:|---:|---:|
| Acquired frames | 189 | 189 | 189 | 189 |
| Physical acquisition rate | 63.309 fps | 63.309 fps | 63.309 fps | 63.309 fps |
| Detector completions | 189 | 189 | 189 | 189 |
| Detector completion rate | 63.223 fps | 63.368 fps | 63.352 fps | 63.404 fps |
| Source gaps / duplicates / out of order | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 |
| Persistence consumer failures | 0 | 0 | 0 | 0 |
| Saved frames | 155 | 124 | 189 | 189 |
| Queue rejections | 34 | 65 | 0 | 0 |
| Save-queue high-water / capacity | 16 / 16 | 16 / 16 | 9 / 16 | 6 / 16 |
| Saved frames per source-time second | 52.196 fps | 41.757 fps | 63.646 fps | 63.646 fps |
| Mean serialized writer service | 20.093 ms | 24.550 ms | 8.646 ms | 6.090 ms |
| Mean writer service capacity | 49.77 fps | 40.73 fps | 115.66 fps | 164.19 fps |

The current physical full-frame source remained at the previously proven USB rate of 63.309 fps; it did not reproduce the earlier manifest-reported 84.72 fps. This does not revise the earlier factual manifest. It makes the new result conservative: the exact persistence path loses frames even at the lower physical rate.

### Writer-stage timing

| Stage | Exact mean | Split mean | No-probe mean |
|---|---:|---:|---:|
| Temporary-file open | 0.829 ms | 0.854 ms | 0.623 ms |
| TIFF encode plus direct temp write | 7.022 ms | — | 6.591 ms |
| TIFF memory encode | — | 9.219 ms | — |
| Encoded temp-file write | — | 2.669 ms | — |
| Flush | 0.0003 ms | 0.0010 ms | 0.0002 ms |
| Write-through publication | 1.392 ms | 1.107 ms | 1.124 ms |
| Post-publication readability probe | 10.545 ms | 10.397 ms | omitted |
| Total | 20.093 ms | 24.550 ms | 8.646 ms |

Direct Mono8 stage timing was 0.738 ms temporary-file open, 3.502 ms direct header/pixel write, 0.0011 ms flush, and 1.484 ms write-through publication, for 6.090 ms total mean service time. Its p95 total was 13.586 ms.

In the exact path, the readability probe is 52.5% of mean serialized writer time and is the largest measured stage. TIFF encode/direct write is the next largest stage. The split result shows that temporary-file writing and write-through publication are materially smaller than encoding and the readability probe.

The no-probe run retained the target-exists guard, `QImageWriter` success/error check, flush, and no-replace write-through publication. It saved 189/189 with zero rejections. After capture, Pillow independently opened and verified every saved file: 189/189 readable, all 2304×2304 grayscale, zero failures.

The direct Mono8 writer also saved 189/189 with zero rejections while acquisition and ordered detector completion remained 189/189. Its mean end-to-end writer time was 6.090 ms, 2.556 ms (29.6%) below the no-probe `QImageWriter` path, and its queue high-water was 6/16 rather than 9/16. Pillow independently fully decoded all 189 output files as 2304×2304 8-bit grayscale TIFFs with zero failures. Each direct TIFF was 5,308,562 bytes; the comparable `QImageWriter` TIFF was 5,308,630 bytes. The direct result therefore proves additional writer overhead exists, but it does not show that replacing `QImageWriter` is necessary to clear the reported 84.72 fps rate: the smaller no-probe path already measured 115.66 fps mean capacity.

## Hypotheses retained but not accepted as root cause

- OneDrive filtering, filesystem cache state, or background disk activity can contribute variance and long-tail latency. They do not explain the dominant repeatable 10.4–10.5 ms readability-probe stage.
- Increasing the 16-frame queue would delay rejection but would not repair a writer whose exact mean service capacity is below the measured source rate.
- A second persistence worker could raise throughput but would materially change ordering/concurrency and is not justified while the smaller hot-path probe removal already clears the measured deficit.

## Accepted conclusion

The sustained loss is caused by serialized per-frame persistence service time, not by queue scheduling or queue capacity. The largest removable boundary is the post-publication `QImageReader::canRead()` probe. Together with TIFF encode/write it raises exact mean service time to 20.093 ms (49.77 fps), below the measured 63.309 fps source. Removing only that probe lowers mean service time to 8.646 ms (115.66 fps), keeps p95 at 10.612 ms, eliminates queue rejection, and leaves all resulting TIFFs readable under independent post-capture verification.

TIFF writer implementation overhead remains material but is not the current blocker after removal of the redundant hot-path readback. The direct Mono8 serializer is faster than `QImageWriter`, but adopting it would be a larger protected persistence-format change. Direct temporary-file writing and write-through publication are not the primary bottleneck.

Detector delivery is independently clean in every accepted run: acquired count equals detector-completion count, with zero gaps, duplicates, or ordering errors. Persistence rejection remains a separate concern.

## Smallest evidence-backed fix plan — user confirmation required

1. Change only `app/runtime/v2/sequence/image_sequence_capture_service.cpp`: remove the per-frame post-publication `QImageReader::canRead()` probe and its now-unused include.
2. Preserve without alteration: target-exists rejection, TIFF `QImageWriter` success/error handling, temporary-file flush, no-replace `MOVEFILE_WRITE_THROUGH` publication, queue capacity/order, failure accounting, manifests, camera/DCAM, detector, ONNX, and unrelated persistence paths.
3. Retain the existing focused service regression that independently reads the completed TIFF and proves valid pixels. Keep existing no-replacement, write-failure, queue-rejection, failed-manifest, and consumer-failure contracts unchanged.
4. Add a production-default mode to the test-only HIL executable, then run one bounded full-frame real-camera verification. Acceptance: acquired = detector completed = saved, zero source gaps/duplicates/order errors, zero queue rejections/consumer failures, and the prior camera profile restored.
5. Re-run the same user Image Sequence workflow that produced `Sequence-2026-07-31_10-44-52`. If the actual camera again reaches the reported 84.72 fps condition, require zero persistence loss there. The diagnostic no-probe result provides 115.66 fps mean service capacity and 10.612 ms p95 service time, but it does not substitute for a reproducible higher-rate actual-camera run.
6. Use the required fresh read-only Plan Guardian before integration. Then run the focused Release service test and one coherent final Release/CTest gate. Rollback is the one production-line probe restoration plus removal of any test-only production-default harness adjustment.

The direct Mono8 serializer is retained only as measured fallback evidence. It should not enter the smallest first fix because that would replace a general library writer with a custom format implementation when removal of the redundant probe already provides measured mean headroom above 84.72 fps. Reconsider it only if the confirmed no-probe production path fails the reproducible higher-rate camera workflow.

After this initial plan was recorded, the user required the decision to account for the actual faster camera transport and slower supported computers. The current 63.309 fps USB result therefore remains characterization, not final acceptance. Prior repository evidence calls the faster target CoaXPress at 89.1 fps, while the user described GigE; the exact hardware transport and maximum full-frame rate must be resolved for HIL. Uncompressed 2304×2304 Mono8 carries 5,308,416 pixel bytes per frame, requiring about 449.7 MB/s at 84.72 fps, 473.0 MB/s at 89.1 fps, or 530.8 MB/s at 100 fps before small TIFF overhead. Neither probe removal nor the direct serializer reduces that storage bandwidth, so the slowest supported machine/storage tier must demonstrate sustained zero-loss performance before either fix is accepted.

## Provisional 16-bit, 89.1 fps follow-up

The user supplied a readout-speed screenshot showing full-frame 2304×2304 Fast scan at 11.22 ms (89.1 fps with CoaXPress or 31.6 fps with USB 3.0) and stated that this is probably the 16-bit target. The crop itself does not display the bit-depth heading, so 16-bit remains provisional until the full specification or user confirmation is recorded.

Current production `convertCameraFrame()` does not preserve Mono16 for Image Sequence persistence: after constructing `QImage::Format_Grayscale16`, it explicitly returns `owned.convertToFormat(QImage::Format_Grayscale8)`. Simple probe removal would therefore still save 8-bit TIFF pixels when acquisition is Mono16. The following diagnostic runs injected a test-only native-depth converter so writer capacity could be measured with genuine 16-bit camera samples; production conversion remained unchanged.

All runs used actual 2304×2304 Mono16 Fast camera frames at 1.000897 ms exposure. The physical USB source matched the supplied 31.6 fps figure. Acquisition and the independent ordered detector both completed 94/94 frames in every run with zero gaps, duplicates, or ordering errors. Pillow fully decoded every saved file as 2304×2304 mode `I;16`.

| Native Mono16 writer | Saved / acquired | Rejected | Queue HWM | Mean service | p95 service | Mean capacity |
|---|---:|---:|---:|---:|---:|---:|
| Qt `QImageWriter`, probe omitted | 94 / 94 | 0 | 1 / 16 | 13.554 ms | 24.083 ms | 73.78 fps |
| OpenCV 4.12 `cv::imwrite`, libtiff, compression none | 81 / 94 | 13 | 16 / 16 | 41.163 ms | 86.203 ms | 24.29 fps |
| libtiff 4.7.1 direct encoded strip, compression none | 94 / 94 | 0 | 11 / 16 | 15.474 ms | 56.851 ms | 64.63 fps |

OpenDSS already links OpenCV `imgcodecs` and the installed OpenCV build has TIFF support backed by libtiff. OpenCV officially supports `CV_16U` TIFF and exposes uncompressed dump mode; it is functionally suitable but its high-level per-file path is decisively too slow here. Direct libtiff removes the need for a hand-written TIFF formatter and improves the codec stage relative to OpenCV, but per-frame temporary-file and write-through publication variance still leaves it below target.

At 89.1 fps the entire persistence service has 11.223 ms/frame. The best genuine-16-bit result, Qt without the probe, averaged 13.554 ms: 2.331 ms (20.8%) over budget and only 73.78 fps capacity. Therefore simple probe removal cannot sustain the provisional 89.1 fps native-16-bit target on this computer, before considering a slower computer. It remains sufficient only for the separately measured 8-bit case on this machine.

Native Mono16 contains 10,616,832 pixel bytes/frame, requiring about 946.0 MB/s at 89.1 fps before TIFF overhead. None of the separate-file uncompressed writers reduces this byte rate. Meeting the target on slower computers would require a new measured persistence design—most plausibly sequential local spooling/preallocation or another bounded high-throughput storage format followed by TIFF finalization—plus an explicit slowest-supported storage contract. Increasing the queue cannot repair this sustained deficit.

## Authorized production fix and verification

The user explicitly accepted that Image Sequence persistence may skip frames when the computer cannot sustain the incoming rate and authorized the smallest probe-removal fix. This decision does not relax the detector invariant: every acquired frame must still enter event detection in order. Existing queue-rejection counts, ranges, degraded error, Failed manifest status, and `queue_rejection` stop reason remain unchanged and continue to report persistence loss truthfully.

The production change removes only the post-publication `QImageReader::canRead()` block and its now-unused include from `app/runtime/v2/sequence/image_sequence_capture_service.cpp`. Target-exists rejection, `QImageWriter` error handling, temporary-file flush, no-replace write-through publication, queue capacity/order, manifests, and consumer-failure accounting are unchanged. Camera, DCAM, detector, ONNX, training/export, native-depth conversion, and unrelated persistence behavior are unchanged.

Focused Release verification passed `image_sequence_capture_service_test` 1/1. A production-default real-camera HIL run then used the changed production writer and converter with 2304×2304 Mono8 Fast frames at the physical USB rate of 63.309 fps:

| Metric | Production-default result |
|---|---:|
| Acquired frames | 188 |
| Detector completions | 188 |
| Source gaps / duplicates / out of order | 0 / 0 / 0 |
| Saved frames | 188 |
| Queue rejections | 0 |
| Persistence consumer failures | 0 |
| Save-queue high-water / capacity | 11 / 16 |
| Lifecycle | Completed |

Pillow independently fully decoded all 188 TIFFs as 2304×2304 8-bit grayscale images; every file was 5,308,630 bytes. The original Mono16 Fast camera profile was restored after the run. This HIL result demonstrates that the production-default fix preserves detector delivery and eliminates persistence loss at the available physical USB rate on this computer. It does not claim zero-loss native-16-bit persistence at 89.1 fps or on every slower storage tier; under the accepted policy, those systems may reject persistence handoffs while detector delivery remains complete and the manifest reports the loss.

A fresh read-only Plan Guardian returned `PASS`. The coherent primary Release build passed and CTest passed 53/53 in 198.53 seconds. The actual-v2 Release build passed and its CTest gate passed 2/2 in 7.21 seconds, including 55/55 QML checks. No production camera, detector, inference, training, or native-depth conversion file was changed.

## Durable evidence

- Exact report: `C:\b\odss-debug-lead-hil\dbg019-characterization-20260731\exact-independent.json`, SHA-256 `3D15B6C0C267F7795043FE65396D31D4A7BC227299BAF1DB2BDFAA2AA8B75FB6`.
- Split report: `C:\b\odss-debug-lead-hil\dbg019-characterization-20260731\split-independent.json`, SHA-256 `354946FBF76CFCCB990BD46237D2780BB3B3488D671BED540EFA14B0487A45B6`.
- No-probe report: `C:\b\odss-debug-lead-hil\dbg019-characterization-20260731\no-probe-independent.json`, SHA-256 `AC9A975A696DD35EE9F50A9C2EF7D1B4EEAC94D846848ECD4648399A5EE2EE35`.
- Direct Mono8 report: `C:\b\odss-debug-lead-hil\dbg019-characterization-20260731\direct-independent.json`, SHA-256 `96A03A604E98EE61842DD66AD8FFEF959EAEE2F5118E7A210C5F6C498400B8B3`.
- Exact failed recovery manifest: `...\DBG-019-exact-20260731_131624\sequence.partial.json`, SHA-256 `26F35977133F2FBF67271DDA16252513A85944A0378AA85150C5F068EEA80EE8`.
- Split failed recovery manifest: `...\DBG-019-split-20260731_131640\sequence.partial.json`, SHA-256 `F4076BE07585E5DC95E748138D8EFFFAF70A89A47EA083CEB813FB2809BDC9E0`.
- No-probe completed manifest: `...\DBG-019-no-probe-20260731_131817\sequence.json`, SHA-256 `9D0B2CB78B8216FB648AEB36FA78C4DBD3B7C675ABF3881EB5AA7AD5D6984E4D`.
- Direct Mono8 completed manifest: `...\DBG-019-direct-20260731_134150\sequence.json`, SHA-256 `CAB80FE8F34741629EFF686D9E643AFE2063D7D4C7A09DF489FE3DFE618A4578`.
- Qt no-probe native-Mono16 report: `C:\b\odss-debug-lead-hil\dbg019-characterization-20260731\no-probe-mono16-independent.json`, SHA-256 `961C8A7494C6105B7C6C378C7A2D3097D0DA8CDAEFD51D5C8A5C1D252F9F2636`.
- Qt no-probe native-Mono16 completed manifest: `...\DBG-019-no-probe-20260731_141053\sequence.json`, SHA-256 `E157A15EA5A91EE390EF6936A580513A29A6104F32B6E0C035D9CA43631D1401`.
- OpenCV native-Mono16 report: `C:\b\odss-debug-lead-hil\dbg019-characterization-20260731\opencv-mono16-independent.json`, SHA-256 `ABC40D878F10B93F0618173D12B20DD3732CDDB3FBCCF8B355AF334BABB23A29`.
- OpenCV native-Mono16 failed recovery manifest: `...\DBG-019-opencv-20260731_141032\sequence.partial.json`, SHA-256 `EDA4445ACC046454972060CAC15260AEE99A064A8810967C37EA91C5386FE2FC`.
- Direct-libtiff native-Mono16 report: `C:\b\odss-debug-lead-hil\dbg019-characterization-20260731\libtiff-mono16-independent.json`, SHA-256 `C3EB62EED81945850CEAAAF549A8F4D62C21B0DE37588106321E2CAC241CA87B`.
- Direct-libtiff native-Mono16 completed manifest: `...\DBG-019-libtiff-20260731_141457\sequence.json`, SHA-256 `65005AF0DC8F775EB99DB821569E298F947211C1BFCC056E99CD882B028D0F77`.
- Production-default fixed report: `C:\b\odss-debug-lead-hil\dbg019-characterization-20260731\production-fixed-mono8-independent.json`, SHA-256 `4C872F4FFBA2EAD45407B03895E9CC2297F4D1B18FCB88586716B54728B045A0`.
- Production-default completed manifest: `...\DBG-019-production-20260731_142439\sequence.json`, SHA-256 `3E523BA005F0EAF108CBA90F990743E8847587BF1F6EA9EEDD5BE24BF6C5115A`.
- Actual-v2 retained QML result: `C:\b\d13\Desktop_app_v2\Release\ShellSingleImage-results.txt`, `55 passed, 0 failed, 0 skipped, 0 blacklisted`.
- Verified program executable: `C:\b\d13\manual-test-20260731\dbg019-portable\OpenDSS_20260731_144753\OpenDSS.exe`, SHA-256 `F4BF2AB904BAADC984B7EADF5425AC14A4E9F7E51F6DF902DBFF54BDAABC775A`.
- Guarded standalone package: `C:\b\d13\manual-test-20260731\OpenDSS_Standalone_2.0_DBG-019.zip`, 1,236,348,597 bytes, SHA-256 `4485A76B3ED37BE51061A9FC292059C65CA4700099A10D3F71BE5A4A2178D31E`. The archive contains 8,858 entries; streamed verification confirms the enclosed `OpenDSS.exe` has the accepted executable hash and `HOW-TO-REPLACE-EXISTING-OPENDSS.md` has SHA-256 `3DC72B84740781414655471A66E373CB70DB8AB1E99D557E8E725E2034ADB1CB`.
