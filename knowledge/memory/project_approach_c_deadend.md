---
name: Approach C dead-end
description: NTU-style top-down banking + force model failed; 1-hop timing model is root cause; all NTU components individually regress
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
Approach C (Hybrid Top-Down Banking with Force Model) attempted 2026-05-06 on v3_experimental branch. **Dead end.**

**What was tried:**
1. NTU_FLOW master gate + TIMING_PRELOC auto-enable ✅ (committed 94f4798)
2. perLevelDecluster (NTU Eq.5) ✅ (committed 7d14de4, ~100 lines)
3. doTopDown4Bit under NTU_FLOW → hc02 +71.5% (reverted 68f16af)
4. doNTUFlowMatching (full rewrite) → hc02 +101% (not committed, reverted)
5. Combined feature auto-enable → tc2 +15.7% (ALG1_2B +11.8%, ACCURATE_BANKING +1.88%, SCLUSTER_EDGE +0.76%)
6. Force model even gentle (frac=0.1 iter=1) → tc2 +0.89%

**Isolated feature impact on tc2 (baseline 761,159):**
- SCLUSTER_EDGE only: 766,922 (+0.76%)
- ALG1_2B only: 850,858 (+11.8%)
- ACCURATE_BANKING only: 775,448 (+1.88%)
- TIMING_PRELOC frac=0.1: 767,922 (+0.89%)

**Root cause:** 1-hop timing model. CostCompare only sees immediate driver+load slack change. NTU's components (force model, top-down matching, per-level decluster) all change FF positions/groupings in ways that cascade through downstream timing paths. Without multi-hop timing awareness, the matching makes systematically worse decisions after perturbation.

**Why perLevelDecluster catches 0 on hc02:** ΔC = α·(si-sBar) + β·dPower + γ·dArea. Power+area savings from 4-bit banking mask timing damage. sBar=0 for positive-slack FFs. The formula can't see downstream timing ripple.

**What survived:** perLevelDecluster code (committed, dormant), NTU_FLOW gate (committed).

**How to apply:** Don't retry NTU architectural components without first fixing the timing model. Multi-hop BFS propagation or incremental STA is the prerequisite.
