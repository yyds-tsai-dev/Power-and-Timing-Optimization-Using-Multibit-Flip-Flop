---
name: CG weight experiments dead-end
description: Post-banking CG optimizer resists all weight/convergence modifications; architecture is the bottleneck, not parameters
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
CG optimizer (PostBankingOptimizer) already has per-pin timing-weighted gradients in getWeight(). Attempted modifications all regressed tc2:

- Global criticality scaling (WMAX=5 P=2): +2.74% (overshooting, fixed kAlpha step size)
- Mild scaling (WMAX=2 P=1): +0.31%
- Very mild (WMAX=0.5 P=1): +0.41%
- Mid-CG BFS refresh (CG_BFS_INTERVAL=10): +0.97% (destabilizes convergence)

Root cause: CG uses fixed step size (kAlpha = cell width). Any gradient magnitude change causes overshooting. The CG→legalize transition destroys 39% of gains regardless (8,709 of 22,531 on tc2).

**Why:** The problem is architectural, not parametric. NTU doesn't use CG at all — they use gentle force-model relocation + interleaved legalization.

**How to apply:** Don't attempt further CG tuning. Focus on architectural changes (force model, top-down banking, per-level declustering).
