---
name: project-refine-allff-discovery
description: "isLegalize is a Banking work-queue marker, not placement liveness — REFINE_ALLFF unlocks ~32% hidden candidates; BATCH+ALLFF+REBANK+DENSITY stack shipped 2026-07-04, hc02 -8.2%, tc2 -0.73%"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9381340c-d956-45d8-adf9-8fe6a9322fba
---

**2026-07-04 V3 refinement stack breakthrough** (commit `c702d48`, branch `v3_experimental`).

**Core discovery**: `FF::isLegalize` is Banking's "skip in Legalize stage" work-queue marker, NOT placement liveness. Banking sets it false for FFs entering clustering (Banking.cpp:769) and true only on its own commit paths; `Legalizer::LoadFF` selects the false ones for placement and **never sets it back**. Result: every FF placed by the final Legalizer is permanently flag-false, and RELOC/CRIT_SWAP/BIT_REPAIR's candidate filters silently excluded them — on hc02 that was 2,024/6,309 physicals (all 2000 2-bit MBFFs = the entire 2-bit swap space). Post-DP, FF_Map membership is the placement truth.

**Four env gates shipped** (each byte-exact off, cross-binary cmp verified):
- `BIT_REPAIR_BATCH=1` — dynasearch apply drops the aff-disjoint skip (legacy discarded ~90% of improving candidates/round; each candidate is exact-repriced pre-apply anyway). Mode 2 = sub-batch parallel repricing, not better than 1 in tests.
- `REFINE_ALLFF=1` — drops the isLegalize filter in all three operators.
- `ORACLE_REBANK=1` — post-LG 2b+2b→4b + 4×1b→4b via bankFF_deferred staging, accept iff α·ΔTNS(oracle)+β·ΔP+γ·ΔA+λ·ΔViol < 0. hc02: dPA=−3000/merge, single clk domain, 2000 2-bit sources.
- `DENSITY_REPAIR=1` — per-violating-bin eviction chains, commit iff α·ΔTNS+λ·ΔViol < 0 (hc01: 13/15 bins, ~−120K).

**Winning production recipe** (interleave beats single-pass by −185K on hc02):
`BANKING_MODE=matching PRODUCTION=1 INCR_RELOC=1 RELOC=1 CRIT_SWAP=1 BIT_REPAIR=1 BIT_REPAIR_DYNA={1|2 per case} BIT_REPAIR_BATCH=1 REFINE_ALLFF=1 ALT_ROUNDS=2 BIT_REPAIR_TIME=600 ORACLE_REBANK=1 REBANK_TIME=240 DENSITY_REPAIR=1`

**OFFICIAL 7/7 records** (solo re-runs 2026-07-05, evaluator Check pass + sanity + placement_checker; outputs `/tmp/v1_*_official.out`): tc1 734,810,950 / **tc2 724,816 (−3.10% vs top-3!)** / tc3 726,167,604 / hc01 30,188,054 / **hc02 10,187,367 (−23.29% vs top-3)** / hc03 55,795,744 / hc04 726,265,769. **Composite 0.968 → 0.9540.** Strict win vs all prior bests incl. tc1/hc01 (legacy fallback arms unnecessary). tc2's "TNS 5422 floor" was an artifact of the filtered move space. tc1/hc01 solo == parallel byte-identical (converged cases deterministic). README + improvements.md + reports/v1/2026-07-04_exp_refine_allff_batch_rebank_density.md carry these numbers; commits `c702d48` (feat) + `5b5e165` + docs follow-up.

**Next levers**: REBANK throughput ~12ms/trial (FindPlace+trial-apply) — side-effect-free structural ΔTNS estimator (generalize evalBitSwapDelta to bit→(cell,slot) remaps) enables parallel screening; EJECT mode (debank mispriced merges) unimplemented = remaining tc2 lever; 8-bit merge stages if lib has them; gates still default-off — consider default-on after regression watch; BIT_REPAIR_DYNA 1-vs-2 could go β-adaptive. hc02 time-budget determinism jitter is inherent to anytime stages (fixed-round runs byte-identical). See [[project-tc2-verified-diagnosis]], [[project-thesis-parta-results]].


**2026-07-06 CRITICAL OPEN ISSUE — oracle-vs-evaluator TNS gap**: C6 checkpoints exposed systematic understatement (tc2 final 0.92%, hc02 1.12%). Incr==full verified exact (INTRA/BATCH/RESCAN clean); but computeAccurateTNS (multihop) ≠ evaluator: tc2 state tri.out → 1hop 4,285.7 / multihop 4,708.6 / eval-implied 5,333.7 (α=10). This is the OLD 5864-vs-7850 ghost — the 6/15 "faithful" oracle never closed it (validateTNSOracle hardcodes eval-true=7850.5). Evaluator DOES propagate (strings: updateArrival/TimingVisitor/SinkDelay::arrival). Fix plan + probe design in reports/v1/2026-07-06_diagnosis_oracle_vs_evaluator_gap.md. If fixed → better pricing → likely score gains + true evaluator-exactness for the paper. Paper headline numbers unaffected (evaluator-measured).

**HANDOFF (2026-07-07, Fable->Opus)**: full playbook at `Project Knowledge/plans/active/HANDOFF_to_opus_20260707.md` — task queue T1-T6 (harvest night workflow wu70n1k0r / EJECT close-out / re-review / oracle-gap probes via new PROBE_MOVE env gate / submission compliance), frozen-items list, environment traps (background-bash cwd resets, binary-copy discipline). EJECT 5/5 wins so far (tc1 -447K, hc01 -137K); PROBE_MOVE harness committed. Paper content<=6pp compliant, all claims honest-calibrated.

**EJECT FINAL (2026-07-07)**: strict 7/7 win, composite 0.9528→**0.9518** (tc1 734,319,834 / tc2 723,165 / tc3 726,041,482 / hc01 30,062,537 / hc02 10,089,886 / hc03 55,781,010 / hc04 726,015,652; all three checks pass). New V3 production config = unified + `ORACLE_EJECT=1 EJECT_TIME=180`. Logged in improvements.md. ASP-DAC paper stays frozen at 0.953 (decision); EJECT = thesis + TCAD material. tc1's −447K came from the case where every other operator had converged — new move class, not budget.