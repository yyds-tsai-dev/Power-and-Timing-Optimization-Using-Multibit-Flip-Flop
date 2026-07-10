# Plan — Stage 5 Incremental Slack + Legalization-Aware Scoring

**Workspace**: V2 (`2024-ICCAD-Problem-B_V2/`, branch `arch_rewrite`)
**Parent**: [`plan_beat_ntu_longterm.md`](plan_beat_ntu_longterm.md) Stage 5 (wide fork, per postmortem §6)
**Predecessor postmortem**: [`../../reports/v2/2026-04-21_postmortem_stage3_lp.md`](../../reports/v2/2026-04-21_postmortem_stage3_lp.md)
**Baseline**: V2 HEAD `ec0bee9` (LP/2 dormant behind `ALG2_LP_BANK=0`)
**Date**: 2026-04-21 (revised 2026-04-21 after SEQ200 triangulation)
**Status**: ACTIVE — scope set; Phase 1.5 SEQ prototype shipped env-gated; Phase 0 diagnostic remains gate for Phase 1

---

## 0. One-paragraph summary

Stage 3 LP-round dead-ended with a 120× gap between LP's predicted gain (sum of per-candidate `CostCompare` deltas) and the evaluator's actual score delta on hc01. The initial sequential-commit experiment (`ALG2_LP_SEQ=1 K=1 max_iter=100`, 2026-04-21 AM) appeared to rule out Cause A (simultaneity), but was under-configured: hc01 needs ≈4,600 commits and K=1 max_iter=100 only achieved 100. The properly-configured SEQ200 run (`K=50 max_iter=200 MATCH_HIGHER_BIT=1`, 2026-04-21 PM) collapses the hc01 regression from +63.85% to +8.5% and tc1 to +0.6% — **Cause A (non-stationary candidate pool under commit cascade) is the dominant contributor**, not a negligible one. The remaining ≤+8.5% is attributable to Cause B (slack-baseline staleness: upstream-FF slack degradation never propagates to downstream `getSlack()`) and Cause C (post-banking drift: `CostCompare` targets a pre-Legalizer midpoint, but Legalizer + DP move the FF further before the evaluator reads final state). Stage 5 addresses all three: (0.5) **iterative LP-commit** — per-iter LP relaxation over a rolling candidate pool, re-enumerated after each commit; (1) **legalization-aware scoring** — score each candidate against the coord the Legalizer will actually produce, not the midpoint; (2) **incremental slack propagation** — `getSlack()` returns a value that reflects the current state of every upstream FF, updated on each `bankFF`. Together they make `CostCompare` a faithful approximation of evaluator cost over a realistic commit trajectory, which is the prerequisite for *any* globalized banker (LP, SA, ILP, Lagrangian) to deliver its provable guarantees on Problem B's composite objective.

---

## 1. Novelty claim (thesis chapter)

> **Iterative-LP + legalization-aware + incremental-STA MBFF banking** — the first MBFF banker for ICCAD 2024 Problem B in which (i) the banking subproblem is formulated as a sequence of local LP-relaxed k-set packings over a non-stationary candidate pool, each with its own (2+ε) rounding guarantee, and (ii) `CostCompare(candidate)` is empirically faithful to the post-placement, post-DP evaluator score under the composite α·TNS + β·Power + γ·Area + λ·Density objective.

Four publishable angles:

