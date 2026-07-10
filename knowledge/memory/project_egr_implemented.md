---
name: EGR implemented and tested
description: Evaluator-Guided Refinement shipped on v3_experimental; 3 modes; best result -514 on tc2; state corruption from debank+revert limits practical sweep
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
EGR (Evaluator-Guided Refinement) committed to v3_experimental (7ecc4f4). Env-gated `EGR=1` (default OFF = byte-exact).

**Modes (EGR_INLINE):**
- 0: binary evaluator only (~30s per candidate)
- 1: inline computeInlineCost() only (~0.5s per candidate)
- 2: hybrid — screen with inline, verify top-N with binary

**Key env gates:** EGR_ITERS, EGR_K, EGR_SCREEN_TOP, EGR_MARGIN, EGR_TIME_BUDGET

**Best result:** Mode 2, 500 candidates screened, 14 verified → tc2 760,645 (-514, -0.07%)

**Limitations:**
1. State corruption from repeated debankFF+bankFF cycles (name changes, legalizer drift). 6596-cycle sweep drifted baseline by +2,251.
2. Inline model 7% TNS gap → weak candidate ranking (3/40 hit rate among inline-negative)
3. Binary evaluator 30s/call limits throughput
4. Even perfect EGR only closes ~3% of NTU gap

**How to apply:** Use EGR_INLINE=2 with EGR_ITERS=500 EGR_SCREEN_TOP=20 for practical benefit. Don't enable by default until state corruption is fixed. Don't rely on EGR to close the 17K gap to NTU.
