# Exp Report — Refinement Move-Space Unlock (BATCH / REFINE_ALLFF / ORACLE_REBANK / DENSITY_REPAIR)

**Date**: 2026-07-04 · **Workspace**: V3 (`v3_experimental`) · **Commit**: `c702d48` · **Status**: shipped (env-gated, defaults off)

## Summary

Four env-gated refinement upgrades, developed and verified in one session, produce a **strict 7/7 win** over the prior all-time bests and pull the composite-vs-contest-top-3 from 0.968 to **0.954**. Every number below is from `evaluator/preliminary-evaluator` run manually on the final `.out` (`Check pass!`), plus `checker/sanity` and `checker/placement_checker`.

| Case | prior best | new best | Δ | vs contest top-3 |
|---|---:|---:|---:|---:|
| testcase1_0812 | 735,326,724 | 734,810,949.69 | −0.07% | −0.59% |
| testcase2_0812 | 743,940 | **724,954.31** | **−2.55%** | **−3.08%** |
| testcase3 | 727,140,578 | 726,187,285.47 | −0.13% | −0.43% |
| hiddencase01 | 30,277,542 | 30,188,054.18 | −0.30% | −4.20% |
| hiddencase02 | 11,099,145 | **10,193,515.72** | **−8.16%** | **−23.24%** |
| hiddencase03 | 55,846,482 | 55,795,735.50 | −0.09% | −0.26% |
| hiddencase04 | 727,171,564 | 726,287,287.18 | −0.12% | −0.33% |

**Official solo re-runs (2026-07-05, sequential)** confirmed equal-or-better on every case except an 8.5-point (0.000015%) jitter on hc03: tc1 734,810,949.69 (identical — converged cases are deterministic) / tc2 **724,816.02** / tc3 726,167,604.40 / hc01 30,188,054.18 (identical) / hc02 **10,187,366.75** / hc03 55,795,744.04 / hc04 726,265,768.78. Official composite **0.9540**. These are the thesis-record numbers.

## Production recipe

```
OMP_NUM_THREADS=8 BANKING_MODE=matching PRODUCTION=1 INCR_RELOC=1 \
RELOC=1 CRIT_SWAP=1 BIT_REPAIR=1 BIT_REPAIR_DYNA={1 for hc02,hc03; 2 otherwise} \
BIT_REPAIR_BATCH=1 REFINE_ALLFF=1 ALT_ROUNDS=2 BIT_REPAIR_TIME=600 \
ORACLE_REBANK=1 REBANK_TIME=240 DENSITY_REPAIR=1
```

Interleaving matters: ALT_ROUNDS=2 (RELOC→CRIT_SWAP→BIT_REPAIR→REBANK ×2) beat the single-pass stack by −185K on hc02 — merges create new bit-repair opportunities and vice versa.

## The four changes

### 1. `REFINE_ALLFF=1` — the root discovery
`FF::isLegalize` is **Banking's "skip in Legalize stage" work-queue marker, not placement liveness**. Banking sets it false for FFs entering clustering (`Banking.cpp:769`) and true only on its own commit paths; `Legalizer::LoadFF` selects flag-false FFs for placement and never sets the flag back. Consequence: every FF placed by the final Legalizer stayed flag-false forever, and the candidate filters of RELOC / CRIT_SWAP / BIT_REPAIR silently excluded them. On hc02 that was 2,024 of 6,309 physicals — including **all 2,000 two-bit MBFFs**, i.e. the entire 2-bit swap space. tc2's long-standing "TNS 5,422 floor" was an artifact of this filter. Post-DP, `FF_Map` membership is the placement truth (dump + checkers prove it every run).

### 2. `BIT_REPAIR_BATCH={1,2}` — dynasearch apply throughput
The legacy greedy aff-disjoint batch apply discarded ~90% of improving candidates per round (hc02: 152 applied of 1,703) even though each candidate is exact-repriced by `evalBitSwapDelta` against the committed state immediately before apply — cone overlap cannot stale that delta. Mode 1 removes the disjoint skip (serial exact-repricing apply); mode 2 re-prices conflicted survivors in parallel between disjoint sub-batches (not better than mode 1 in tests). Verified: `INCR_VALIDATE` incr==full every round (diff 0.000000), `DYNA_CHK` 7.4e-13.

### 3. `ORACLE_REBANK=1` — first oracle-priced structural rebanking
Post-LG merges 2b+2b→4b and 4×1b→4b via `bankFF_deferred`/`rollbackBank`/`commitFinalizeBank` staging (FF_Map untouched until commit). Accept iff `α·ΔTNS(incr oracle) + β·ΔPower + γ·ΔArea (lib exact) + λ·ΔViol (BinDensityTable) < 0`, strictly monotone; `FindPlace` failure = reject. hc02: single clk domain, dPA = −3,000 weighted units per 2b+2b→4b merge (FF25×2 → FF29). This is the move class where every crude-model attempt (MATCH_HIGHER_BIT +254% hc02, unbankRebank v2 +134%) cascaded — exact per-move pricing + monotone accept removes the cascade mechanism, confirming the tc2-diagnosis prediction that the model, not the move class, was the blocker.

### 4. `DENSITY_REPAIR=1` — pricing the λ term
The density term (λ × #violating bins) was invisible to every refinement operator. New pass: per violating bin, evict FFs to `FindPlace` sites outside the bin, chain-commit iff `α·ΣΔTNS + λ·ΔViol < 0`, else revert all. hc01 smoke: 13/15 bins fixed in 0.16 s, ≈ −120K net.

## Verification

- **Byte-exact off**: old binary (git HEAD rebuild) vs new binary with all gates unset — `cmp` byte-identical on round-capped tc2 config, for dyna1 and dyna2, re-verified after each feature landed.
- **Oracle integrity**: `INCR_VALIDATE` full-recompute cross-check diff 0.0 every round under BATCH=1.
- **Legality**: evaluator `Check pass!` + sanity + placement_checker on every reported run.
- **Determinism**: fixed-round configs are byte-identical across binaries/runs. Time-budgeted runs on descending cases (hc02) differ run-to-run by where the budget cuts the descent (~0.1%); this is inherent to all anytime stages (pre-existing), not introduced by these changes.

## Follow-ups

1. ~~Solo re-runs of the 7-case table~~ — done 2026-07-05, see summary table note.
2. REBANK throughput: 9,720 trials/120 s (~12 ms/trial, FindPlace + trial-apply dominated) → a side-effect-free structural ΔTNS estimator (generalizing `evalBitSwapDelta` to arbitrary bit→(cell,slot) remaps) would allow parallel screening; also EJECT mode (debank mispriced merges) not yet implemented — the remaining tc2 lever.
3. Consider flipping the new gates to default-on after a regression-watch period (needs the solo record runs first).
4. `BIT_REPAIR_DYNA` per-case choice (1 vs 2) could become β-adaptive like MATCH_K.