1. **Iterative LP-round for non-stationary k-set packing** (2026-04-21, new): batch LP k-set packing (Chan-Lau 2012) assumes a stationary candidate universe. MBFF banking violates this — committing a k-set removes its FFs from the rtree, which reshapes *every* remaining candidate's neighborhood and gain. Iterative LP (SEQ K=50, re-enumerate after commit) achieves greedy-scale clustering (4,583 vs baseline 4,646 FF43 on hc01) while retaining per-iteration (2+ε) guarantees; batch LP by contrast regresses 64%. No prior MBFF banker (coherent17, ntu-113-2, contest 2024 field) iterates LP this way; prior LP work on related routing / floorplanning problems (Even-Shiloach, Plotkin) iterates over *constraint violations*, not over a reshaping candidate universe.
2. **Incremental per-sink STA update for banking**: each `bankFF` triggers an O(|affected sinks| + |upstream hops|) update to a cached timing graph; `getSlack()` becomes an O(1) lookup that reflects all prior banking commits. Prior art (coherent17, ntu-113-2) recomputes from parse-time base each query.
3. **Legalization-aware candidate scoring**: replace the `midpoint` proxy with either (a) a pre-computed `subrowSnap(midpoint)` or (b) a lightweight one-candidate LG simulation. Prior heuristic bankers optimize a coord the placer never produces.
4. **Globalizer as diagnostic harness**: iterative LP (angle 1) is also the faithfulness tester for angles 2+3. If the scoring rewrite is correct, per-iter LP predicted gain should track per-iter evaluator delta within a constant factor across cases. Greedy MWM masks scoring bugs (the implicit serialization hides them); LP-per-iter exposes them. This is itself a methodology contribution.

Prior art: OpenSTA (Peter Ayala), OpenROAD `dbSta`, incremental-STA papers (Xu et al. 2018). All assume a *finished* placement; none integrate into a banking inner loop where FF positions change per candidate evaluation. MBFF-specific: Lin et al. 2021 uses STA but only at coarse granularity (pre/post banking, not per-candidate). LP-for-MBFF: the Stage 3 archived plan surveyed the space; no prior work iterates LP over a reshaping FF pool.

---

## 2. Empirical premise (from postmortem + SEQ200 triangulation)

| Evidence | Implication |
|:---------|:------------|
| hc01 batch LP gap = 2.3%, score regression = +63.85% | Batch LP objective ≠ final score |
| Initial SEQ K=1 max_iter=100 within 0.15% of batch | **DISCARDED 2026-04-21**: under-configured — 100 commits vs ~4,600 needed; not a Cause A refutation |
| SEQ200 (K=50 max_iter=200 MHB=1) hc01 score 33.75M vs batch 51M vs baseline 31.1M | **Cause A IS the dominant contributor**: iterating LP over reshaping pool closes 55/63.85 ≈ 86% of batch regression |
| SEQ200 tc1: 753.3M vs baseline 748.7M = +0.6% | Cause A dominance is not hc01-specific — tc1 near-parity confirms iterative direction is general |
| SEQ200 hc01 commit scale: 4,583 FF43 vs baseline 4,646 FF43 | Iterative LP recovers greedy's commit scale; batch LP cannot (static enumeration misses cascaded merges) |
| Residual SEQ200 regression +8.5% hc01 / +0.6% tc1 | Cause B + Cause C account for what remains — Phase 1 + Phase 2 target this residual |
| `FF::getSlack()` reads live `getNewCoor()` but never reconciles upstream slack degradation | Cause B mechanism confirmed in code |
| `Banking::computePinTNS` scores against `c.coord` = `midpoint`, before `FindPlace` | Cause C mechanism confirmed in code |
| SEQ200 banking time: hc01 464s, tc1 503s (vs baseline 8–13s) | Solve is 80% of banking — Phase 1.5 hardening must address runtime |

**What Stage 5 does NOT prove until Phase 0 lands**: the exact split of residual +8.5% between B and C. Phase 0 diagnostic (midpoint→placeCoor drift + re-scored CostCompare) classifies C's contribution; the remainder is B. This decides whether Phase 1 (LG-aware) or Phase 2 (incremental STA) closes more of the residual first.

---

## 3. Scope (Phases)

### Phase 0 — Cause C premise check (diagnostic, ~4 hours)

