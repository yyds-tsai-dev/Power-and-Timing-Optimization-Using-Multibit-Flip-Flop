---
name: Per-edge Adaptive DIST_BONUS (shipped)
description: Per-edge adaptive DIST_BONUS zeros proximity bias for negative-slack pairs; hc02 -2.18%, t2_MBFF -1.25%, 4 cases improved; v2.1 superseded; SLACK_THRESH=20 gives hc02 -4.83% but hurts t2_MBFF
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Implemented 2026-04-18. Per-edge adaptive DIST_BONUS is the shipped default (v2.1 OFF).

**What it does**: For each matching edge, checks if EITHER FF has negative D-pin slack. If so, DIST_BONUS=0 for that edge (let matching optimize TNS). Otherwise, full DIST_BONUS (prefer nearby for placement quality).

**Results (shipped state)**:
- hc02: **-2.18%** (12,056,721 vs 12,325,446)
- t2_MBFF: **-1.25%** (806,094 vs 816,308)
- t2_0812: **-0.17%**, t3: **-0.05%**
- Area-dominated: +0.00% to +0.12% (hc01 worst)

**Why v2.1 was superseded**: v2.1 drops DIST_BONUS for ALL edges → hurts t2_MBFF (+2.67%). Per-edge adaptive only drops it for timing-critical pairs → both hc02 and t2_MBFF improve.

**Key findings**:
1. Pre-banking cost fractions are useless for predicting post-banking TNS importance (alpha=10, beta=400-200K, gamma=0 for all cases; pre-banking TNS <1% everywhere)
2. SLACK_THRESH=20 gives hc02 -4.83% (best ever) but t2_MBFF +0.01% → case-dependent optimum
3. Per-edge TNS-ratio (deltaTNS/savings) also tried — t2_MBFF -1.35% but t2_0812 +0.41%

**Env vars**: `ADAPTIVE_DIST=0` disables. `SLACK_THRESH=N` sets criticality threshold (default 0). `DECOUPLED_V21=1` enables old v2.1.

**How to apply**: This is the production default. Next step: auto-tune SLACK_THRESH per case, or combine per-edge adaptive with v2.1 window-optimal for critical pairs only.
