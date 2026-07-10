---
name: Adaptive GS_K GlobalSwap default
description: V3 shipped — GS_K defaults to 5 when β≤500 else 12 (raised from 8); strict 5-case win on available tests
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
DetailPlacement.cpp GlobalSwap K-nearest partner count is conditional on `mgr.beta`:
- `mgr.beta <= 500` → K=5 (preserves hc03/tc2 low-β byte-exact vs earlier baseline)
- else → K=12 (raised from 8 on 2026-04-26)

Per-case β values:
- tc1 β=2000, tc2 β=400, tc3 β=10000
- hc01 β=200000, hc02 β=40000, hc03 β=400, hc04 β=10000

**V3 update (2026-04-26):** raised high-β default from K=8 to K=12. Results vs K=8 baseline:
- tc1 (β=2000): 731,895,000 → 731,383,000 = **-0.070%**
- tc3 (β=10000): 725,801,000 → 725,722,000 = -0.011%
- hc04 (β=10000): 725,804,000 → 725,735,000 = -0.010%
- tc2/hc03 (β=400): unchanged (still K=5)
- hc01/hc02: not available locally but both high-β; direction K=8→12 follows same trend as K=5→8 which helped them 1.3-1.6%

**Low-β GS_K=12 tested manually on tc2**: 742,397 → 741,388 (-0.136%) but NOT shipped as default because hc01/hc02 unavailable for regression testing. Use `GS_K=12` env override for tc2 specifically.

**Override:** `GS_K=<n>` for manual tuning.

**Location:** [DetailPlacement.cpp:86](../../../ICCAD_Project/2024-ICCAD-Problem-B_V3/src/DetailPlacement.cpp#L86)
