# DBG-021 native multi-droplet replay

Date: 2026-08-01

## Scope

- Dataset root: `E:\260731 Datacollection\test`
- Background-only frames: 1400–1449 (50 files)
- Target replay: 10200–10420 (221 files)
- No camera, DAQ, GUI, Graphify, grepai, or full-dataset replay was used.
- The replay target is test-only. This evidence predates the production multi-track integration.

## Result

The replay passed with 50/50 background frames and 221/221 target frames decoded, zero failures, and zero candidate-capacity overflow.

The target interval contains 39 frames with two full qualifying droplet components in four spans:

- 10247–10250
- 10303–10309
- 10349–10363
- 10395–10407

In the final span, the test-only tracker retained track 3 on the older right-hand droplet and created track 4 for the newly entering left-hand droplet at frame 10395. Both identities remained stable through frame 10407. Track 4 received one crop at entry (`x=28, y=48, width=138, height=138`); track 3 remained active until its disappearance tolerance expired after frame 10409.

Candidate A remained attached to the older droplet and emitted no entry/crop for the new droplet. Its sole entry in this replay was at frame 10200 because the target window began inside an already-active droplet. This proves that a single active centroid is insufficient for the required crop/inference contract even though it suppresses random component switching.

Frame 10400 visually includes a third partial object at the left edge, but only the two complete droplets pass the detector's component-local area, bounding-box, and frame-margin qualifications. The qualified claim is therefore two simultaneous full droplets, not three.

## Timing

- Production Candidate A detector: 0.672 ms average, 1.043 ms p95, 1.416 ms p99.
- Test-only candidate detector: 0.520 ms average.
- Test-only fixed-array tracking: 0.137 ms average, 0.235 ms p95, 0.339 ms p99.

The diagnostic tracker performs a second connected-components pass. A production implementation must reuse the detector's existing component loop, so these numbers are not an additive production-cost claim.

## Evidence identity

- JSON: `docs/debug/evidence/DBG-021-multidroplet-replay-20260801.json`
- JSON SHA-256: `AF69129D0F0C6055938E9BC6570B24B614B7F8B210F4FB8633DC41B6FF73D536`
- JSON size: 469,709 bytes
