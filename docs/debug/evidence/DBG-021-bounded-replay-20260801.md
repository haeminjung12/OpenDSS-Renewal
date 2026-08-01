# DBG-021 bounded production-detector replay — 2026-08-01

## Scope and method

- User-authorized input was limited to `E:\260731 Datacollection\test\dataset.json`, crops 14–32, and the 701 ordered TIFFs `sequence/frame_00001400.tif` through `sequence/frame_00002100.tif`. The full dataset was not inspected.
- The test-only target `opendss_dbg021_bounded_detector_replay` feeds each decoded grayscale TIFF through the production `FastEventDetectorAdapter(FastEventConfig{})` and derives event-entry crop rectangles through `CropService::makeDatasetCrop`.
- `dataset.json` contains an empty `capture.detection_settings` object. `Desktop_app_v2/App/main.cpp` constructs the production detector from the same default `FastEventConfig{}`.
- The required fresh Plan Guardian gate passed. The first target build exposed one missing existing link dependency; after adding `event_detector.cpp`, the affected Release target built successfully and the replay exited zero.
- No Graphify or grepai index, GUI, camera, DAQ, hardware output, or dataset write occurred.

## Result

- Requested/decoded frames: 701/701.
- Decode or replay failures: 0.
- Detected frames: 579.
- Event entries and derived crops: 10.
- Detector timing: 0.568 ms average, 0.574 ms p50, 0.814 ms p95, 0.970 ms p99, and 7.296 ms maximum. Average detector-only capacity was 1,761.663 fps, well above the measured 710.858 fps Mono8 ROI camera rate.
- Decode timing was measured separately: 4.123 ms average and 8.255 ms p95. It is not detector cost.

## Named-event comparison

| Original saved crop | Original result | Fixed replay at the mapped frame | Fixed next clean event entry |
|---|---|---|---|
| Event 17, frame 1576 | `(980,11) 154×154`; a full right-edge droplet plus a partial second droplet | The same exiting droplet remains detected, but it is a continuation (`event_entered=false`), so no duplicate crop is created | Frame 1584: `(10,49) 134×134` |
| Event 26, frame 1853 | `(1048,129) 42×42`; blank/incorrect saved crop while two droplets are visible | No qualifying event entry and no crop are created | Frame 1855: `(22,48) 136×136` |
| Event 29, frame 1977 | `(1065,68) 80×80`; edge-only/nozzle-adjacent crop | No detection, event entry, or crop is created | Frame 1992: `(52,48) 136×136` |

Within original crop records 15–32, the old run created 18 crops, including repeated right-edge entries at frames 1576, 1639, 1644, 1781, 1846, 1853, 1913, 1977, 1981, 1985, and 1989. Over the same replay span, Candidate A creates nine event-entry crops, all ordinary 134–136 px left-side entries. It therefore suppresses the observed duplicate/random crop publications rather than moving them elsewhere.

## Remaining behavior

The replay also exposes a narrower per-frame association limitation. At frame 1973, after the tracked droplet reaches the right edge, the only qualifying foreground fragment is a `68×64` nozzle-adjacent component. The selected centroid moves 131.070 px from frame 1972 without a detection gap. Similar non-entry jumps occur at 1907→1908 (116.757 px) and 2041→2042 (133.332 px). Candidate A has no distance gate, so it cannot reject an unrelated component when that component is the only qualifier.

None of these three jumps fires a new event or creates a crop; the lifecycle subsequently ends and the next clean droplet creates a normal crop. The supplied-data replay therefore verifies event-entry/crop stabilization, but it does not prove perfect per-frame component identity near the nozzle/right boundary. Event 29 remains a separate merged/partial-mask case and is not claimed fully resolved.

## Artifacts

- Full JSON trace: `docs/debug/evidence/DBG-021-bounded-replay-20260801.json`
- JSON SHA-256: `1E2D4E5B2959DE33FD5082A614F3990557AC5360A4BCA9EA5360D1FBFC60F745`
- JSON size: 441,054 bytes.

## Next decision

Candidate A is sufficient to prevent the demonstrated random/duplicate crop publications in this 701-frame slice without threatening throughput. If per-frame detector identity near the nozzle must also remain stable, authorize a separate constant-time association guard (for example, a squared-distance gate or a qualified border-fragment rule) and characterize it specifically against Event 29 before changing production behavior.
