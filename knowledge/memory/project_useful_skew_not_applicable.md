---
name: Useful skew not applicable to ICCAD Problem B
description: ICCAD 2024 Problem B uses ideal clock (zero skew) — getSlack() has no clock arrival term, so useful skew optimization cannot improve scores
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Investigated 2026-04-17. The timing model in `FF::getSlack()` is:
```
newSlack = originalSlack + delta_Qdelay + DisplacementDelay × delta_HPWL
```
No clock arrival time term — the problem assumes ideal clock distribution (zero skew). Useful skew requires non-zero clock skew to redistribute timing margin between stages.

**Why:** ICCAD Problem B's cost function uses `α·TNS + β·Power + γ·Area`, where TNS is computed from D-pin slack without any clock network model.

**How to apply:** Do NOT attempt useful skew optimization for this problem. Focus on displacement-aware and power/area-aware optimizations instead.
