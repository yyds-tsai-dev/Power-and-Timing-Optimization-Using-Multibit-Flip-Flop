---
name: project_rewrite_base_decision
description: "Major-rewrite base = V2; we already beat 1st on 5/7, only tc1 & tc2 lose (opposite causes); root blocker = 1-hop timing; lead = incremental timing oracle"
metadata: 
  node_type: memory
  type: project
  originSessionId: e10d56da-a5c4-4d0f-a839-3a76f0ba3cb4
---

Decision for the "大改 to beat ICCAD2024 top-3 on all 7" effort (2026-06, grounded in a 5-agent code+gap audit, not memory).

**Base = V2 (arch_rewrite).** Cleaner than V3 (cleanliness 5 vs 4, both high headroom ~7-8 because most cruft is dormant/revert-safe). V2: ~12.5k LOC, clean stage pipeline (main.cpp 99 lines + thin Manager delegators), rot concentrated in ONE god-method `doMatchingClustering` (src/Banking.cpp:839-2815, ~1976 LOC, 41 env flags inside), only 13 commits ahead of V1, ortools linked. V3: ~15.8k LOC, ~130 env gates / 145 getenv sites, a 2428-line doMatchingClustering with 63 getenv inside, ~1180 LOC ifdef-dead MILP, NO Manager::run() (main.cpp is orchestrator w/ ~20 flow gates), CLAUDE.md still self-IDs as "V1/main". V3's only asset (adaptive beta<=500 tuning) is worthless for a rewrite — it tunes the exact 1-hop matcher being torn out.

**Reframe: the goal is to convert 2 losses, not rebuild everything.** V2 default already BEATS 1st place on 5/7 (tc3 -0.14%, hc01 -2.61%, hc02 -9.84%, hc03 -0.92%, hc04 -0.06%). Only loses **tc1 (+0.20%)** and **tc2 (+2.92%)** — and they fail for OPPOSITE reasons:
- **tc1 = TNS loss** on its 964 neg-slack FFs (only area-family case with timing pressure; tc3/hc04 have 38). ~1100-1480 TNS units. Fix = FF force-relocation + WL-weighted slack split.
- **tc2 = merge-count (Area+Power) loss**, 93% of score is area+power. We drop 637 merges on cost vs sister hc03's 50; over-reject safe near-zero-slack merges. Fix = top-down cascade + per-level ΔC decluster + slack-aware site selection.
These are COUPLED: aggressive merging helps tc2 but blows tc1/hc02 TNS — why every single-knob sweep failed.

**Root blocker (confirmed in code, not just memory):** 1-hop timing model. `FF::getSlack()` (FF.cpp:366-416) + `Banking::computePinTNS()` (Banking.cpp:363-471) price a merge off parse-time base + single wire-hop, NO forward propagation into downstream cone, NO required-time reconciliation; PrevStage/NextStage graph (Preprocess.cpp:406-462) keeps only the single maxInput edge. Internal TNS underestimates evaluator by ~34% @1-hop (tc2 internal 5865 vs eval 7850). Every refinement inherits the bias → see [[project_tc2_exhaustive_analysis]] (14/14), [[project_approach_c_deadend]], [[project_cg_weight_deadend]], [[project_stage3_lp_deadend]] (5/7), [[project_stage1_steiner_deadend]] (6/7), [[project_net_hpwl_deadend]].

**Recommended plan:** (1) lead = levelized incremental forward-propagating timing oracle in V2 (~3-6wk; first milestone = reproduce evaluator TNS to within rounding OFFLINE before touching the matcher); (2) mandatory companion = case-adaptive alpha (~3-5d) because part of tc2 is NTU deliberately trading 88% more TNS for 16% less power ([[project_evaluator_vs_internal]]); (3) backup = cohesive NTU 5-step Stage B port (sidesteps the model via commit/revert asymmetry; defensible 0.988 ceiling); (4) LP-round/approx-bound = the real thesis chapter ([[thesis_direction]]) but GATED behind the oracle — inert/harmful on current model. Precondition: shatter doMatchingClustering + delete dormant LP-SEQ/FITP/Steiner first.
