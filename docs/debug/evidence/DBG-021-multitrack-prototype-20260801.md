# DBG-021 test-only multi-centroid prototype — 2026-08-01

## Authority and scope

- The user authorized a test-first multi-centroid fix and required supplied-dataset results before any full production implementation.
- Production detector, adapter, crop, acquisition, and application files were not changed for this prototype.
- The existing test-only replay harness was extended with a three-slot centroid tracker and rerun over exactly `sequence/frame_00001400.tif` through `sequence/frame_00002100.tif` (701 frames). The full dataset was not inspected.
- Fresh Plan Guardian returned PASS. The affected Release replay target built and the replay exited zero.
- No Graphify, grepai, GUI, camera, DAQ, hardware output, or dataset write occurred.

## Prototype behavior

- Fixed capacities: three active tracks and 16 candidate components; no dynamic track/candidate container is required by the proposed production design.
- Candidate geometry and filtering are reconstructed from the same default `FastEventConfig{}` used by production.
- Flow assumption: droplets move left to right.
- Each existing track takes the nearest unused component that does not jump backward by more than half the larger bounding-box width.
- An unused component creates a new droplet track only when its centroid is in the normalized left entry zone (first 20% of frame width).
- Tracks retire after the existing two missed-frame reset interval.
- Entry crops use `CropService::makeDatasetCrop`.

This is diagnostic code, not production behavior. The harness performs a second connected-components pass so it can inspect every candidate without changing the protected production detector. A production implementation would integrate assignment into the existing component loop instead.

## Replay result

- Frames requested/decoded: 701/701.
- Failures: 0.
- Candidate A entries: 10.
- Multi-centroid prototype entries: 10.
- Maximum simultaneous active prototype tracks: 1.
- Candidate/track capacity overflow: 0.
- Prototype entry frames and crop rectangles exactly match the accepted Candidate A result: 1450 `(44,50) 134×134`, 1518 `(24,49) 134×134`, 1584 `(10,49) 134×134`, 1653 `(32,48) 136×136`, 1718 `(10,49) 134×134`, 1788 `(18,49) 134×134`, 1855 `(22,48) 136×136`, 1925 `(52,50) 134×134`, 1992 `(52,48) 136×136`, and 2058 `(10,49) 134×134`.
- No prototype entry or crop is created at mapped Event 17/frame 1576, Event 26/frame 1853, or Event 29/frame 1977.

## What the dataset actually exercises

| Qualifying candidate count | Frames |
|---|---:|
| 0 | 122 |
| 1 | 317 |
| 2 | 262 |

All 262 two-candidate frames contain one full droplet-sized component and one smaller nozzle/border fragment. Zero frames contain two full droplet-sized qualifying components, and zero frames contain two candidates together in the entry zone. The prototype correctly keeps the small right-side fragment from becoming a new droplet track, but the replay never exercises two simultaneous genuine droplet tracks. Its maximum active-track count is therefore one.

At the named frames, Event 17 has one qualifying component, while Events 26 and 29 have none. The visually apparent second object at Event 26 does not survive the current production threshold/morphology/area/bounding-box qualification as a second droplet candidate.

## Diagnostic timing

- Candidate A production detector: 0.543 ms average, 0.763 ms p95, and 0.897 ms p99.
- Prototype candidate tracking step: 0.121 ms average, 0.187 ms p95, and 0.256 ms p99. This includes the diagnostic second connected-components pass, JSON construction, and entry-time crop derivation; it is an upper bound, not projected production overhead.
- Conservatively adding that entire diagnostic tracking measurement to Candidate A gives 0.664 ms average and 0.949 ms p95, still below the 1.407 ms frame interval at the measured 710.858 fps ROI rate. A final implementation would reuse the existing component loop and should cost less, but must be measured rather than assumed.

## Assessment

The prototype does not regress the supplied 701-frame sequence and successfully prevents the persistent small right-edge/nozzle component from becoming a second droplet. However, this dataset cannot validate the central multi-track requirement because it contains no frame with two full qualifying droplet components. Production implementation is therefore not yet justified by this replay alone.

Before production work, validate the same prototype against either:

1. a bounded native sequence containing two full droplets that both survive current component qualification in the same frame; or
2. deterministic synthetic frames matching the measured 1152×288 geometry, motion direction, droplet size, entry overlap, and nozzle fragment.

Acceptance should require two simultaneous active track IDs, one event/crop per entering droplet, no nozzle-fragment track, no track swap, and retained 710.858 fps ROI headroom.

## Artifacts

- Full prototype JSON: `docs/debug/evidence/DBG-021-multitrack-prototype-20260801.json`
- JSON SHA-256: `36101B61388246615AE957615EAD8FC7FE4E9E189B70B858E40A01EBFF9B412B`
- JSON size: 1,463,717 bytes.
