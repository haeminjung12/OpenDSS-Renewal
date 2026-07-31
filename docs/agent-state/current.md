# OpenDSS Agent State

State format: `1`

- Policy revision: `ODSS-2026-07-30.4`
- Mode: `implementation`
- User-facing Lead: `Implementation Lead`
- Checkpoint branch: `codex/v2-shell-single-image-slice`
- Checkpoint commit: `2001d66` (authorized integrated product-source commit)
- Active ID: `AE-1+DBG-020`
- Status: `COMPLETE — integrated, committed, verified, and packaged; awaiting user manual acceptance`
- Dirty paths at checkpoint: pre-existing untracked `.worktrees/` only; preserved and excluded
- Updated: `2026-07-31T18:04:30-05:00`

## Accepted decisions

- The Implementation Lead is the sole user-facing implementation agent.
- Integrate exact pushed debug commit `864f97d09e12688a03864696b7fcf2207e5c808d`.
- Integrate the verified but uncommitted `DBG-019` and `DBG-020` changes from the designated Debug Lead worktree without independently recreating them.
- Integrate the previous Implementation Lead's completed AE-1 change from the pre-fast-forward Implementation worktree.
- The user's 2026-07-31 instruction reauthorizes minimal one-shot Auto Exposure and supersedes the prior removal decision in `ODSS-DES-002`.
- AE-1 remains limited to one `Auto` button beside the existing Exposure input, automatic update of that input, no more than two decimal places, and preservation of the existing camera/exposure path.
- Preserve the user-authoritative detector invariant: every acquired frame reaches the detector once, in order, before occurrence, trajectory, crop, or ONNX-routing decisions; preview frames may be dropped, detector-input frames may not.
- Image Sequence camera input may be 8-bit or 16-bit, while TIFF output remains 8-bit.
- Per-frame spool checksums and crash/failure resume remain explicitly declined non-goals.
- Deliver one verified executable and a portable ZIP containing instructions for replacing an old installation and the actual-camera every-frame diagnostic.

## Accepted evidence

- AE-1 authority and scope: `docs/v2/design/consolidated-design-draft.md` §19.3.2 and `docs/v2/implementation/current-slice.md`.
- Debug authority and exact records: `docs/debug/bug-ledger.md`, especially `DBG-019` and `DBG-020`.
- DBG-019 evidence: `docs/debug/evidence/DBG-019-persistence-characterization-20260731.md`.
- DBG-020 evidence: `docs/debug/evidence/DBG-020-chunk-spool-characterization-20260731.md`.
- Recoverable pre-integration snapshots: AE-1 stash objects `08e6b07a8f467a9463bca5200e89033ed7763376` and `78aaba4717ba438f6e52f4e936163636a4b2c64f`; DBG-019/020 stash object `1785b5abe30f313712678f497a4af8429c4f638f`.

## Verification

- Policy revisions verified: `G-2026-07-30.4` and `ODSS-2026-07-30.4`.
- Implementation worktree verified at `C:\Dev\OpenDSS-Renewal` on `codex/v2-shell-single-image-slice`.
- The implementation branch fast-forwarded from `cb101cb87c23e1718ea4e97316e052091e194bee` to exact committed debug head `864f97d09e12688a03864696b7fcf2207e5c808d`.
- The designated Debug Lead worktree remains at `864f97d` with verified DBG-019/020 changes intact.
- The previous Implementation worktree state and Debug Lead dirty state were captured recoverably before integration.
- The updated `ODSS-DES-002` lock must pass before the renewed Plan Guardian gate.
- `ODSS-DES-002` lock verification passed at SHA-256 `9be03ed85474f41fb55261418f37c1f61537107adc4afe398e02a0f6127a2932`.
- The actual v2 executable was rebuilt at `C:\b\d13\i\Desktop_app_v2\Release\Desktop_app_v2App.exe`; SHA-256 before packaging is `9B146F358F5B51688F2C51E361712EC0D1AE566EC2491E4176B294A39B77AC49`.
- Focused `camera_controller_test` passed 1/1, including no-frame preservation and accepted one-shot calculation/readback.
- Final actual-v2 CTest passed 2/2; QML totals are 56 passed, 0 failed. The native-window runtime-path test passed with the deterministic Qt environment.
- Fresh final-diff Plan Guardian: `PASS`; scope, authority, protected boundaries, detector invariant, declined non-goals, and accepted evidence align.
- Authorized integrated product-source commit: `2001d66` (`Integrate high-rate sequence spooling and auto exposure`).
- Final portable directory: `C:\b\d13\manual-test-20260731\ae1-dbg020-final-portable\OpenDSS_20260731_175800`; guarded package manifest status `pass`; packaged launch smoke remained running after five seconds.
- Final executable: `C:\b\d13\manual-test-20260731\ae1-dbg020-final-portable\OpenDSS_20260731_175800\OpenDSS.exe`; SHA-256 `9B146F358F5B51688F2C51E361712EC0D1AE566EC2491E4176B294A39B77AC49`.
- Final portable ZIP: `C:\b\d13\manual-test-20260731\OpenDSS_Standalone_2.0_AE1_DBG-020_FINAL.zip`; 1,237,298,496 bytes; SHA-256 `0974314808166AFE43A6A9E1E70C30A08BFF45FE848E699282BDE3A90E2C8699`.
- Streamed archive verification found 8,934 entries, matched the embedded executable hash, and confirmed replacement instructions plus both every-frame camera diagnostic files.

## Blockers

- None. Auto Exposure authority conflict was resolved by the user's explicit inclusion instruction and recorded in the master specification and active slice.

## Exact next action

Await user manual acceptance of the delivered executable and portable ZIP. Do not reopen DBG-014 through DBG-020 or add declined checksum/crash-resume features.
