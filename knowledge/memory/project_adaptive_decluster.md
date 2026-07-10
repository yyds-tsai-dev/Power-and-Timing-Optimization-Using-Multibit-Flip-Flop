---
name: Adaptive postLGDecluster for low-β
description: V3 shipped adaptive postLGDecluster — β≤500 enables with margin=55; tc2 -0.138%, hc03 -0.001%, 3 cases byte-exact
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
Adaptive postLGDecluster shipped in V3 (2026-04-26).

**Why:** Low-β cases (β≤500, e.g. tc2 β=400, hc03 β=400) have timing-dominated cost where the CostCompare prediction model is reliable enough for decluster decisions. High-β cases have DP ripple that makes predictions unreliable.

**How to apply:** `postLGDecluster()` in Manager.cpp auto-enables for β≤500 with margin=55. High-β cases stay OFF (byte-exact). Override with `POST_LG_DECLUSTER={0,1}` and `POST_LG_DECLUSTER_MARGIN=N`.

**Results (5 available cases):**
- tc2 (β=400): 743,426 → 742,397 = **-0.138%** (4 MBFFs declustered)
- hc03 (β=400): 55,816,400 → 55,815,900 = -0.001% (1 MBFF declustered)
- tc1/tc3/hc04 (β≥2000): byte-exact (OFF)

**Also implemented but default OFF:** `COMMIT_ORDER=1` (gain-descending commit) gives tc2 -0.059% standalone but regresses other cases 0.01-0.03%. Stacks with decluster for -0.181% on tc2 but not a strict multi-case win.
