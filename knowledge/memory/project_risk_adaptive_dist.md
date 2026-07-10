---
name: Risk-Adaptive DIST_BONUS default
description: RISK_ADAPTIVE=true RISK_SCALE=10 shipped as new default; tc2 -0.87%, hc02 -1.34%, tc1 -0.08%; supersedes simple slack<0 mode
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
Risk-adaptive DIST_BONUS mode is now the DEFAULT in Banking.cpp. Zeroes DIST_BONUS when `slack < DisplacementDelay * dist / riskScale` (distance-dependent threshold, more physical than blanket slack<0).

**Why:** Simple slack<0 mode was too blunt — zeroed bonus for ALL negative-slack pairs regardless of distance. Risk mode allows close pairs with slightly negative slack to still get the proximity bonus, while blocking distant pairs that would cause large displacement on critical paths.

**How to apply:** Default is ON. Set `RISK_ADAPTIVE=0` to restore old slack<0 mode (byte-exact fallback). Tune with `RISK_SCALE=N` (default 10; lower=more aggressive zeroing). RS=10 is optimal for tc2; don't change without full 7-case sweep.

**Results (evaluator scores, 2026-04-27):**
| Case | Old (slack<0) | New (risk, RS=10) | Delta |
|------|---------------|-------------------|-------|
| tc1 | 738,510,530 | 737,931,323 | -0.08% |
| tc2 | 768,315 | 761,617 | -0.87% |
| tc3 | 727,909,743 | 728,024,313 | +0.016% |
| hc01 | 30,874,248 | 30,874,248 | 0.00% |
| hc02 | 11,831,829 | 11,673,921 | -1.34% |
| hc03 | 55,862,356 | 55,871,128 | +0.016% |
| hc04 | 727,854,085 | 728,156,005 | +0.041% |

tc2 gap to NTU: 4.05% → 3.14%.