**Goal**: confirm Cause C is the dominant contributor before investing in Phase 1's scoring rewrite. Adds a diagnostic to Pass 3 that logs, per committed candidate, the midpoint → `placeCoor` (post-`FindPlace`) displacement and the corresponding re-scored CostCompare delta.

**Files**: `src/Banking.cpp` (Pass 3 commit loop, both 2b and HB), `src/LPBanking.cpp` (commit loop if `ALG2_LP_COMMIT=1`).

**Env gate**: `ALG2_STAGE5_DIAG=1` (off by default; purely additive).

**Diag line**:
```
[STAGE5_DIAG] cluster=N bits=K mid=(x1,y1) place=(x2,y2) disp=D
              cc_mid=G_mid cc_place=G_place delta=(G_place-G_mid)
```

**Run**: hc01 + tc1 with `ALG2_LP_BANK=1 ALG2_LP_COMMIT=1 ALG2_STAGE5_DIAG=1` (LP commits ~37 clusters on hc01 → tractable sample).

**Decision rule**:
- If ≥ 50% of clusters show `|cc_place - cc_mid|` > 10× `cc_mid` → **Cause C confirmed dominant**. Phase 1 proceeds as written.
- If `cc_place ≈ cc_mid` for most clusters but the evaluator still regresses → **Cause B is the bigger chunk**. Re-scope: defer Phase 1, start Phase 2 first.
- Mixed (C and B both material) → run Phase 1 and Phase 2 in parallel branches.

**Non-goal**: Phase 0 does not attempt to *close* the gap; it only classifies it.

---

### Phase 1.5 — Iterative LP-commit hardening (Cause A fix, 1–2 weeks)

**Status**: prototype shipped 2026-04-21 behind `ALG2_LP_SEQ=1 ALG2_LP_SEQ_K=<K> ALG2_LP_SEQ_MAX_ITER=<N>`. hc01 SEQ200 closes 86% of batch regression; runtime is 35–60× baseline. Remaining work = runtime + ablation.

**Goal**: harden iterative LP from a diagnostic prototype into a shippable banker. The prototype's correctness is already validated (hc01 +63.85% → +8.5%, tc1 ≈parity); the work here is runtime reduction and disciplined ablation — without this, Phase 3's validation harness cannot run the 7-case matrix in a tractable budget.

**Approach** (three parallel tracks, any subset shippable):

- **1.5.a — LP warm-start across iterations** (~3 days): GLOP's default re-solves each iteration from scratch. Since SEQ200 re-enumerates ≥70% overlap in the candidate pool across consecutive iters (committed k-sets account for <1% per iter), the LP basis is nearly reusable. Plumb OR-tools `glop::LinearProgramBuilder` basis export/import; re-solve warm. Expected: 3–5× solve speedup per iter.
- **1.5.b — Rolling frontier enumeration** (~1 week): instead of re-enumerating the full candidate pool each iter, maintain a per-FF "stale-flag"; after commit, mark only FFs within `r` rtree-hops of committed FFs as stale; re-enumerate only candidates touching stale FFs. Cost per iter drops from O(|FF|) to O(Δ|FF|) where Δ ≈ K·fanout. Expected: 2–3× enumeration speedup.
- **1.5.c — Adaptive K schedule** (~2 days): SEQ200 trajectory shows `gain_p50` drops from 15K (iter 0) to 4.4K (iter 80+) on tc1 — late iters commit marginal k-sets. Schedule `K(t) = max(K_min, K0 · γ^t)` with early termination when `gain_p90 < gain_floor`. Expected: 30–50% iter count reduction without score loss.

**Files**:
- `src/LPBanking.cpp` — OR-tools basis plumbing (1.5.a), stale-flag bookkeeping (1.5.b), K scheduler + early-term (1.5.c).
- `inc/LPBanking.h` — SEQ solver state struct (persists across iters for warm-start).
- `src/Manager.cpp` — unchanged.

