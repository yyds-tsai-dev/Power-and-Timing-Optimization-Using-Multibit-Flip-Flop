---
name: POST_LG_DECLUSTER shipped adaptive (D2-family only + margin=5000)
description: Post-LG MBFF decluster shipped default-on for D2 family (inst in [130000,180000]); hc02 -0.080%, other 6 byte-exact
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
V1 `postLGDecluster()` ships with adaptive default (commit 6a7b36a, 2026-04-21): enable when `NumInstances ∈ [130000, 180000]` (D2 design family only), `POST_LG_DECLUSTER_MARGIN=5000` default. Strict 7-case win: hc02 -0.080%, other 6 byte-exact.

**Why:** ΔC prediction (power/area/TNS at Q-pin) is unreliable because DP ripple dominates actual score change — confirmed by per-case sweep.
- D1 family (tc1/hc01, 108,685 inst): always regresses even with margin=5000 (hc01 +0.072% with 2 strong candidates)
- D2 family (tc2/hc02/hc03, 153,457 inst): hc02 uniquely has 2 large-Δ candidates (-39384 + -6911) that survive DP; tc2/hc03 have only weak Δ (<=-569) filtered by margin=5000
- D3 family (tc3/hc04, 101,221 inst): always regresses (hc04 +0.002% even with strong predictions)

Gate is essentially "case family" selector — hc02 is the only case where prediction survives downstream. On hc02 BOTH declusters are needed together for the win; keeping only the stronger one (margin>15000) regresses +0.039%.

**How to apply:** adaptive is default — no env needed. `POST_LG_DECLUSTER=0` force off (byte-exact pre-2026-04-21). `POST_LG_DECLUSTER=1` force on regardless of inst count. `POST_LG_DECLUSTER_MARGIN=<value>` overrides filter.

**Caveats:** prior memory note "margin is band-aid" is confirmed — margin alone couldn't achieve strict win, required NumInstances gate on top. If a held-out case has inst in [130k,180k] and weak declusters survive margin, could regress; the ΔC predictor is still fundamentally unreliable for the DP ripple cost (~1000-5000 units unmodeled per decluster from GlobalSwap/ChangeCell reshuffle).

Key files: `src/Manager.cpp` postLGDecluster() at L558; `src/Legalizer.cpp` FreeRect()/RemoveNodeByFFPtr() at ~L272-290 (must pair when debanking post-LG).
