# Research Report: Closing the 4.05% tc2 Gap to NTU

*Generated: 2026-04-26 | Sources: 12 | Confidence: High*

## Executive Summary

NTU (ICCAD 2024 1st place, team cadb0027) outperforms us on tc2 by 4.05% (evaluator 768,315 vs 738,400). Their DAC 2025 LBR paper reveals a **top-down k_max→k_min iterative framework** with three mechanisms we lack: (1) timing-driven force relocation before each clustering level, (2) S_cluster scoring to control clustering order, and (3) per-level ΔC declustering. For tc2 (α=10, area+power = 93% of score), the key is more aggressive MBFF merging — especially 4-bit — with safety mechanisms to bound TNS damage. The single most impactful implementable technique is **NTU's top-down clustering order with per-level ΔC safety**.

## 1. NTU's Winning Methodology (P7 DAC 2025 LBR)

NTU's framework from the paper ([P7_MOMBFFPrePlaced_DAC25LBR](../../papers/text/P7_MOMBFFPrePlaced_DAC25LBR.txt)):

**Stage A: Preprocessing**
- Debank ALL MBFFs into SBFFs
- Mark cell library styles: low delay, low power, small area, unique aspect ratios

**Stage B: Iterative k_max → k_min clustering** (repeat for k = 4, 2, 1):
1. **FF Relocation** — timing-driven force model:
   - F_x(i) = Σ F_{p,x}(i) for all critical paths p of FF i
   - F_{p,x}(i) = sgn(x'_{p,i} - x_{p,i}) if path p's slack < 0, else 0
   - Simple sign-of-distance force from each neg-slack path endpoint
   - Iterate until convergence

2. **k-bit Clustering + Legalization**:
   - Score each FF: S_cluster(i) = (L_{2k}(i) - L_k(i)) + c1·σ(-slack(i)/c2)
   - L_k(i) = HPBB of k-closest SBFFs to FF i
   - Cluster in DESCENDING S_cluster order (dense + slack-rich first)
   - Legalize immediately, adjusting nearby cells if needed

3. **MBFF Declustering** — per-level safety:
   - ΔC(i) = α·(s(i) - s̄) + β·ΔPower(i) + γ·ΔArea(i)
   - s(i) = additional negative slack from FF i
   - s̄ = reference slack for lower-bit FFs
   - Debank if ΔC(i) < 0

**Stage C: Refinement**
- Swap critical-path FFs with nearby ones
- Decluster + recluster for balance
- Move FFs from congested bins

NTU explicitly states: "the 2nd-place team [us] faced difficulties with testcase2."

## 2. Why tc2 Is Our Worst Case

tc2 cost breakdown (NTU):
- α·TNS = 10 × 5,223 = 52,230 (7% of 738,400)
- β·Power + γ·Area = 686,170 (93% of score)

**Area+power dominate**. More aggressive MBFF merging directly reduces both. The gap of 29,915 can mostly be closed by merging more FFs into MBFFs.

Our approach (2-bit first, greedy 4-bit) vs NTU (4-bit first, then 2-bit):
- We match 2-bit MBFFs first, locking pairs that could form better 4-bit groups
- Our greedy 4-bit fallback finds suboptimal groupings
- NTU's top-down approach finds optimal 4-bit groups from fresh SBFFs

## 3. Key Algorithmic Differences

| Feature | Our V3 | NTU |
|---------|--------|-----|
| Matching order | 2-bit → greedy 4-bit | 4-bit → 2-bit → 1-bit |
| Pre-relocation | MeanShift (spatial only) | Force model (timing-driven) |
| Clustering priority | None (LEMON global opt) | S_cluster score ordering |
| Safety mechanism | PostLG ΔC check (one-shot) | Per-level ΔC (iterative) |
| Cell selection | Global libScoring | Per-aspect-ratio marking |

## 4. Why MATCH_HIGHER_BIT Failed (But Top-Down Should Work)

Our MATCH_HIGHER_BIT=1 tried 4-bit by pairing EXISTING 2-bit MBFFs. This fails because:
1. 2-bit MBFFs are already committed, positions locked
2. Pairing two 2-bit MBFFs compounds displacement
3. No per-merge safety → cascading timing damage

NTU's top-down is fundamentally different:
1. 4-bit clustering on FRESH 1-bit SBFFs (maximum flexibility)
2. Per-merge ΔC check prevents bad merges
3. Unmerged FFs fall through to 2-bit level

## 5. Recommended Implementation

**Technique: Top-Down 4-Bit Clustering with Per-Merge Safety**

Before 2-bit matching, run an additional pass that:
1. Groups 4 nearby SBFFs into 4-bit MBFF candidates
2. Evaluates each candidate with CostCompare (using actual legal position)
3. Only commits groups where CostCompare gain > safety_margin
4. Remaining SBFFs proceed to normal 2-bit matching

Implementation approach using existing infrastructure:
- Use R-tree to find k-nearest SBFFs
- For each FF, find best group of 4 nearby same-clock FFs
- Score groups by CostCompare gain (α·ΔTNS + β·ΔPower + γ·ΔArea)
- Greedy commit in descending gain order (high-gain groups first)
- Per-merge ΔC check with strengthened margin
- Env gate: `TOP_DOWN_4BIT=1` (default OFF)

Expected impact: -2% to -4% on tc2 (area+power dominated)
Risk: Medium — per-merge safety should prevent hc02 regression

## Sources

1. [NTU DAC 2025 LBR](../../papers/text/P7_MOMBFFPrePlaced_DAC25LBR.txt) — NTU's winning methodology
2. [ICCAD 2024 Contest Winners](https://www.iccad-contest.org/2024/Winners.html) — team rankings
3. [coherent17 HackMD](https://hackmd.io/@coherent17/Hk1CdKACa) — baseline codebase notes
4. [Revisit MBFF ASP-DAC 2025](../../papers/text/P1_RevisitMBFF_ASPDAC25.txt) — early-stage clustering
5. [Capacitated K-Means GLSVLSI 2024](../../papers/text/P13_CapacitatedKMeans_DAC24.txt) — scalable MCF approach
6. [Slack Redistribution ISPD 2024](../../papers/text/P19_SlackRedistClustering_ISPD24.txt) — slack budgeting + force model
7. [Analytical FF Binding TCAD 2025](../../papers/text/P21_AnalyticalFFBinding_TCAD25.txt) — in-placement binding
8. [ICCAD 2024 Contest Problem B paper](https://dl.acm.org/doi/10.1145/3676536.3689911)
9. [Binding MBFF DAC 2024](https://dl.acm.org/doi/10.1145/3649329.3658264) — DTCO cell library
10. [Power-driven FF merging ISPD 2011](https://dl.acm.org/doi/abs/10.1145/1960397.1960423)

## Methodology

Searched 15+ queries across web (WebSearch) and local paper corpus. Analyzed NTU thesis, DAC 2025 LBR, contest winners page, 6 related papers. Cross-referenced with codebase Banking.cpp architecture.
