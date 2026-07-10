# Deep Research: Optimization Opportunities to Beat NTU

**Date**: 2026-05-03  
**Type**: Research synthesis  
**Goal**: Close tc2 (+3.09%) and hc04 (+0.16%) gaps vs NTU  

---

## Current Gap Summary

| Case | Our Score | NTU Score | Gap | Dominant Factor |
|------|-----------|-----------|-----|-----------------|
| tc2 | 761,159 | 738,400 | **+3.09%** | TNS (7,850 vs 5,223) |
| hc04 | 728,024K | 726,900K | **+0.16%** | TNS |
| tc1 | 738,014K | 738,800K | **-0.11%** ✓ | — |
| tc3 | 728,024K | 728,800K | **-0.11%** ✓ | — |
| hc01-03 | winning | — | ✓ | — |

The tc2 gap is entirely TNS-driven: our evaluator TNS = 7,850 vs NTU's 5,223 (50% higher).

---

## NTU's Methodology (P7 DAC'25 LBR)

Three-stage framework, iterated from k_max→k_min:

### 1. Force-Model FF Relocation (Eq. 2-3)
```
F_x(i) = Σ_{p ∈ P_crit} sgn(x'_{p,i} - x_{p,i})   [only when slack(p) < 0]
```
Simple unit forces along critical paths. Iterates until convergence. Moves FFs toward timing-optimal positions BEFORE banking.

### 2. S_cluster Priority Scoring (Eq. 4)
```
S_cluster(i) = (L_{2k}(i) - L_k(i)) + c1 · sigmoid(-slack(i) / c2)
```
Determines clustering ORDER: FFs in dense regions (small L_2k - L_k) with negative slack (high sigmoid) get clustered first. Sequential processing in descending score order.

### 3. Multi-Objective Declustering (Eq. 5)
```
ΔC(i) = α·(s(i) - s̄) + β·ΔPower(i) + γ·ΔArea(i)
```
After clustering + legalization, debank MBFFs where ΔC < 0. The `s̄` reference slack is for lower-bit FFs — creates a relative threshold.

### 4. Refinement
(1) Swap critical-path FFs with nearby ones, (2) decluster + recluster, (3) move FFs from congested bins.

### 5. Top-Down k_max→k_min
Repeat steps 1-3 for each bit-width level descending. This is fundamentally different from our 2-bit matching → greedy 4-bit fallback.

---

## Research Findings — Ranked by Expected Impact

### Tier 1: Highest Impact, Directly Addresses tc2 Gap

#### A. Criticality-Weighted CG Net Weighting (DREAMPlace 4.0)
**Source**: Liao et al., "DREAMPlace 4.0," IEEE TCAD 2023  
**Idea**: After each CG pass, run STA, compute per-net criticality = max(0, -slack/max_neg_slack), set w_net = 1 + W_max · criticality^p (p=2-3, W_max=5-10). Re-run CG with updated weights.  
**Why it matters**: Our CG optimizer uses uniform net weights. Critical nets get the same gradient as non-critical ones. DREAMPlace 4.0 reports **46.8% TNS improvement** from this alone.  
**Integration difficulty**: **LOW**. Our log-sum-exp HPWL gradient already operates per-net. Multiply by per-net weight. The STA feedback is the only new piece — can use existing `getSlack()`.  
**Expected tc2 impact**: High. CG saves 22,531 cost on tc2 → with timing-weighted nets, critical paths get stronger attraction, reducing TNS at source.

#### B. Force-Model Pre-Relocation (NTU Eq. 2-3)
**Source**: NTU P7 DAC'25 LBR  
**Idea**: Before banking, apply unit forces along critical paths to push FFs toward timing-optimal positions. Only fires when path slack < 0.  
**Why it matters**: Our MeanShift is purely spatial. Critical FFs can get pulled away from drivers/loads. NTU's force model is the simplest possible correction — just sign function, no magnitude calibration needed.  
**Integration difficulty**: **LOW**. Add a new pipeline stage after SlackRedist, before Banking. Iterate ~3-10 times. Each FF moves by Σ sgn(direction-to-reduce-path-length) for negative-slack paths.  
**Expected tc2 impact**: Medium-high. Directly addresses displacement-induced TNS.

#### C. Incremental Dirty-Flag BFS at Banking Commit Time
**Source**: OpenTimer v2 (Huang & Wong, TCAD 2021), iTimerC 2.0 (Li et al., ICCAD 2015)  
**Idea**: After each banking move, mark affected FF's output net dirty, BFS-propagate arrival corrections through downstream cone only. Skip if delta < threshold. Maintains persistent dirty flags.  
**Why it matters**: Our 1-hop CostCompare underestimates TNS by 34% on tc2. `refreshArrivalCorrections()` closes 74% of the gap but becomes stale after moves. Lazy incremental propagation keeps corrections fresh.  
**Integration difficulty**: **MEDIUM**. Need to maintain dirty-flag set, run mini-BFS after each banking commit. Can reuse existing `refreshArrivalCorrections()` infrastructure but scope it to modified cones.  
**Expected tc2 impact**: Medium. More accurate cost → better banking decisions → lower true TNS.

