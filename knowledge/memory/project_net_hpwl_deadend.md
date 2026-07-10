---
name: Net HPWL hypothesis was wrong
description: Evaluator uses per-sink two-point HPWL not net HPWL; confirmed via binary strings analysis; all net HPWL approaches (veto, decluster, full replacement) failed
type: project
originSessionId: 035095eb-9656-4485-9aa8-cec726920b80
---
Net HPWL hypothesis DISPROVED (2026-05-09): the evaluator uses per-sink two-point HPWL (`SinkDelay` with `inputPins.size()==1`), identical to our model. Verified via `strings` on evaluator binary.

Tested and failed:
- `NET_HPWL=1`: full net HPWL replacement in CostCompare
- `NET_HPWL_VETO=1`: veto merges that fail net HPWL cost check — too aggressive, rejects profitable merges
- `NET_HPWL_DECLUSTER=1`: post-LG decluster using net HPWL delta — 0 candidates found

**Why:** Per-merge cost model improvements CANNOT close the tc2 gap. With alpha=10, beta=400, power savings dominate individual merges. The 34% TNS gap is collective "death by a thousand cuts" from incremental timing errors, not per-merge mispricing.

**Real root cause:** Incremental timing error accumulation. `getSlack()` (FF.cpp:377) uses 2-hop incremental computation. Evaluator does full BFS propagation. `refreshArrivalCorrections()` (Manager.cpp:2486) can fix this but is default OFF in production.

**How to apply:** Stop pursuing per-merge cost model refinements. Focus on `refreshArrivalCorrections()` (ACCURATE_BANKING=1) and inter-batch BFS refresh to reduce incremental timing errors during banking.
