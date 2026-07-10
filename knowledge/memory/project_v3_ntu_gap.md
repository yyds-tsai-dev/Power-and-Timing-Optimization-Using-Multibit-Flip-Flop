---
name: V3 vs NTU current gap analysis
description: V3 beats NTU on 4/5 available cases as of 2026-04-26; tc2 only +0.38-0.54% behind, gap is purely TNS (705 units)
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
V3 vs NTU gap as of 2026-04-26 (5 available cases):

| Case  | V3 Score     | NTU Score    | Gap     | Win? |
|-------|-------------|-------------|---------|------|
| tc1   | 731,383,000 | 738,800,000 | -1.00%  | WIN  |
| tc2   | 742,397*    | 738,400     | +0.54%  | LOSE |
| tc3   | 725,722,000 | 728,800,000 | -0.42%  | WIN  |
| hc03  | 55,812,000  | 55,920,000  | -0.19%  | WIN  |
| hc04  | 725,735,000 | 726,900,000 | -0.16%  | WIN  |

*tc2 with GS_K=12 override: 741,171 (-0.165%), gap narrows to +0.38%

**Why:** The original plan (adaptive-brewing-donut) stated tc2 gap as +4.46%. Multiple improvements shipped since then (adaptive MATCH_K, adaptive GS_K, adaptive decluster) closed most of it.

**tc2 cost anatomy** (α=10, β=400, γ=8e-7, λ=10000):
- Area: 557,131 (75.04%)
- Power: 125,990 (16.97%)
- TNS: 59,276 (7.98%)
- Bin: 0 (0%)

NTU's tc2 TNS=5,223 vs our 5,928. Entire 3,997 gap is from TNS.

**How to apply:** tc2 is the only remaining losing case (on available tests). TNS reduction of 705 units closes the gap. Post-LG corrections all hurt due to DP ripple. GS_K=12 + DP_SLOT_ASSIGN=3 manually gives 741,171 but not shipped as default.

**Dead ends tested for tc2 this session:**
- POST_LG_RESYNTH: +0.88% regression (DP ripple)
- MATCH_HIGHER_BIT (4-bit): +0.84% (double-height cells cause massive legalization displacement)
- COMMIT_NEG_THRESH (accept negative-gain pairs): every threshold worsens
- Safety margin sweeps on 4-bit: no setting helps
