---
name: MATCH_HIGHER_BIT feature — dangerous, default off
description: 4-bit LEMON matching (MATCH_HIGHER_BIT=1) still regresses TNS-heavy cases even with per-pin CostCompare; root cause is cascading effect, not per-merge accuracy
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
`MATCH_HIGHER_BIT=1` env var enables LEMON max-weight matching for 4-bit (and 8-bit) banking by pairing sourceBit MBFFs (2+2→4, 4+4→8). Added 2026-04-17, default OFF.

Results with per-pin CostCompare + `MATCH_HIGHER_BIT=1`:
| Case | Baseline (per-pin) | + 4-bit matching | Change |
|------|-------------------|-----------------|--------|
| t2_0812 | 788,424 | 980,783 | **+24.4%** |
| hc01 | 31,146,055 | 30,970,384 | -0.56% |
| hc02 | 12,477,578 | 44,213,583 | **+254%** |

**Why it fails:** NOT per-merge accuracy (per-pin CostCompare is correct for individual merges). The root cause is **cascading effect**: each merge changes neighbor FFs' slack, but CostCompare evaluates each merge independently. 4553+ merges individually look profitable but collectively destroy timing. 2-bit matching doesn't suffer because individual 1-bit displacements are smaller and affect fewer pins.

**How to apply:** NEVER set `MATCH_HIGHER_BIT=1` in benchmarks. Fix requires cascading-aware approach: iterative matching (re-evaluate slack after each batch), cascading safety factor, or Method D slack redistribution for better slack prediction.
