# Current Work

*Update this frequently. Replaces old `progress.md`.*

**Last updated**: 2026-04-19

---

## V1 (`2024-ICCAD-Problem-B/`, branch `main`)

**Current head**: `c1d2edd [RA/1] third_party: integrate OR-tools 9.15 prebuilt + toy set-partition MILP`

**Recent shipped work** (per git log):
- `[P4/1]` Legalizer `FindTop2LegalCoors` read-only probe
- `[P4/2]` Banking 2-bit defer-and-batch (NTU Algorithm 1 + N3 lookahead, env-gated `ALG1_2B`)
- `[P4/3]` Banking HB defer-and-batch + option (ii) downstream margin + SA spike
- `[RA/1]` OR-tools 9.15 integration + toy set-partition MILP

**Open loose ends**:
- P4 HB on hc02 still +76% (4-component decomposition in `plans/active/p4_hb_diagnosis.md`)
- Need attribution validation sweep before committing to option (ii) or Path B

## V2 (`2024-ICCAD-Problem-B_V2/`, branch `arch_rewrite`)

**Status**: workspace freshly initialized from V1 HEAD. No V2-specific work yet.

**Planned direction**: approximation algorithms for NP-hard 4-bit k-set packing. Candidates under evaluation:
- Berman 3-swap local search (provable 2.5+ε ratio)
- Path B local-search refinement on cascaded matching
- LP relaxation + rounding (OR-tools infra already in place)
- Hybrid 3-swap warm-start + SA polish
- Lagrangian relaxation (thesis ceiling, see `plans/active/plan_lagrangianMatching.md`)

Not yet committed to a specific path — decision pending.

## Cross-workspace

**Coordination points**:
- DP bug fixes (ChangeCell OMP race, GlobalSwap rtree, K-nearest) — V1 scope
- CostCompare internals — being worked on in parallel (coordinate before V2 layers on top)
- B1/B2/B3 contest competitor trick absorption — V1 scope

**Reference docs**:
- `plans/active/p4_hb_diagnosis.md` — 4-component failure-mode decomposition
- `plans/active/p4_path_b_design.md` — Path B local search design sketch
- `plans/active/p4_v1_baseline.md` — V1 baseline numbers for regression tracking
- `plans/active/plan_unified_singleSession.md` — Phase 3Z (superseded by P4 direction but still referenced)
