---
name: Stage 3 LP-round dead-end + SEQ200 revival + 7-case reality check
description: Batch LP archived; SEQ200 revival only closes on hc01/tc1 (n=2). 2026-04-22 full 7-case sweep shows LP regresses +4–18% on 5/7 cases (tc2 +9.4%, hc02 +17.6%, tc3 +4.4%, hc04 +4.5%). Cap tuning won't fix it. See phase1_5_b_cap_probe_findings.md
type: project
originSessionId: 337e23e6-9fa1-45e1-93b2-f42a788fbcff
---
Stage 3 LP-round banking (plan_stage3_lp_round.md) archived 2026-04-21 after Phase 2 7-case commit-mode sweep regressed 7/7 vs V1 HEAD: +1.90% tc1, +22.37% tc2, +5.72% tc3, +63.85% hc01, +70.21% hc02, +3.28% hc03, +5.75% hc04.

**Why:** The empirical regression is real; the cost-model explanation in the first draft of the postmortem was wrong and was corrected 2026-04-21.

- **Original (WRONG)** claim: CostCompare uses `DisplacementDelay · (|dx|+|dy|)` per-pin proxy vs evaluator per-sink HPWL.
- **Correct claim**: `Banking::computePinTNS` (src/Banking.cpp:363-471) already computes per-constituent `HPWL(driverCoor, D-pin)` and `HPWL(loadCoor, Q-pin)` against actual `prev.instance` / `NextStage` positions — aligned with the evaluator's slack model (per memory `project_evaluator_slack_model`). The per-call cost is correct; the failure is in *batch composition*.

**Three candidate causes**, in order of uniqueness to LP:
1. **Cause A — Simultaneity assumption (LP-unique)**. LP scores all candidates pre-any-commit. If cluster_i's driver FF is itself in cluster_j and both get picked, cluster_i's scoring used cluster_j's FF at its *pre-banking* position. Greedy is immune because `bankFF` updates `getNewCoor()` between each CostCompare call.
2. **Cause B — Slack baseline staleness (LP + greedy)**. `cf->getSlack()` returns parse-time base + current-position wire delay, but never reconciles against upstream-FF's post-banking degraded slack.
3. **Cause C — Post-banking drift (LP + greedy)**. CostCompare targets midpoint (pre-LG); Legalizer + DP move FFs further before evaluator reads final state.

Hc01 smoking gun (LP gap 2.3%, score +63.85%) stands: only 35 clusters committed yet score collapsed. R-thresh / pipage rounding cannot close this, so Chan-Lau rounding variants are **not worth pursuing** within Stage 3 **in batch form**.

**SEQ200 triangulation (2026-04-21 PM)** — the sequential-commit experiment that the "how to apply" line below originally gated:

Earlier SEQ K=1 max_iter=100 run appeared to rule out Cause A (within 0.15% of batch), but was **under-configured** — hc01 needs ~4,600 FF commits and max_iter=100 only produced 100. Properly-configured SEQ (K=50 max_iter=200 MATCH_HIGHER_BIT=1):
- hc01: 33.75M vs batch 51M vs baseline 31.1M = **+8.5% vs baseline (from +63.85% batch)**. Closes 86% of gap.
- tc1: 753.3M vs baseline 748.7M = **+0.6%** (near-parity).
- Commit scale: hc01 4,583 FF43 vs baseline 4,646 FF43 — LP recovers greedy's scale.
- Runtime: hc01 464s, tc1 503s vs baseline 8–13s (~60× slower; solve is 80% of banking).

**Cause A (non-stationary candidate pool) is the dominant contributor**, not negligible. Iterating LP over a pool that reshapes on each commit is what closes the gap. Residual ≤+8.5% is attributable to Cause B + C.

**How to apply:**
- LP-based banking is **not dead** — it lives on as Phase 1.5 in `plans/active/plan_stage5_incremental_slack.md` (iterative LP-commit hardening: warm-start + rolling frontier + adaptive K schedule).
- Thesis novelty pivot: "iterative LP-round for non-stationary k-set packing" is now novelty angle 1 of Stage 5 (was previously only angles 2–4: LG-aware + incremental STA + diagnostic harness).
- When proposing globalized-objective banking (LP/MIP/SA/ILP), **sequentialize the commits** — batch-mode is pathological on this problem class. SA may share batch-mode issues (Cause B/C) but not Cause A (SA is already sequential).
- Full-STA rewrite and LG-aware scoring (Phase 2 + Phase 1 of Stage 5) are still motivated: they target the residual ≤+8.5% (Cause B + C), which iterative LP alone cannot close.
- **Do not repeat under-configured SEQ experiments** — always size `max_iter` such that `K · max_iter ≥ expected_baseline_commit_count`. For hc01 that's ~4600; for tc1 ~6000. Rule of thumb: run with `K=50 max_iter=200` first for hc01-scale, bump `max_iter` for larger cases.
- Code at `[LP/2] ec0bee9` (batch) + SEQ loop + STAGE5_DIAG + P1/P2 prototypes. All env-gated, default-off. Behind `ALG2_LP_BANK=0` the batch path is inert; `ALG2_LP_SEQ=1` enables iterative mode.
- Stage 4 (joint banking+LG SA) stays on hold, but for a different reason: SA doesn't share Cause A, but still needs Cause B + C fixes before its accept-delta is meaningful.
- Plan in `Project Knowledge/plans/archived/plan_stage3_lp_round.md` (batch history); active follow-up in `Project Knowledge/plans/active/plan_stage5_incremental_slack.md` §Phase 1.5.

**2026-04-22 reality check (Phase 1.5.b)**: SEQ200's "86% closed" was n=2 (hc01 + tc1 only). Proper 7-case sweep with OR-tools-linked binary and `MATCH_HIGHER_BIT=1`:
- Cap40k vs V1 HEAD: tc1 +0.26%, tc2 +9.40%, tc3 +4.44%, hc01 +4.10%, hc02 **+17.58%**, hc03 +0.64%, hc04 +4.46%.
- Cold SEQ200 tc2: +17.14% (worse than cap40k). LP is worse than V1 matching on 5/7 cases regardless of cap.

Decision: `ALG2_LP_BANK` stays default-off. Phase 1.5.b rolling-frontier plan archived — speed win already available via cap knob, quality gap not closable by this plan. Phase 1.5.c adaptive-K also decision-gated off. See `Project Knowledge/reports/v2/stage5_phase15/phase1_5_b_cap_probe_findings.md`.
