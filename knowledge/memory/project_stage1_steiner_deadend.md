---
name: Stage 1 Steiner CostCompare dead-end
description: Plan_stage1_steiner_cost.md archived; both deliverables (hop-2, RSMT ΔWL) regressed 7-case sweep; FLUTE infra kept dormant
type: project
originSessionId: 337e23e6-9fa1-45e1-93b2-f42a788fbcff
---
Stage 1 of `plan_beat_ntu_longterm.md` is closed as dead-end.

- **Deliverable A (2-hop downstream `computePinTNS`)**: reverted `14d3e13`. Narrow intra-cluster variant double-counted `(f2.Q→hop2.D)` across `cf=f1` and `cf=f2` iterations; tc2 +6.55%, hc02 +79.8%.
- **Deliverable B (Steiner ΔWL regularizer)**: reverted; Phase 2 infra kept at `ff0f70f`. Full sweep regressed 6/7 (steiner−baseline: tc1 +0.14%, tc2 +1.26%, tc3 +0.08%, hc01 +0.33%, hc02 +6.05%, hc04 +0.08%; hc03 noise).

**Why**: model mismatch with evaluator's per-sink HPWL model (see memory `project_evaluator_slack_model`).

**How to apply**: FLUTE is available via `make setup_flute` (fetches `bsg-external/flute`), wrapper at `inc/SteinerTree.h` / `src/SteinerTree.cpp`, Makefile gates on `HAVE_FLUTE`. Usable for future Hanan-grid / useful-skew / buffer-aware experiments. Do **not** reintroduce it as a TNS / CostCompare term without first verifying the evaluator model has changed. Env gates `DOWNSTREAM_HOP`, `ALG2_STEINER_COST`, `ALG2_STEINER_WEIGHT` still parseable but inert; reuse rather than re-plumb if a new experiment needs them.

Report: `Project Knowledge/reports/v2/2026-04-20_postmortem_stage1_steiner-cost.md`.
Plan archived at `Project Knowledge/plans/archived/plan_stage1_steiner_cost.md`.
Long-term plan moves to **Stage 2** — candidate directions (LP round, 3-swap SA, Path B cascade) listed in `plan_beat_ntu_longterm.md`.
