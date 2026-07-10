---
name: SLACK_OVERSHOOT_WEIGHT sweep results
description: Phase 3C slack-aware banking penalty sweep 0.1-10.0; case-dependent optimal — hc01 likes w=2.0 (-0.94%), hc02 likes w=0.5 (-2.11%), t2 always worse; needs adaptive weight
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
SLACK_OVERSHOOT_WEIGHT sweep on 2026-04-17 with BANKING_MODE=matching:

| Weight | t2_0812 | hc01 | hc02 |
|--------|---------|------|------|
| 0 (baseline) | 799,697 | 31,194,218 | 12,587,579 |
| 0.1 | -0.006% | **-0.36%** | +1.65% |
| 0.5 | +1.48% | **-0.39%** | **-2.11%** |
| 1.0 | +4.1% | **-0.90%** | -1.65% |
| 2.0 | +9.4% | **-0.94%** | +3.84% |

**Why:** No single weight works for all cases. TNS-dominated (hc01) wants larger weight to protect critical paths. Area-dominated (t2) wants zero — slack penalty makes banking too conservative, reducing area savings.

**How to apply:** Needs adaptive weight (auto-tune based on TNS/area ratio) or per-FF weight. Deferred for now — revisit when tackling case-adaptive optimization. The code path is ready in CostCompare (Phase 3C).
