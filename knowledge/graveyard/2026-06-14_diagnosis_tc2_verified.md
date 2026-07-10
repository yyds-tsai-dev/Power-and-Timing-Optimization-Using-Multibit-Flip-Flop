# Diagnosis — tc2 is the sole per-case gap, and it is 100% TNS-mispricing (evaluator-verified)

**Date**: 2026-06-14
**Binary**: V3 production `cadb_0015_final` (branch v3_experimental HEAD 7ecc4f4), `BANKING_MODE=matching PRODUCTION=1`
**Validation**: real `evaluator/preliminary-evaluator` on every `.out` (per memory `feedback_verify_legality`)
**Goal context**: "optimize score to beat all top-3 on all 7, evaluator-verified"

## 1. Verified baseline vs top-3 (real evaluator)

| Case | V3 verified | min(Team1,2,3) | result |
|---|---:|---:|---|
| testcase1_0812 | 737,272,707 | 739,200,000 | WIN −0.26% |
| **testcase2_0812** | **761,159** | **748,000** | **LOSE +1.76%** |
| testcase3 | 728,024,317 | 729,300,000 | WIN −0.17% |
| hiddencase01 | 30,615,486 | 31,510,000 | WIN −2.84% |
| hiddencase02 | 11,614,120 | 13,280,000 | WIN −12.5% |
| hiddencase03 | 55,870,606 | 55,940,000 | WIN −0.12% (thin) |
| hiddencase04 | 728,092,959 | 728,700,000 | WIN −0.08% |

**V3 beats all top-3 per-case on 6/7.** Average composite ≈ **0.977** → already beats all three teams (1.000/1.020/1.075) and matches NTU thesis ceiling. The strict per-case gate fails only on tc2; hc03 margin is thin (0.12%) and must not regress.

## 2. tc2 premise measurement — the gap is 100% TNS

Ran V3 tc2 with the internal cost table printed (`.out` cross-checks to 761,159):

| term | weight | internal value | note |
|---|---:|---:|---|
| TNS | α=10 | **5,864.6** | internal model |
| Power | β=400 | 313.9 (cost 125,570) | matches evaluator |
| Area | γ | 557,085 | matches evaluator |
| Bin | λ=10000 | 0 | matches evaluator |
| internal total | | 741,301 | |
| **evaluator total** | | **761,159** | |

Back-out: `α·TNS_eval = 761,159 − 125,570 − 557,085 = 78,505 → TNS_eval = 7,850.5`.
- **Internal underprices TNS by 33.9%** (5,865 vs 7,850).
- The entire internal-vs-eval gap (19,859) is **100% TNS**; Power/Area/Bin match exactly.
- NTU tc2: TNS≈5,223, Power 320.8, Area 557,850 — NTU has slightly MORE area/power but far LOWER TNS. **tc2 is a placement-quality/TNS problem, not merge-count.** Our commit count (10,262) is already optimal.

## 3. 27-config evaluator-verified sweep — no session-scale lever flips tc2

- **Threshold knobs** (RISK_SCALE 20/50/100/1e6, MATCH_MIN_GAIN −500/−2000/−10000, MATCH_K 15/20, combos): all regress; commit count saturated ~10,240–10,277. Forcing the 646 cost-dropped merges adds more TNS than the area/power saved.
- **Structural gates** (TIMING_PRELOC, NTU_FLOW, ACCURATE_BANKING/BFS, DP_ROUNDS 2/3): all regress (775K–785K). Existing relocation/accurate-timing features HURT tc2 (confirms `project_approach_c_deadend`, `project_net_hpwl_deadend`).
- **Best of 27** = `RELOC=1 + EGR` → 760,209 (−950, −0.12%). Still +1.63% above 748,000. Not shipped (global, unverified on other 6).

## 4. Conclusion

tc2 per-case requires the faithful forward-propagating timing oracle (`plan_two_track_beat_top3.md` Track A). Precise target: getSlack must report eval-true TNS (7,850 not 5,864) so banker+DP minimize REAL TNS toward NTU's 5,223 — which would flip tc2 by ~26K, well past the 13.2K needed. The A0 premise check is hereby **DONE and passed** (mispricing proven, evaluator-verified). This is a multi-week build; no tuning reaches it.
