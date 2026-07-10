---
name: Per-pin CostCompare improvement (commit 7f7d807)
description: Replaced crude MBFF-level displacement with per-pin driver/load HPWL + max(0,-slack) TNS filter; all 7 cases improved -0.02% to -1.41%
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Per-pin CostCompare (2026-04-17, commit 7f7d807) replaces the old crude model:

**Old:** `increaseTNS = DisplacementDelay × HPWL(oldMBFF, newMBFF) × affectNum`
- Treats all pins as equally affected
- No slack buffer absorption (positive-slack FFs penalized same as negative)
- Q-pin delay savings not weighted by α (unit mismatch)

**New:** Per-constituent-FF calculation using actual driver/load positions:
- D-pin: `HPWL(driver, curDpin) - HPWL(driver, newDpin)` — direction-aware
- Q-pin: `HPWL(load, curQpin) - HPWL(load, newQpin)` + Q-delay change
- `max(0, -slack)` filter: only negative-slack pins contribute to TNS
- Q-pin delay change flows through downstream slack (properly weighted)

Results (2-bit matching, `BANKING_MODE=matching PRODUCTION=1`):
| Case | Old | Per-pin | Change |
|------|-----|---------|--------|
| t2_0812 | 799,707 | 788,424 | **-1.41%** |
| hc01 | 31,194,218 | 31,146,055 | **-0.15%** |
| hc02 | 12,587,603 | 12,477,578 | **-0.87%** |
| t1_0812 | 742,555,934 | 740,532,369 | **-0.27%** |
| t3 | 728,538,922 | 728,410,476 | **-0.02%** |
| hc03 | 55,934,849 | 55,878,855 | **-0.10%** |
| hc04 | 728,875,222 | 728,370,159 | **-0.07%** |

**Why:** More merges correctly accepted (t2: 10244 vs 9957 committed) because positive-slack FFs absorb displacement. Graph build ~2x slower (478ms vs 222ms) due to getSlack() calls, but still <1s.
