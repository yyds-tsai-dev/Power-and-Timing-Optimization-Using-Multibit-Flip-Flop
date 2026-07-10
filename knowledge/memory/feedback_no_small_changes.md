---
name: No more small changes or sweeps
description: User explicitly says incremental tuning has hit its ceiling; commit to architectural changes only
type: feedback
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
Stop making small parametric changes and parameter sweeps. The upper bound of incremental tuning is limited (~0.1-0.3% per change) while the gap to NTU is 3.09% on tc2. 20+ parametric experiments confirmed we're at a local optimum.

**Why:** CG weight experiments (5 variants, all regressed), BFS convergence (regressed), and every prior sweep yielded marginal gains. 100 small changes won't close a 3% gap.

**How to apply:** Go directly to architectural-level changes (force model, top-down banking, per-level declustering). Don't propose parameter sweeps or env-gate tuning as the primary approach. Bold structural moves > cautious incremental tuning.
