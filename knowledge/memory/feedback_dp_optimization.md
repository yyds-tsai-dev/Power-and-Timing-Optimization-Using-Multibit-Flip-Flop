---
name: Detail Placement optimization findings
description: DP stage has large optimization potential; ChangeCell had OMP race, GlobalSwap had crude criterion + rtree bug; K-nearest search is very effective
type: feedback
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Detail Placement has significant untapped optimization potential. Key findings from 2026-04-17:

**ChangeCell OMP race condition:** Original code used `#pragma omp parallel for` but `setCell()` + `getCost()` reads neighbor state through nextStage connections, causing data races. Fix: single-threaded. Effect: t2_0812 -1.7%, hc02 -0.6%.

**GlobalSwap crude acceptance criterion:** Original used `displacement × fanout` which ignores the actual cost function. Fix: tentative swap + `getCost()` (α·TNS + β·Power + γ·Area) comparison. Effect: t2_0812 -2.0%, hc02 -0.7%.

**GlobalSwap rtree bug:** Original only removed target's old position, left current FF's stale entry, never inserted target's new position. Fix: bidirectional remove + insert.

**K-nearest search is the biggest win:** Querying K=5 neighbors instead of 1, selecting best swap partner. Effect: hc02 -5.84%, hc01 -0.78%, t2_MBFF -0.81% (vs K=1). K sweep 5-12 pending.

**How to apply:** When modifying DP code, always verify with manual evaluator (not getEvaluatorCost). DP improvements are orthogonal to Banking and stack additively.
