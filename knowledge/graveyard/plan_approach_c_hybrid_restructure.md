# Approach C: Hybrid Top-Down Banking with Force Model

**Date**: 2026-05-05
**Type**: Architecture design spec
**Goal**: Close tc2 +3.09% and hc04 +0.16% gaps vs NTU by restructuring the pipeline

---

## 1. Problem Statement

We lose to NTU on tc2 by 3.09% (761,159 vs 738,400), entirely TNS-driven (7,850 vs 5,223).
Parametric tuning is exhausted (20+ experiments, CG weight scaling all regressed).
The gap is architectural, not parametric.

### NTU's Architecture (P7 DAC'25 LBR)
```
debank all → preprocess
for k = k_max down to k_min:
    force_relocate(critical paths)         // Eq.2-3
    cluster_k_bit + legalize(S_cluster)    // Eq.4
    decluster(ΔC < 0)                      // Eq.5
refinement (swap, recluster, bin fix)
```

### Our Current Architecture
```
parse → libScoring → preprocess → preLegalize → slackRedist → timingPreReloc(dormant)
→ banking(2-bit matching → greedy 4-bit, one-pass)
→ postBankingOpt(CG) → legalize → postLGDecluster → unbankRebank
→ unbankRebankGlobal → postLGResynth → iterBanking → DP → dump
```

### Key Architectural Gaps
1. **No top-down iteration**: We do 2-bit matching + greedy 4-bit in one pass. NTU iterates k_max→k_min with legalization and declustering at each level.
2. **No timing-driven pre-relocation**: Force model exists but is dormant (`TIMING_PRELOC=1`). NTU runs it before each clustering level.
3. **CG→legalize destruction**: CG saves 22,531 cost on tc2, legalization destroys 8,709 (39%). NTU avoids this by legalizing during clustering.
4. **No per-level declustering**: We have post-LG decluster but it's default-off and crude. NTU declusters immediately after each bit-width level.

---

## 2. Proposed Architecture (Approach C)

```
parse → libScoring → preprocess → preLegalize → slackRedist
→ forceRelocate()                                    // EXISTING (dormant), activate
→ banking_v2():                                      // RESTRUCTURED main loop
    for k = k_max (4) down to k_min (2):
        doMatchingClustering(k-bit)                   // REUSE existing LEMON matching
        perLevelDecluster(k-bit, ΔC threshold)        // NEW: NTU Eq.5
→ postBankingOpt(CG)                                  // KEEP as-is (less critical now)
→ legalize → postLGDecluster → unbankRebank
→ unbankRebankGlobal → postLGResynth → iterBanking → DP → dump
```

### 2.1 Component: Force-Model Relocation (activate existing)

**Status**: Already implemented at `Manager.cpp:1745-1870` behind `TIMING_PRELOC=1`.

**What it does**: For each 1-bit FF with negative slack, apply NTU Eq.2-3 sign-based unit forces along critical paths. Iterates to convergence. Moves FFs toward timing-optimal positions.

**Change**: Make it default-ON when `NTU_FLOW=1` (new master gate). No code changes to the force model itself — it already implements NTU's exact formula.

### 2.2 Component: Top-Down Banking Loop (restructure `doMatchingClustering`)

**Current flow in Banking::doMatchingClustering()**:
```
Phase 1: 2-bit LEMON matching on all 1-bit FFs → commit
Phase 2+3: For each higher-bit (4, 8...):
    (a) MATCH_HIGHER_BIT matching (default OFF)
    (b) Greedy fallback: iterate 2-bit MBFFs, try merge → 4-bit
```

**Proposed flow when NTU_FLOW=1**:
```
for k = k_max (4) down to k_min (2):
    sourceBit = k / 2
    Collect all sourceBit-width FFs (1-bit for k=2, 2-bit for k=4)
    Build LEMON matching graph (reuse existing graph-build infrastructure)
    Run MaxWeightedMatching
    Commit matches using existing ALG1 defer-and-batch + CostCompare verify
    perLevelDecluster(k-bit)   // NEW: decluster harmful merges
    Rebuild legalizer state
```

