---
name: Unbank+Rebank thesis direction
description: Post-LG debank+rematch (v1 targeted, v2 global) tested; v2 catastrophically regressed; predictMBFFCost too crude to drive decisions at scale
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Post-LG unbank+rebank explored as thesis direction (Method ?, after Method D shipped but didn't fit).

**v1 (UNBANK_REBANK=1)** — targeted: only 4-bit MBFFs, try 3 pairings into 2x 2-bit, ΔC<0 commit.
- tc2: 771385 → 771666 (+0.04%, regression). 6/3340 committed, predicted gain 420.
- hc02: 12056721 → 12015008 (-0.346%, real win). 2/4407 committed, predicted gain 60001.
- Verdict: tiny signal, partial false-positive predictor (tc2 said gain but actual loss).

**v2 (UNBANK_REBANK_V2=1)** — global: debank ALL MBFFs, LEMON max-weight matching at post-LG coords.
- tc2: 771385 → 1140157 (+47.8%, catastrophic). 10347 committed, predicted gain 685K, actual loss 369K.
- hc02: 12056721 → 28205091 (+133.9%, catastrophic). 10566 committed, predicted gain 64M, actual loss 16M.
- Verdict: predictor breaks at scale. Decision sign correctness ≠ magnitude correctness.

**Why:** predictMBFFCost = α·TNS + β·power + γ·area only considers FFs IN cluster. Misses:
1. Downstream fanout slack cascade (D-pin slack of all loads driven by this FF's Q-pin)
2. Bin density (λ term) entirely absent
3. Cross-cluster wire HPWL secondary effects

**How to apply:** 
- Post-LG re-banking is NOT a viable thesis direction in current architecture without a much better predictor.
- If revisiting, build a full per-net incremental TNS calculator (touch every fanout edge of moved FF) before scaling to >10 commits.
- Pre-LG matching improvement (NTU outer loop, S_space + N3) remains the more tractable direction — it operates on fresh state where predictor is actually validated.
