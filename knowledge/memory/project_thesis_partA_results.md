---
name: project-thesis-parta-results
description: "Thesis Ch.5 Part-A experiment numbers, freshly evaluator-verified 2026-06-17 in a single reproducible config; all 7 beat contest-time top-3"
metadata: 
  node_type: memory
  type: project
  originSessionId: e10d56da-a5c4-4d0f-a839-3a76f0ba3cb4
---

Thesis §5.4/§5.5 experiment data, run 2026-06-17 on the V3 production binary (`cadb_0015_final`), every score from the real `evaluator/preliminary-evaluator`, all legal (Check pass). **Single reproducible config** (not the README's per-case best-of-serial/dyna1/dyna2).

**Config**: `OMP_NUM_THREADS=8 BANKING_MODE=matching PRODUCTION=1 INCR_RELOC=1 RELOC=1 CRIT_SWAP=1 BIT_REPAIR=1 BIT_REPAIR_DYNA=1`, default budgets (RELOC_TIME=120, CRIT_SWAP_TIME=150, BIT_REPAIR_TIME=400) for 6 cases; **tc2 uses extended budget** (BIT_REPAIR_TIME=1200 BIT_REPAIR_ROUNDS=60 RELOC_TIME=200) → 745,973.

**§5.4 main table** (baseline=no-refine `BANKING_MODE=matching PRODUCTION=1`; proposed=above; top-3=min Team1/2/3 from ntu_baseline.md):
| case | baseline | proposed | contest top-3 | Δ vs top-3 |
|---|--:|--:|--:|--:|
| tc1 | 737,272,707 | 735,966,287 | 739,200,000 | −0.44% |
| tc2 | 761,159 | **745,973** | 748,000 | −0.27% |
| tc3 | 728,024,317 | 727,352,842 | 729,300,000 | −0.27% |
| hc01 | 30,615,486 | 30,424,226 | 31,510,000 | −3.45% |
| hc02 | 11,614,120 | 11,156,227 | 13,280,000 | −15.99% |
| hc03 | 55,870,606 | 55,849,822 | 55,940,000 | −0.16% |
| hc04 | 728,092,959 | 727,425,032 | 728,700,000 | −0.18% |
**All 7 beat contest-time top-3, no regression.** tc2 at default budget = 749,427 (just ABOVE 748,000); needs extended budget to reach 745,973.

**§5.5 ablation tc2** (cumulative, default budget; Full=extended): base 761,159 → RELOC 760,494 → CRIT_SWAP 759,899 → BIT_REPAIR-serial 755,168 → BIT_REPAIR-dynasearch 749,427 → Full 745,973 (−15,186, −2.0%). BIT_REPAIR is the dominant operator; dynasearch beats serial by −5,741 at the same budget.

**Greedy vs matching** (banking-only, no refine; matching improvement over greedy): tc1 −0.41%, tc2 −5.21%, tc3 −0.13%, hc01 −4.50%, hc02 −20.32%, hc03 −0.03%, hc04 +0.00% (greedy negligibly better). **Avg −4.4%, max −20.3% (hc02).** Greedy = `BANKING_MODE=greedy`.

**Budget curve**: tc2 dynasearch internal multi-hop TNS vs BIT_REPAIR wall-clock, from tc2_ext log: 6824@38s → 5787@1120s, monotone with diminishing returns.

**45× throughput**: 582s→13s full-recompute round (already in thesis).

ρ (RISK_SCALE) default = 10. Risk-adaptive vs fixed-bonus (filled from prior [[project_risk_adaptive_dist]] verified run, NOT re-run this session: tc2 −0.87%, hc02 −1.34%) — re-verify when convenient.

Outputs in /tmp/exprun/. See [[project_evaluator_vs_internal]] for the contest top-3 source.