**Key differences from current**:
1. 4-bit matching runs FIRST (not after 2-bit as greedy fallback)
2. 4-bit uses full LEMON matching (not greedy iteration)
3. Per-level declustering catches harmful 4-bit merges before 2-bit pass
4. 2-bit matching operates on remaining unbanked 1-bit FFs

**How it addresses MATCH_HIGHER_BIT +254% hc02 regression**:
The hc02 cascade happened because 4-bit merges created displacement that ruined 2-bit FFs downstream, with no way to undo. With per-level declustering:
- After 4-bit matching, immediately evaluate each 4-bit MBFF via ΔC
- ΔC < 0 → debank back to 2×2-bit or 4×1-bit
- Freed FFs re-enter the 2-bit matching pool
- Safety margin on 4-bit commit can be HIGHER (less conservative) because declustering catches mistakes

**Reused infrastructure**:
- `Banking::buildMatchingGraph()` — builds LEMON graph (already parameterized by sourceBit)
- `Banking::commitMatches()` — ALG1 defer-and-batch with CostCompare verify
- `Banking::CostCompare()` — per-pin timing-aware cost comparison
- `Legalizer::FindPlace()` / `UpdateRows()` — legal position finding
- `Manager::debankFF()` / `bankFF()` — debank/rebank primitives

### 2.3 Component: Per-Level Declustering (NEW, NTU Eq.5)

**Algorithm** (NTU Eq.5):
```
For each k-bit MBFF created in this level:
    s(i) = sum of negative slack on all pins of MBFF i
    s̄ = reference slack for (k/2)-bit FFs (median or mean of existing)
    ΔC(i) = α·(s(i) - s̄) + β·ΔPower(i) + γ·ΔArea(i)
    if ΔC(i) < decluster_threshold:
        debankFF(i)
        for each freed constituent:
            FindPlace(1-bit) or remain as (k/2)-bit
            UpdateRows()
```

**Where it fits**: Called inside the banking_v2 loop, after each k-bit matching commit round.

