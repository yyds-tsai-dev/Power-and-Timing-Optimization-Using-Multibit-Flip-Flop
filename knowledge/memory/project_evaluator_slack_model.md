---
name: Evaluator slack model is per-sink HPWL
description: ICCAD Problem B evaluator charges slack per (driver_pin, sink_pin) pair as DisplacementDelay · Manhattan; not per-net RSMT or bounding-box
type: project
originSessionId: 337e23e6-9fa1-45e1-93b2-f42a788fbcff
---
The preliminary-evaluator binary (`2024-ICCAD-Problem-B[_V2]/evaluator/preliminary-evaluator`) computes slack per-sink, independently for each (driver, sink) pair, as `Δslack = DisplacementDelay · (HPWL(driver, sink, new) − HPWL(driver, sink, old))`. There is **no** per-net RSMT, bounding-box HPWL, or shared-backbone term. Strings-dump confirms: `SinkDelay::arrival(...)`, `QpinDelay`, `dispDelay`, `dSlack` — no `HPWL` / `Steiner` / `wirelength` / `bounding` symbols.

**Why**: Stage 1 Phase 2 Steiner-ΔWL regularizer (ff0f70f reverted algorithm, kept dormant FLUTE infra) regressed 6/7 cases because it optimized an objective the scorer does not reward. Plan `plan_stage1_steiner_cost.md` §2 Deliverable B premise ("HPWL under-counts co-sink cost") was wrong. See `reports/v2/2026-04-20_postmortem_stage1_steiner-cost.md`.

**How to apply**: Before adding any cost term beyond per-sink `DisplacementDelay · HPWL(driver_pin, sink_pin)`, verify the evaluator actually charges that term — strings-dump the binary and look for the model's keywords. Proposals involving tree topology, bounding-box cost, Steiner points, or per-net aggregates are epistemically wrong for TNS in this problem. They may still be valid for other costs (Power, Area, CellDensity) but those already have explicit scorer formulas that don't need proxying.
