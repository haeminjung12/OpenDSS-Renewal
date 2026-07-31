# DBG-011 Dataset Re-audit

Date: 2026-07-30

## Accepted dataset identity

- Dataset: `droplet_target_nontarget_3class_starter`
- Current `dataset.json` SHA-256: `58500D725D1E185667225835915DAC64473EB790896B561D6DB892CFBAF9233B`
- Retained audit manifest SHA-256: `1583ED3DDD9C76C4FBB8BADF34932FF79835AC7FAAA639E64FB4EE84F3361ADF`

## Reconciliation

- Dataset identity and three-class schema remain unchanged.
- The retained audit contains 3,625 records: 3,620 included and 5 rejected.
- Every included item retains the same record ID, crop path, and image SHA-256 in the current dataset.
- All rejected items remain excluded.
- The sole semantic difference is `legacy-row-000208`. The retained audit records label `1`, reviewed label `2`, and review state `relabeled`; the current dataset correctly uses the reviewed label `2`.
- Consequently, class totals changed from `3142/387/91` to `3142/386/92` for classes `0/1/2`.

## Conclusion

The current dataset is accepted as the reviewed successor to the prior serialized metadata. No image, crop, label, dataset file, or external audit file was modified during this re-audit. The registered representative test contract may pin the current `dataset.json` hash.

## Rollback

Revert the test-contract hash change if later provenance evidence invalidates this acceptance. External dataset contents remain untouched.
