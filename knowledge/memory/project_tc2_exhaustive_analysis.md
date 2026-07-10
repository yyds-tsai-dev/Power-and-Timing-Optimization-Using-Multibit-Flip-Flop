---
name: tc2 exhaustive optimization analysis
description: Every model-guided optimization regresses tc2; combinatorial cliff at current matching weights; only evaluator-oracle gives improvement
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
Exhaustive tc2 optimization sweep (2026-05-08). Baseline: 761,159 (evaluator). NTU: 744,231. Gap: 16,928.

**tc2 cost breakdown** (α=10, β=400, γ=0.0000008, λ=10000):
- TNS component: ~78,500 (10.3%) — evaluator TNS ≈ 7,850
- Power component: 125,570 (16.5%) — total power 313.9
- Area component: 557,085 (73.2%) — dominated by non-FF cells, nearly unchangeable
- Bin: 0

**Model accuracy hierarchy:**
- 1-hop (getSlack/CostCompare): TNS = 5,865 → 34% under evaluator
- BFS (computeAccurateTNS): TNS = 7,325 → 7% under evaluator
- Evaluator: TNS ≈ 7,850

**All experiments that REGRESSED tc2:**

| Experiment | Score | Delta |
|---|---|---|
| MATCH_HIGHER_BIT=1 | 788,606 | +3.6% |
| COSTCOMPARE_DOWNSTREAM=1 | 779,020 | +2.35% |
| SKIP_DP=1 | 782,522 | +2.81% |
| TNS_SCALE=0.95 | 779,485 | +2.41% |
| TNS_SCALE=1.05 | 780,448 | +2.53% |
| TNS_SCALE=0.9 | 776,637 | +2.03% |
| TNS_SCALE=1.1 | 771,459 | +1.35% |
| TNS_SCALE=1.2 | 772,681 | +1.51% |
| ACCURATE_BANKING=1 | 775,448 | +1.88% |
| TNS_SCALE=0.8 | 768,331 | +0.94% |
| POST_LG_RESYNTH=1 | 767,938 | +0.89% |
| ITER_BANKING=1 | 763,524 | +0.31% |
| BFS_PRE_DP=1 | 762,343 | +0.16% |
| DP_ROUNDS=1 | 761,707 | +0.07% |

**Only improvement: EGR (evaluator-oracle) = -514 (-0.07%)**

**Key findings:**
1. Matching sits at a combinatorial cliff — TNS_SCALE ±5% causes ~2.5% regression. The discrete optimization is hyper-sensitive to edge weights.
2. More conservative banking (TNS_SCALE>1) hurts because it reduces power savings more than it helps TNS.
3. More aggressive banking (TNS_SCALE<1, MATCH_HIGHER_BIT) hurts because timing damage exceeds power+area savings.
4. Post-LG stages ALL regress because they use the 1-hop timing model to make banking decisions.
5. Even BFS-refreshed timing (7% gap vs evaluator, not 34%) still misguides DP.
6. Legalization order is area-first (correct — large cells need priority for fit).
7. LibScoring correctly selects narrowest cells (area > QpinDelay benefit by 200x per cell).

**Why:** The 1-hop timing model is accurate enough for the INITIAL banking pass (current score is the best achievable with our model) but NOT accurate enough for any REFINEMENT. Every refinement uses the model's delta predictions, and systematic underestimation of TNS changes leads to wrong decisions.

**How to apply:** Don't retry model-guided post-banking refinements on tc2 without first closing the evaluator gap. The only path forward is (a) reverse-engineer evaluator's timing model, (b) use evaluator as oracle but fix state corruption, or (c) accept tc2 loss and focus on other cases.