**Parameters**:
- `decluster_threshold`: default 0 (NTU's choice). Env-override `DECLUSTER_THRESH`.
- `s̄` computation: median of negative slacks among all same-clock (k/2)-bit FFs
- α, β, γ: from testcase header (already available as `FF::alpha`, `FF::beta`, `FF::gamma`)

**Relationship to existing postLGDecluster**:
This replaces the need for `postLGDecluster` — catching harmful merges at banking time is strictly better than catching them after legalization (position is more accurate, less disruption).

### 2.4 Component: S_cluster Priority in Edge Weights (NTU Eq.4)

**Algorithm** (NTU Eq.4):
```
S_cluster(i) = (L_{2k}(i) - L_k(i)) + c1 · sigmoid(-slack(i) / c2)
```
Where L_k = HPWL of k-nearest FFs to i. Dense + timing-critical FFs get highest priority.

**Integration**: Modify edge weight in `Banking::buildMatchingGraph()`:
```cpp
// Current:
adjGain = gain + dist_bonus_term;
// Proposed:
double urgency = sigmoid(-slack / c2);
adjGain = gain * (1 + SCLUSTER_BOOST * (1 - urgency))
        + dist_bonus_term * (1 - SCLUSTER_DAMPEN * urgency);
```

This doesn't change matching ORDER (LEMON decides), but makes the matching algorithm naturally prefer low-urgency (slack-rich) pairs and penalize high-urgency (timing-critical) pairs that require large displacement.

---

## 3. Env Gate Design

**Master gate**: `NTU_FLOW=1` (default 0 = byte-exact baseline)

When `NTU_FLOW=1`:
- `TIMING_PRELOC` automatically enabled (force model)
- Banking uses top-down k_max→k_min loop
- Per-level declustering active
- S_cluster edge weights active
- `MATCH_HIGHER_BIT` semantics change (4-bit matching is integral, not optional)

Sub-gates for fine-grained testing:
- `FORCE_RELOC_ITERS=10` (force model iterations)
- `DECLUSTER_THRESH=0` (per-level decluster threshold)
- `SCLUSTER_BOOST=0.3`, `SCLUSTER_DAMPEN=0.5` (edge weight coefficients)
- `TD_SAFETY_MULT=2.0` (4-bit commit safety margin multiplier)

When `NTU_FLOW=0`: completely byte-exact with current HEAD.

---

## 4. Implementation Phases

### Phase 1: Force Model Activation + Per-Level Decluster (~2 days)
- Activate `timingPreRelocation()` under `NTU_FLOW=1`
- Implement `perLevelDecluster()` as standalone function in Manager.cpp
- Test: run after existing banking, decluster 4-bit MBFFs only
- Validation: tc2 + hc02 (hc02 is the 4-bit cascade canary)

### Phase 2: Top-Down Banking Loop (~3-4 days)
- Restructure `doMatchingClustering()` to iterate k_max→k_min
- Wire perLevelDecluster into the loop
- Handle 4-bit matching on 1-bit FFs: match 4×1-bit groups directly (build 4-nearest graph, LEMON matching, commit as 4-bit MBFF). This is different from current greedy which merges 2×2-bit.
- Validation: all 7 contest cases + hc02 regression gate

### Phase 3: S_cluster Edge Weights (~1 day)
- Add urgency-based gain adjustment to edge weight computation
- Compute L_k and L_{2k} via R-tree k-nearest queries (infrastructure exists)
- Validation: tc2 sweep on SCLUSTER_BOOST, SCLUSTER_DAMPEN

### Phase 4: Integration + Sweep (~2 days)
- Combined NTU_FLOW=1 sweep on all 7 cases
- Coefficient fitting on sub-gates
- Compare vs NTU per-case numbers
- Ship if strict 7-case win or selective per-case gates

---

## 5. Risk Analysis

| Risk | Mitigation |
|------|------------|
| 4-bit cascade (hc02 +254% history) | Per-level decluster catches bad merges; TD_SAFETY_MULT=2.0 |
| Force model hurts non-critical FFs | Only moves FFs with negative slack (already implemented) |
| Top-down loop slower | k_max=4, k_min=2 → only 2 iterations; each reuses existing infra |
| Per-level decluster too aggressive | Threshold tunable; ΔC=0 is NTU's default, conservative |
| Interaction with post-LG stages | postLGDecluster/unbankRebank still run but have less work |

---

## 6. Expected Impact

| Component | Expected tc2 Impact | Mechanism |
|-----------|-------------------|-----------|
| Force model | -1.0 to -2.0% | Moves critical FFs toward timing-optimal pre-banking |
| Top-down 4→2 | -0.5 to -1.0% | Better 4-bit decisions with safety net |
| Per-level decluster | -0.5 to -1.0% | Catches harmful merges at source |
| S_cluster weights | -0.3 to -0.5% | Matching avoids critical-path displacement |
| **Combined** | **-2.3 to -4.5%** | Closes or exceeds 3.09% gap |

NTU achieves TNS=5,223 vs our 7,850 on tc2. If the combined flow brings our TNS to ~5,500, the score gap closes to ~0.5% (within reach of existing post-LG refinement stages).

---

## 7. Files Modified

| File | Change | Phase |
|------|--------|-------|
| `main.cpp` | Wire `NTU_FLOW` master gate | 1 |
| `src/Manager.cpp` | Activate force model; add `perLevelDecluster()` | 1 |
| `inc/Manager.h` | Declare `perLevelDecluster()` | 1 |
| `src/Banking.cpp` | Restructure `doMatchingClustering()` for top-down loop; add S_cluster weights | 2, 3 |
| `inc/Banking.h` | Add helpers if needed | 2, 3 |

No new files. No new dependencies. All changes within existing class structure.