**Env gates**:
- `ALG2_LP_SEQ={0,1}` — master (already shipped).
- `ALG2_LP_SEQ_K` — fixed K (already shipped) or comma-pair like `50,5` meaning K0=50 K_min=5 for schedule (1.5.c).
- `ALG2_LP_SEQ_MAX_ITER` — ceiling (already shipped).
- `ALG2_LP_WARMSTART={0,1}` — new, default 0 (byte-exact baseline preserved).
- `ALG2_LP_FRONTIER_R={0,1,2}` — new, 0 = full enumeration (default), 1/2 = rolling frontier hop radius.
- `ALG2_LP_GAIN_FLOOR` — early-term threshold (already parsed; extend semantics to trigger termination when `gain_p90 < floor`).

**Acceptance**:
- (i) `ALG2_LP_SEQ=0` → byte-exact vs batch-LP path (preserved via prototype's existing branch).
- (ii) `ALG2_LP_SEQ=1 K=50 max_iter=200 MHB=1` on hc01: reproduces SEQ200 result (33.75M ±0.5%). Ablation regression test.
- (iii) `ALG2_LP_WARMSTART=1` on hc01 SEQ200 config: banking time ≤ **200s** (from 464s), score within ±0.1% of (ii). Warm-start preserves correctness.
- (iv) `ALG2_LP_FRONTIER_R=2` on hc01 SEQ200 config: banking time ≤ **100s**, score within ±0.5% of (ii).
- (v) Per-iter trajectory diag (`[ALG2_LP_SEQ_TRAJ]`): logs per iter `{lp_obj, agg_committed, gain_p50, gain_p90, score_approx}`. This feeds Phase 3's thesis figure.

**Ablation**: {warmstart off/on} × {frontier 0/1/2} × {schedule off/on} × {hc01, tc1} = 24 runs. Plus 7-case sweep at the chosen default.

**Non-goal**: changing the LP formulation itself. The ILP is `x_S ∈ {0,1}`, constraint `Σ_{S ∋ f} x_S ≤ 1`, objective `Σ gain(S) x_S` — identical to Stage 3. Only the *meta-loop* around the LP changes.

**Risk**: frontier radius 1 may under-enumerate near commit boundaries and leave gap on top of SEQ200's 8.5% — fall back to radius 2 or full. 1.5.c schedule may terminate too early on hc-variant cases with long-tail gain distributions — tune `gain_floor` per-case via env or adaptive.

---

### Phase 1 — Legalization-aware candidate scoring (2 weeks)

**Goal**: replace `c.coord = midpoint` in scoring paths with a coord that approximates what Legalizer will produce, so `CostCompare(c)` is a faithful predictor of post-LG cost.

**Approach tiers** (pick based on Phase 0 dispersion):

- **1.a — subrow-snap proxy** (~2 days): precompute per-subrow free-slot R-tree once, and for each candidate snap `midpoint` to the nearest free slot of the target cell width on its row. Cost: O(log N) per candidate. Captures ~70% of midpoint→placeCoor delta (empirical guess; refine after Phase 0).
- **1.b — single-candidate incremental LG** (~1 week): expose `Legalizer::tentativePlace(ff, cellType, midpoint) → placeCoor` that runs the existing `FindPlace` logic *without* committing. `CostCompare` calls this; if the candidate is rejected, no state change. Cost: one R-tree query + bounded linear probe per candidate. Exact for single-FF placement.
- **1.c — candidate-batch LG lookahead** (~3 days, optional): for the top-K candidates per Pass-3 iteration, run a hypothetical `Legalizer::batchLegalize(K)` and pick the one whose true post-LG cost is smallest. Expensive but strictly accurate. Use only if 1.a/1.b still leave >20% gap.

**Default**: 1.a (cheapest, likely enough per postmortem § 3.1 discussion).

**Files**:
- `src/Legalizer.cpp` — add `tentativePlace(FF*, CellType*, Coor) → Coor`, read-only.
- `src/Banking.cpp` — `FindTop2LegalCoors` path (Pass 2) optionally routes through `tentativePlace`.
- `src/LPBanking.cpp` — `buildAndSolve` computes `gain(S)` against `tentativePlace(...)` coord.
- `src/Banking.cpp` `computePinTNS` — unchanged (already per-pin HPWL); only `c.coord` source changes.

**Env gate**: `ALG2_LGAWARE={off, snap, incremental}`. Default `off` → Stage 5 Phase 1 disabled → byte-exact vs V2 HEAD.

**Acceptance**:
- (i) `ALG2_LGAWARE=off` → byte-exact vs V2 HEAD (Phase 0 check).
- (ii) `ALG2_LGAWARE=snap ALG2_LP_BANK=1 ALG2_LP_COMMIT=1` on hc01: regression drops from +63.85% to **≤ +20%**. If not, 1.a insufficient; escalate to 1.b.
- (iii) `ALG2_LGAWARE=incremental ALG2_LP_BANK=1 ALG2_LP_COMMIT=1` on hc01: regression ≤ **+5%**.
- (iv) Greedy path (FITP/3) under `ALG2_LGAWARE=snap` on 7-case sweep: no case regresses > +1% vs V1 HEAD; ≥ 2 cases improve. (Stage 5 must not break the shipped greedy.)

**Ablation**: full 2^3 = 8 matrix over {off, snap, incremental} × {greedy, LP} × {ALG2_LG_CACHE={on, off}}.

**Non-goal**: DP-ripple awareness. DP's post-LG cleanup is a smaller second-order delta (plan_batchedMatching.md §4 estimates 3-5%); Phase 1 explicitly ignores it.

---

### Phase 2 — Incremental slack propagation (3–5 weeks)

**Goal**: rewrite `FF::getSlack()` / `Manager::updateSlack` so that after `bankFF(cluster)`, downstream FFs' slack reflects the *current* timing state — not parse-time base + local HPWL correction.

**Approach**: build a cached directed timing-arc graph at parser time. Each arc carries `(driver, sink, base_delay, base_wire_hpwl)`. `FF::getSlack()` becomes:

```
slack(f) = min over fanin path p:
  base_arrival(p) + Σ_{arcs in p} (current_wire_delay - base_wire_delay)
```

`bankFF(cluster)` triggers an incremental update: for each FF `f` in `cluster`, walk its fanout arcs, update current_wire_delay, propagate to downstream FFs' cached slack. Bounded hop depth (config `ALG2_STA_HOP={1,2,inf}`, default 2).

**Files**:
- `inc/STAGraph.h` + `src/STAGraph.cpp` — new. O(|FF|) nodes, O(|Arc|) edges (Arc = timing path segment between FFs).
- `src/Manager.cpp` — build STAGraph during `preprocess()`. Invalidate `STAGraph.node[f].slack_cache` on each `bankFF`.
- `src/FF.cpp` — `getSlack()` becomes a cache lookup; on miss, call `STAGraph::recompute(this, hop_depth)`.
- `src/Banking.cpp` — unchanged (just reads `getSlack()`).

**Env gate**: `ALG2_STA={off, hop1, hop2, full}`. Default `off` → `getSlack()` behavior unchanged → byte-exact vs V2 HEAD.

**Correctness test**: on a small testcase (e.g. `testcase1_MBFF`), compare `ALG2_STA=full` against an OpenSTA run on the same netlist + placement. Per-FF slack must match within float-epsilon. This is the validation chapter for the thesis.

**Acceptance**:
- (i) `ALG2_STA=off` → byte-exact (sanity gate).
- (ii) `ALG2_STA=hop2` on 7-case greedy sweep: ≥ 3 cases improve vs V2 HEAD, no case regresses > +1%.
- (iii) `ALG2_STA=hop2 ALG2_LGAWARE=incremental ALG2_LP_BANK=1 ALG2_LP_COMMIT=1` on hc01: regression ≤ **+2%** vs V1 HEAD (stretch: wins).
- (iv) Runtime: `ALG2_STA=hop2` adds ≤ 30% to banking stage on largest case (hc04).

**Ablation**: {off, hop1, hop2, full} × {greedy, LP} × 7 cases = 56 runs. Run in partition across test machines.

**Non-goal**: full STA arc-delay modeling (gate cell delay, input slew, capacitance-derated). We approximate arc delay as `DisplacementDelay · HPWL(driver, sink)` — same model the evaluator uses. Full STA (with `.lib` arc tables) is Stage 6, out of scope.

---

### Phase 3 — Globalizer re-validation (2–3 days)

**Goal**: with Phase 1 + 2 landed, re-run LP batch commit on the 7-case sweep; demonstrate the regression closes. This produces the thesis's headline result.

**Runs**:
1. `ALG2_LP_BANK=1 ALG2_LP_COMMIT=1 ALG2_LGAWARE=incremental ALG2_STA=hop2` on 7 cases.
2. Same config + `ALG2_LP_SEQ=1 ALG2_LP_SEQ_K=1` — SEQ should now track batch within 0.1%.
3. Compute LP dual bound vs final score per case — the 120× factor should become ≤ 2× (≈ the (k+1)/2 worst-case rounding loss).

**Acceptance**:
- LP batch on 7 cases: geo-mean ≤ 0.97 vs V1 HEAD (stretch: ≤ 0.93).
- `|lp_obj - (V1_score - LP_score)|` per case ≤ 2× `lp_obj` — demonstrates CostCompare is now faithful.
- All 7 cases pass `checker/sanity` + `checker/placement_checker`.

**Report**: `reports/v2/exp_stage5_full.md` with per-case numbers + LP-gap-vs-score-delta scatter. This is the thesis figure.

---

## 4. Files / interface changes

| File | Phase | Change |
|:-----|:-----|:------|
| `src/LPBanking.cpp` | 0, 1, 1.5 | STAGE5_DIAG on commit; SEQ loop + warm-start + frontier + schedule; gain computation against LG-aware coord |
| `inc/LPBanking.h` | 1.5 | expose SEQ solver state for warm-start |
| `inc/STAGraph.h` | 2 | NEW — arc graph + incremental API |
| `src/STAGraph.cpp` | 2 | NEW — ~500 LoC |
| `src/Manager.cpp` | 2 | build STAGraph in `preprocess()`, invalidate on `bankFF` |
| `src/FF.cpp` | 2 | `getSlack()` routes through STAGraph when `ALG2_STA != off` |
| `src/Legalizer.cpp` | 1 | `tentativePlace()` read-only variant of `FindPlace` |
| `inc/Legalizer.h` | 1 | expose `tentativePlace` |
| `src/Banking.cpp` | 0, 1 | STAGE5_DIAG line; optional reroute through `tentativePlace` in Pass 2 |
| `Makefile` | — | no change (OR-tools already optional) |

Legacy default (all stage-5 gates off) reproduces V2 HEAD byte-for-byte. That is a hard gate in Phase 0 and Phase 1 acceptance.

---

## 5. Env gates

| Gate | Values | Default | Scope |
|:-----|:-------|:------:|:------|
| `ALG2_STAGE5_DIAG` | 0/1 | 0 | Phase 0 diagnostic log |
| `ALG2_LP_SEQ` | 0/1 | 0 | Phase 1.5 master — enable iterative LP commit |
| `ALG2_LP_SEQ_K` | int or "K0,Kmin" | 50 | Phase 1.5 fixed-K or schedule (1.5.c) |
| `ALG2_LP_SEQ_MAX_ITER` | int | 200 | Phase 1.5 iteration ceiling |
| `ALG2_LP_WARMSTART` | 0/1 | 0 | Phase 1.5 LP basis reuse across iters (1.5.a) |
| `ALG2_LP_FRONTIER_R` | 0/1/2 | 0 | Phase 1.5 rolling frontier re-enumeration radius (1.5.b) |
| `ALG2_LP_GAIN_FLOOR` | float | -1e18 | Phase 1.5 early-term threshold on gain_p90 |
| `ALG2_LGAWARE` | off/snap/incremental | off | Phase 1 scoring coord source |
| `ALG2_LG_CACHE` | 0/1 | 1 | Phase 1 subrow free-slot R-tree reuse |
| `ALG2_STA` | off/hop1/hop2/full | off | Phase 2 slack propagation depth |
| `ALG2_STA_VALIDATE` | 0/1 | 0 | Phase 2 OpenSTA cross-check (slow) |

`ALG2_LP_BANK`, `ALG2_LP_COMMIT` stay (already in LPBanking.cpp; orthogonal to SEQ mode).

---

## 6. Gates & ablation discipline

Per V2 SOP:

1. **Phase 0 blocker**: if Phase 0 diagnostic shows `|cc_place - cc_mid|` small for >50% of clusters (Cause C not dominant), pause Phase 1 and re-scope.
2. **Phase 1.5 ship gate**: `ALG2_LP_SEQ=1 K=50 max_iter=200 MHB=1` on 7-case sweep must reproduce SEQ200 hc01/tc1 numbers; runtime ≤ 100s per case after warm-start + frontier land (1.5.a + 1.5.b). Both checkers PASS.
3. **Phase 1 ship gate**: greedy 7-case sweep under `ALG2_LGAWARE=incremental` must be non-regressing (≤ +1% worst case) before merging to arch_rewrite HEAD.
4. **Phase 2 ship gate**: same, under `ALG2_STA=hop2`.
5. **Phase 3 thesis gate**: SEQ-LP + both rewrites on 7 cases — geo-mean ≤ 0.97 vs V1 HEAD, per-iter CostCompare-to-evaluator-delta ratio ≤ 2× per case.
6. **Full ablation on merge-back**: each of Phase 1.5, Phase 1, Phase 2 toggled independently, both under greedy and LP paths. 2^3 × 2 × 7 = 112 runs. Required before any V1 cherry-pick.

---

## 7. Risk register

| Risk | Mitigation |
|:-----|:-----------|
| Phase 0 shows Cause C is not dominant | Re-scope: start Phase 2 first, revisit Phase 1 after Phase 2's slack rewrite lands |
| `Legalizer::tentativePlace` has too many side-effects in current code | Phase 1.a (subrow-snap) sidesteps the issue; 1.b only taken if 1.a proves insufficient |
| STAGraph incremental update is O(N) not O(ΔE) per commit | Hop-bounded (`ALG2_STA_HOP=2`) keeps worst case O(fanout² · depth) per bankFF; profile before expanding |
| STAGraph correctness hard to prove without OpenSTA | `ALG2_STA_VALIDATE=1` cross-checks on small testcase; ship without validation only if checker + evaluator pass on 7 cases |
| Runtime regression on hc04 | Per-commit invalidation, not per-query recompute; `ALG2_STA=hop1` fallback if hop2 too slow |
| LP solve time (already 13 min on hc02 batch; 464s on hc01 SEQ200) | Phase 1.5.a (warm-start) + 1.5.b (frontier) target this directly; fallback: HiGHS / interior-point if GLOP warm-start plumbing hits OR-tools API limits |
| Frontier radius under-enumerates and leaves score gap on top of SEQ200's 8.5% | Fallback to radius 2 or full (radius 0); gate acceptance on score within ±0.5% of full-enum reference |
| Adaptive K schedule (1.5.c) terminates too early on long-tail cases | Keep fixed-K path as default; gate 1.5.c opt-in per-case via `ALG2_LP_SEQ_K=<K0>,<Kmin>` syntax |
| Thesis timeline > 8 weeks | Phase 1.5 alone (iterative LP-round novelty) is a publishable chapter without Phase 1 or Phase 2 — it stands if LG-aware + STA stall |

---

## 8. Dependencies / sequencing

- Blocks on: Phase 0 diagnostic → directs Phase 1 vs Phase 2 priority.
- Can run in parallel with: Stage 4 (SA polish) — **but** per postmortem §6 Stage 4 is blocked on Stage 5, so effectively serial.
- Affects: Stage 3 LP dormant code (re-activated in Phase 3 for validation only; no new algorithmic work in LPBanking.cpp beyond scoring-coord routing).
- Does not affect: Stage 2 (slack-driven pre-GP, still deferred); Stage 1 (Steiner, dead).

---

## 9. Exit criteria (plan → report)

Stage 5 concludes when *either*:

1. **Ship path**: Phase 1 + Phase 2 + Phase 3 all meet gates → write `reports/v2/exp_stage5_full.md`, archive this plan, propose cherry-pick plan for V1 (likely partial — just Phase 1 subrow-snap, which is minimal-surface and orthogonal to V1 banking changes).
2. **Dead-end path**: any phase's gate fails twice (once after normal implementation, once after mitigation) → write `reports/v2/postmortem_stage5_*.md`, archive plan. Thesis fallback: use Stage 3 dual-bound chapter + whatever partial Stage 5 component shipped cleanly.

---

## 10. Thesis chapter outline (draft)

- **Ch. N — Iterative-LP + Legalization-aware + Incremental-STA MBFF banking**
  - N.1 Problem: composite-objective banking's cost function is a per-candidate evaluator approximation over a *cascading* commit trajectory; prior work (coherent17, ntu-113-2) uses midpoint + parse-time slack + batch/greedy commit.
  - N.2 Empirical exposure: Stage 3 LP batch-commit harness reveals 120× gap on hc01. SEQ200 triangulation (2026-04-21) decomposes the gap into three orthogonal causes.
  - N.3 Cause A (non-stationary pool): batch LP enumerates over a stationary candidate universe; MBFF banking violates this — committing a k-set reshapes the rtree neighborhood of *every* remaining FF. Iterative LP (SEQ) matches greedy's commit scale (4,583 vs 4,646 FF43) while retaining per-iter (2+ε) rounding guarantee. Closes ~86% of hc01 batch regression.
  - N.4 Cause B (stale slack): upstream-FF slack degradation from prior commits never propagates to downstream `getSlack()`. Fix: hop-bounded incremental STA propagation on `bankFF`.
  - N.5 Cause C (scoring drift): `CostCompare` scores against midpoint but Legalizer moves the FF elsewhere. Fix: subrow-snap / one-candidate LG tentative-place.
  - N.6 Composed solution: the three fixes are orthogonal and compose. SEQ-LP supplies the meta-loop; LG-aware supplies the coord; incremental STA supplies the slack.
  - N.7 Validation: per-iter LP objective tracks per-iter evaluator delta within ≤2× across 7 cases; 7-case geo-mean ≤ 0.97 vs V1 HEAD. CostCompare-to-evaluator ratio from 120× → ≤2×.
  - N.8 Methodology: LP-per-iter is simultaneously the banker *and* the scoring-faithfulness harness. Greedy MWM masks scoring bugs (implicit serialization); LP-per-iter exposes them. Reusable design pattern for any cascading combinatorial optimization problem.
  - N.9 Related: OpenSTA, OpenROAD, Xu 2018 (all assume finished placement); Lin 2021 (MBFF + STA, coarse); Chan-Lau 2012 (batch LP k-set); no prior iterative-LP-round for non-stationary k-set packing in MBFF or adjacent fields.
  - N.10 Limitations: no gate-cell arc delay; STA approximation depth bounded at hop2; SEQ runtime scales linearly with commit count (mitigated by warm-start + frontier but not eliminated).
