# DBG-001 real DCAM headless evidence — 2026-07-30

## Artifact

- Source: `app/runtime/tests/real_dcam_pipeline_headless.cpp`
- Target: `opendss_real_dcam_pipeline_headless`
- Release executable: `C:\b\odss-debug-lead-hil\desktop_app\tests\Release\opendss_real_dcam_pipeline_headless.exe`
- Device: physical `DCAM:0`
- Camera SDK: Hamamatsu DCAM SDK 4
- Detector: production `FastEventDetectorAdapter`

The executable opens the physical camera through `DcamCameraDevice`, applies and
reads back the requested profile, continuously drains real DCAM frames, and
submits every drained frame to the production event detector in delivery-ID
order. It fails closed on DCAM overrun, gaps, duplicates, out-of-order delivery,
format mismatch, or detector count mismatch. Persistence is not measured.

## Observations

### Current-profile safety check

The physical camera opened and reported:

- 2304 × 2304
- Mono16
- Fast readout
- 42.954 ms exposure

The harness correctly refused to run the Mono8 detector path against this
incompatible current profile.

### Initial real ROI run — failing characterization

Profile: 1152 × 288, Mono8, Fast, 1 ms, 5 seconds.

- Real frames drained and detector-completed before failure: 1,825 / 1,825
- Source rate: 710.858 fps
- Detector completion rate: 705.030 fps
- Delivery gaps / duplicates / out-of-order: 0 / 0 / 0
- Failure: `DCAM ring buffer overrun: 18 unread frames exceed 16 buffers`

The initial single-threaded harness blocked DCAM draining while running the
detector. A rare 11.385 ms detector stall and the small 16-frame camera ring were
enough to overrun acquisition.

### Corrected real ROI run — pass

The harness was corrected to keep DCAM draining independently while a separate
ordered consumer submitted every frame to the detector.

Profile: 1152 × 288, Mono8, Fast, 1 ms, 5 seconds.

- Real DCAM delivery span: 3,554
- Frames drained / detector submitted / detector completed:
  3,554 / 3,554 / 3,554
- Source rate: 710.858 fps
- Detector completion rate: 706.039 fps
- Delivery gaps / duplicates / out-of-order: 0 / 0 / 0
- DCAM overrun: false
- Maximum DCAM drain batch: 4
- Maximum detector queue depth: 60
- Detector latency p50 / p95 / p99 / max:
  0.565 / 1.301 / 2.191 / 10.408 ms
- Result: pass

### Corrected real full-frame run — pass

Profile: 2304 × 2304, Mono8, Fast, 1 ms, 5 seconds.

- Real DCAM delivery span: 316
- Frames drained / detector submitted / detector completed: 316 / 316 / 316
- Source rate: 63.309 fps
- Vendor baseline: 63.33 fps
- Average detector service time: 7.804 ms/frame
- Equivalent detector service capacity: 128.140 fps
- CoaXPress minimum: 89.1 fps
- Capacity above CoaXPress minimum: 43.8%
- Conservative headroom target: 100 fps
- Capacity above conservative target: 28.1%
- Delivery gaps / duplicates / out-of-order: 0 / 0 / 0
- DCAM overrun: false
- Maximum DCAM drain batch: 1
- Maximum detector queue depth: 1
- Detector latency p50 / p95 / p99 / max:
  8.134 / 12.180 / 13.228 / 13.502 ms
- Result: pass

The physical USB source cannot produce the test computer's CoaXPress rate.
CoaXPress readiness is therefore a mathematical capacity check: 89.1 fps
requires an average service time below 11.223 ms/frame, and the 100 fps
headroom target requires below 10.000 ms/frame. The measured 7.804 ms/frame
passes both limits.

### Release service-path characterization — pass

`opendss_camera_pipeline_characterization_test` exercises
`CameraService` → `CameraController` → production detector before preview
coalescing:

- 2304 × 2304 Mono8 Fast: acquired / service-published /
  detector-entered / detector-completed = 270 / 270 / 270 / 270.
- 1152 × 288 Mono8 Fast: acquired / service-published /
  detector-entered / detector-completed = 2,800 / 2,800 / 2,800 / 2,800.
- Both profiles: zero missing IDs, zero out-of-order frames, zero pixel-ID
  mismatches, and zero source coalescing before the service.
- Release CTest result: pass in 8.90 seconds.

### Restoration check

After both profile-changing runs, the next physical-camera readback again
reported the original 2304 × 2304, Mono16, Fast, 42.954 ms profile.

## Conclusions

- These were physical-camera runs, not simulated-frame runs.
- Ordered independent acquisition removes the observed DCAM ring overrun while
  preserving exactly-once detector submission for every drained frame.
- Full-frame 8-bit Fast acquisition matches the vendor-provided throughput
  baseline.
- The measured detector service capacity mathematically exceeds the 89.1 fps
  CoaXPress specification and the 100 fps conservative headroom target.
- No droplets were detected in these short live scenes, so this evidence proves
  acquisition-to-detector delivery and throughput, not droplet-recall
  acceptance.

## Remaining risk

- The 89.1 fps CoaXPress result is a user-authorized mathematical capacity
  check, not a physical CoaXPress HIL run.
- Dataset persistence throughput and GUI scheduling were not measured here.

## Rollback

The headless target is isolated test infrastructure. Production rollback is the
reversion of the uncommitted `DBG-001` acquisition diff; the harness itself can
be removed without altering qualified detector decisions.