### Tier 2: Medium Impact, Addresses Legalization Destruction

#### D. Weighted-Abacus Legalization
**Source**: Spindler et al., "Abacus," ISPD 2008; Puget & Flach, "Jezz," SBCCI 2015; Brenner & Vygen, ISPD 2004  
**Idea**: Replace Tetris with Abacus: sort cells by x-position, for each cell try each row with DP-based compaction minimizing weighted squared displacement. Weight critical cells higher → they stay near CG-optimized positions.  
**Why it matters**: Legalization destroys 39% of CG timing gains on tc2 (8,709 of 22,531). Abacus gives 30% less displacement than Tetris. With criticality weighting, critical FFs get near-zero displacement.  
**Integration difficulty**: **MEDIUM-HIGH**. ~200-line core DP per row, but need to replace the current Tetris infrastructure.  
**Expected tc2 impact**: Medium. 30% less displacement × 39% destruction rate = ~12% recovery → ~1,000 cost units.

#### E. S_cluster Edge Reweighting (Plan Module 1)
**Source**: NTU Eq. 4  
**Idea**: Embed timing criticality into matching edge weights: `adjGain = gain * (1 + BOOST * (1 - avg_urgency)) + dist_bonus * (1 - DAMPEN * max_urgency)`. Dense + slack-rich pairs get boosted; critical pairs get dampened distance bonus (stay close).  
**Integration difficulty**: **LOW**. Modify ~20 lines in Banking.cpp edge weight loop.  
**Expected tc2 impact**: Medium. Better matching decisions → fewer harmful merges on critical paths.

### Tier 3: Lower Impact, Longer-Term

#### F. Top-Down k_max→k_min Matching (Plan Module 3)
**Source**: NTU framework  
**Idea**: Full LEMON matching at each bit-width level (4→2→1) instead of 2-bit matching + greedy 4-bit.  
**Risk**: MATCH_HIGHER_BIT caused +254% hc02 regression. Need strengthened safety margins.  
**Integration difficulty**: **HIGH**. Restructures the banking pipeline.

#### G. Lagrangian-Based Timing DP (DREAMPlace 4.0)
**Source**: Liao et al., TCAD 2023  
**Idea**: Lagrangian multipliers propagated in reverse topological order encode timing criticality for DP move acceptance.  
**Integration difficulty**: **HIGH**. Requires Lagrangian update loop around existing DP.

#### H. ALNS / ILS for Escaping Local Optima
**Source**: Ropke & Pisinger 2006; Arya et al., SIAM 2004  
**Idea**: ILS: perturb (debank k random MBFFs) → local search → accept/reject. ALNS: adaptive destroy/repair with SA acceptance. Arya p-swap: simultaneously swap p facilities.  
**Integration difficulty**: **LOW** (ILS wraps existing infrastructure).  
**Expected impact**: 0.5-1% TNS improvement. Best as a final polish step.

#### I. Chen ISPD 2024 Slack Redistribution
**Source**: Chen et al., ISPD 2024  
**Idea**: Timing-feasible region expansion via inter-path slack transfer using interval graphs on red-black trees.  
**Integration difficulty**: **MEDIUM-HIGH**. Already attempted as Plan Module 4 / Method D. Previous results were negative (tc2 +1.55%).

---

## Recommended Implementation Order

Based on impact/effort ratio and tc2 gap analysis:

### Phase 1 (immediate, 1-2 days each)
1. **A: CG Net Weighting** — lowest effort, highest ceiling. Add `w_net = 1 + W_max * crit^p` to PostBankingOptimizer. Sweep W_max ∈ {3,5,10}, p ∈ {2,3}.
2. **B: Force-Model Pre-Relocation** — NTU's simplest innovation. New stage between SlackRedist and Banking. Iterate sign-function forces on negative-slack paths.
3. **E: S_cluster Edge Reweighting** — modify Banking.cpp edge weights with timing urgency. ~20 lines.

### Phase 2 (3-5 days)
4. **C: Incremental BFS at Commit** — maintain dirty flags, mini-BFS after banking commits.
5. **D: Weighted-Abacus Legalization** — replace Tetris sort with criticality-weighted Abacus.

### Phase 3 (1-2 weeks)
6. **F: Top-Down Matching** — only after hc02 safety net is solid.
7. **H: ILS/ALNS** — final polish layer wrapping all prior improvements.

---

## Key Insight

NTU's advantage is NOT algorithmic sophistication — it's **timing integration at every stage**. Their force model, S_cluster priority, and declustering criterion all use timing information. Our pipeline does great on power/area but treats timing as a side constraint rather than a primary objective. The fix is not one big change but making every existing stage timing-aware: CG weights, banking priorities, legalization ordering, DP acceptance.
