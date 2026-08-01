# DBG-021 crop-failure and multi-droplet evidence — 2026-08-01

## Scope

- Visually inspected all 294 manifest-listed crop PNGs. This is a complete crop-output review, not a complete source-sequence review.
- Mapped every rejected crop back through `dataset.json` to its exact source frame and crop rectangle.
- Inspected all 294 crop-event source frames, 477 unique source frames within ±5 of the rejected crops, a sparse every-tenth-frame scan of 2,049/20,499 sequence TIFFs, and a focused 41-frame neighborhood around the first confirmed three-droplet hit.
- The complete 20,499-frame source sequence was not scanned. Graphify, grepai, GUI, camera, DAQ, hardware output, and dataset writes were not used.

## Crop-output classification

Of 294 crop PNGs, 49 contain no complete droplet:

- 16 are blank or near-uniform.
- 33 contain only a partial border/nozzle/droplet arc.
- 245 contain a complete droplet.

The 49 failures are geometrically distinct from good crops:

| Metric | No-complete-droplet crops | Other crops |
|---|---:|---:|
| Count | 49 | 245 |
| Median source crop width/height | 48×48 | 134×134 |
| Width range | 34–82 | 130–194 |
| Source rect at x ≥ 950 | 48/49 | 21/245 |

Thus 98.0% of failed crops are small right-edge/nozzle selections. The one exception is crop 198 at the lower-left boundary.

### Blank or near-uniform crops

| Crop | Source frame | Source rectangle |
|---:|---:|---|
| 26 | 1853 | `(1048,129) 42×42` |
| 32 | 1989 | `(1040,127) 48×48` |
| 38 | 2197 | `(986,32) 36×36` |
| 41 | 2408 | `(986,31) 36×36` |
| 43 | 2476 | `(986,32) 36×36` |
| 57 | 4119 | `(1038,121) 54×54` |
| 68 | 4682 | `(991,16) 42×42` |
| 86 | 5441 | `(1031,122) 42×42` |
| 88 | 5517 | `(992,18) 40×40` |
| 89 | 5519 | `(1020,113) 60×60` |
| 90 | 5523 | `(1027,114) 54×54` |
| 93 | 5665 | `(1060,112) 40×40` |
| 202 | 14465 | `(987,22) 34×34` |
| 203 | 14525 | `(986,16) 40×40` |
| 206 | 14589 | `(981,16) 44×44` |
| 213 | 14777 | `(1042,126) 36×36` |

### Edge-only/no-complete-droplet crops

`29→1977`, `36→2124`, `37→2192`, `40→2404`, `42→2472`, `46→2758`, `47→2889`, `48→2958`, `49→3097`, `50→3419`, `52→3552`, `53→3621`, `55→4002`, `59→4182`, `62→4323`, `67→4614`, `73→4820`, `77→4959`, `79→5025`, `87→5513`, `92→5588`, `95→5820`, `97→5895`, `120→7483`, `121→7486`, `154→9668`, `156→9728`, `160→9910`, `181→11433`, `198→14336`, `210→14713`, `211→14718`, and `212→14771` (crop ID → source frame index).

## What went wrong

Directly checked facts:

1. Complete droplets are visible elsewhere in many source frames while the saved crop rectangle points to a 34–82 px fragment or empty patch near the right nozzle/border.
2. `CropService` centers deterministically on the detector-supplied bounding box; it does not choose an independent random location.
3. The current minimum bounding-box threshold is 32 px, so these 34–82 px fragments can qualify geometrically even though normal droplets produce approximately 130–194 px source boxes.
4. The original manifest contains repeated event/crop entries for these small right-edge fragments.

Reasonable inference:

- The apparent random crops are detector event-selection failures, not crop-service randomness. A partial outgoing droplet/nozzle foreground component survives the permissive minimum geometry and can be promoted after a gap or component-rank change. The crop service then faithfully crops that incorrect small box.
- Multi-droplet visibility makes this more likely because one droplet can be entering while another exits into the noisy right/nozzle region.

## Confirmed three-droplet source location

- Best frame: `sequence/frame_00010400.tif`.
- Confirmed span: source indices 10398–10407 contain three droplet-sized foreground components.
- Frame 10400 visibly contains one partial entering droplet at the left edge, one complete droplet around x≈190, and one complete droplet around x≈975.
- No crop record exists inside this three-droplet span. The surrounding manifest records jump from crop/event 164 at source frame 10192 to crop/event 165 at source frame 10506.

The absence of an event record inside the span is direct manifest evidence. Whether this is a missed expected droplet event or an intentional continuation cannot be decided without the detector state for the rejected/nonpersisted acquisition frames, but it is the correct native fixture for testing simultaneous three-droplet tracking.

## Artifacts

- Lossless PNG conversion of source frame 10400: `docs/debug/evidence/DBG-021-three-droplets-frame-00010400.png`
- PNG SHA-256: `7841E9B7BEA0F175D601C9D073C008F487B2350309AE32448168507B85BD4EAB`
- Source authority: `E:\260731 Datacollection\test\dataset.json` and its referenced crop/source files.

## Recommended next dataset-first test

Use frames 10350–10450 (or a slightly wider bounded span with a clean background lead-in) as the native three-droplet fixture for the test-only fixed-capacity tracker. Acceptance should require three stable track identities where visible, no promotion of the right/nozzle outline, one entry per genuine entering droplet, and no blank or edge-only crop.
