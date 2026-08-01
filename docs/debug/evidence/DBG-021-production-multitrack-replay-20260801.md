# DBG-021 production multi-track replay

Date: 2026-08-01

## Scope

- Background only: source frames 1400–1449.
- Target only: source frames 10200–10420.
- Production path: `FastEventDetector` through `DropletFrameProcessor` and `CropService`.
- No full-dataset scan, GUI, DAQ output, Graphify, or grepai indexing.

## Result

- Passed with 50/50 background and 221/221 target frames decoded.
- Five tracks entered and each produced exactly one crop: frames 10200, 10247, 10303, 10349, and 10395.
- Two tracks remained simultaneously visible over four spans: 10247–10250, 10303–10309, 10349–10363, and 10395–10407.
- Each older track remained active until its own disappearance: track 1 ended at 10252, track 2 at 10311, track 3 at 10365, and tracks 5/4 at 10409/10410.
- Zero replay failures and zero track-capacity overflow.
- Production detector/processor time: 0.601 ms average, 0.738 ms p95, 0.930 ms p99, and 6.319 ms maximum.

## Integrated verification

- Focused Release tests: 7/7 passed.
- Release `ALL_BUILD`: passed.
- Camera→service→production-detector check: passed in 8.81 seconds. Full frame completed 270/270 at 63.177 fps; fast ROI completed 2800/2800 at 662.325 fps. Both had zero missing IDs, ordering faults, pixel mismatches, or source coalescing. No DAQ output occurred.

## Evidence identity

- JSON: `docs/debug/evidence/DBG-021-production-multitrack-replay-20260801.json`
- JSON SHA-256: `58D5E4AF09A38570691CF6173F3247A6681982C82D5384F4256AA8B5099818F2`
- JSON size: 310,604 bytes
