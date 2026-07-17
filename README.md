# Exact-Oracle Post-Placement Refinement for MBFF Banking

Solver for **ICCAD 2024 CAD Contest Problem B** (multi-bit flip-flop banking + placement).
Fork of [coherent17's open-source contest flow](https://github.com/coherent17/2024-ICCAD-Problem-B), first strengthened in the front end (max-weight-matching banking, per-pin pricing, DP repairs), then rebuilt from detailed placement onward around an **evaluator-exact, side-effect-free incremental timing oracle** that prices every refinement move against the official cost function.

---

## Results (official evaluator, 2026-07-11 record runs)

![Score Comparison](docs/score_comparison.png)

One **unified configuration** — a single environment recipe, zero per-case hyperparameters — beats the contest top-3 best entry on **all seven cases**. Every solution passes the official `preliminary-evaluator` (`Check pass`) plus the contest `sanity` and `placement_checker`.

Current record = **v2** (unified + `EVAL_ANCHOR=1`: the timing oracle anchored at parse-time truth with evaluator-exact propagation semantics — tie-cell unfreeze, unreachable-drop, primary-output-only arcs). The 2026-07-06 pre-exactness record is kept for comparison: the delta between the two columns is the *passive* dividend of making the internal pricer agree with the official evaluator, with zero operator changes.

| Testcase | Contest top-3 best* | 2026-07-06 record | **v2 (2026-07-11)** | v3 (2026-07-13)† | v4.1 (2026-07-17)‡ | Δ v4.1 vs top-3 |
|---|---:|---:|---:|---:|---:|---:|
| testcase1_0812 | 739,235,861 | 734,319,834 | 734,266,279⁑ | 734,700,023 | **734,281,585** | −0.67% |
| testcase2_0812 | 744,231 | 723,165 | 717,378 | 705,141 | **703,220** | −5.51% |
| testcase3 | 727,971,140 | 726,041,482 | 725,877,524 | 725,803,320 | **725,796,410** | −0.30% |
| hiddencase01 | 31,507,917 | 30,062,537 | 30,065,106 | 29,993,041 | **29,952,245** | −4.94% |
| hiddencase02 | 13,408,414 | 10,089,886 | 10,028,140 | 9,678,013 | **9,621,649** | −28.24% |
| hiddencase03 | 55,941,500 | 55,781,010 | 55,772,291 | 55,771,347 | **55,769,936** | −0.31% |
| hiddencase04 | 728,497,353 | 726,015,652 | 725,906,134 | 725,980,968 | **725,936,994** | −0.35% |

**Composite ratio: 0.952 (2026-07-06) → 0.9500 (v2) → 0.9437 (v3) → 0.9428 (v4) → 0.9424 (v4.1, machine-B verified‡)** against the top-3-best baseline. Strongest published methods on this benchmark reach 0.979 (DATE'26) and 0.991 (DAC'25 LBR, recomputed under the same baseline).
\*exact per-case top-3 costs as re-evaluated in the DATE'26 study.
†v3 = v2 + convergence termination (`CONV_TERM`) + single-axis adaptive operator stacking (`ADAPT_STACK`), frozen env. Machine-B verification 2026-07-13: 7/7 byte-identical across repeats, thread counts 8–128, ambient load, and rebuilt binaries.
‡v4.1 = v3 + rebank modes 4/8 (`REBANK_MODES=15`) + exact-priced regional LNS with bit-splitting destroy (`LNS_KICK/LNS_SPLIT`, K=4) + a third interleave round (`ALT_ROUNDS=3`, which pays only once the two new move classes exist) — still one uniform recipe, zero per-case hyperparameters; beats v3 AND v4 on all seven cases. Machine-B rituals complete (v4 2026-07-14, v4.1 2026-07-17: rep2 byte-identical 7/7 each; thread-invariant at 128T). Cross-machine reconciliation with machine A pending for official promotion.
⁑the v2-era tc1 number is irreproducible on machine B under any archived binary/env combination (see `knowledge/reports/2026-07-16_diagnosis_tc1_v2_record_provenance.md`); v4.1 is within +0.002% of it.

Reproducibility: deterministic in practice — six of seven cases are byte-identical across repeats (hiddencase04 has one floating-point-noise accept, ~2×10⁻⁴% of score). Scores are additionally **invariant to thread count and ambient load** on the non-wall-clock-capped cases (bit-identical across 8/16/32/64 threads, verified on two machines); on the capped cases (tc3, hc04, hc02) more threads means more exact-priced work inside the same time budget, i.e. equal-or-better scores at a fraction of the wall clock. Reference machine: 2.2 GHz Xeon E5-2630 v4, 8 threads, <4 GB RAM per case; full suite ≈ 3–5 h (≈ 70 min at 64 threads on a 4-socket Xeon Gold).

---

## Method

```
inherited front end (proxy-priced)          refinement layer (exact-priced, this work)
─────────────────────────────────           ──────────────────────────────────────────
parse → debank → global place →       →     ┌ batch bit re-pairing  ┐
cluster → bank → legalize → DP              │ structural rebanking  │ ←→ incremental
                                            │ relocation / swap     │     timing oracle
                                            │ merge ejection        │  (evaluator-exact,
                                            └ bin-density eviction  ┘   side-effect-free)
                                            interleaved rounds to convergence → dump
                                            → official evaluator + both checkers
```

**Oracle.** The pricing model is **proven evaluator-exact**: the official evaluator's semantics were pinned by black-box micro-testcases plus full binary disassembly, reproduced in an offline reference STA, and the internal oracle audited per-bit against it — residuals on all cases sit at float32-quantization noise. All timing state is keyed by immutable logical objects (debanked single-bit FFs and gates), so banking never invalidates a cache; positions and clock-to-Q delays are read live through one logical→physical indirection. Three modes share a single cone-propagation core: full pass (validation ground truth), incremental cone recompute (after commits), and a side-effect-free delta mode that prices a hypothetical move through a per-bit override map in thread-local scratch — thousands of candidates are screened concurrently against one frozen state (~4,000 candidates/s at 8 threads; an exact trial-apply ≈ 12 ms). Incremental vs. full recomputation agrees to 0.000000 every round; deltas match apply-and-revert to 7.4×10⁻¹³.

**Operators.** Batch bit re-pairing (inter-cell exchange + intra-cell slot permutation; exact re-pricing at apply time replaces conflict-disjoint batching, an order-of-magnitude commit-throughput gain at identical safety) · structural rebanking (2×2b→4b, 4×1b→4b; parallel screening at centroid → best-first staged exact trials with full rollback) · merge ejection (the inverse move: splits merges that exact pricing exposes as unprofitable; strict 7/7-case win on top of the converged configuration) · relocation & critical-pair swap (inherited move generators, re-priced by the oracle) · bin-density eviction (prices the λ·V term together with exact timing). All structural commits are monotone in the official cost; the schedule is anytime — interrupting at any stage boundary yields a legal solution, and the official score descends monotonically across all recorded checkpoints.

---

## Quick Start

```bash
make boost          # first time only (downloads boost headers; header-only, no b2 needed)
make release        # NDEBUG build → ./cadb_0015_final
# g++ ≥ 12 without glibc-static: build dynamically instead —
#   make release CXXFLAGS="-I ./inc -I ./lib -I ./boost_1_84_0 -std=c++14 -fopenmp -pipe" WARNINGS="-g -Wall"
# unified v2 production configuration (the record above):
env OMP_NUM_THREADS=8 BANKING_MODE=matching PRODUCTION=1 INCR_RELOC=1 \
    RELOC=1 CRIT_SWAP=1 BIT_REPAIR=1 BIT_REPAIR_DYNA=2 BIT_REPAIR_RESCAN=4 \
    BIT_REPAIR_BATCH=1 REFINE_ALLFF=1 BIT_REPAIR_INTRA=1 ALT_ROUNDS=2 \
    BIT_REPAIR_TIME=1200 ORACLE_REBANK=1 REBANK_TIME=600 DENSITY_REPAIR=1 \
    ORACLE_EJECT=1 EJECT_TIME=180 EVAL_ANCHOR=1 \
    ./cadb_0015_final testcase/testcase2_0812.txt out.txt
# verify with the official tools:
evaluator/preliminary-evaluator testcase/testcase2_0812.txt out.txt
checker/sanity testcase/testcase2_0812.txt out.txt
checker/placement_checker testcase/testcase2_0812.txt out.txt
```

Requirements: g++ ≥ 9 (OpenMP), network for `make boost`. LEMON is vendored under `lib/lemon`. OR-Tools is optional (experimental LP path only, off by default).

### Configuration switches

Every algorithmic gate is an environment variable and is **byte-exact when off** — setting any gate to 0 reproduces the pre-feature behavior exactly.

| Switch | Meaning |
|---|---|
| `BANKING_MODE=matching` | max-weight-matching banking (mandatory for scoring runs) |
| `BIT_REPAIR=1` + `BIT_REPAIR_BATCH/INTRA/RESCAN/DYNA` | batch bit re-pairing family (budget `BIT_REPAIR_TIME`, s) |
| `REFINE_ALLFF=1` | full move space (removes a stale work-queue-flag exclusion) |
| `ORACLE_REBANK=1` | oracle-priced structural rebanking (budget `REBANK_TIME`, s) |
| `ORACLE_EJECT=1` | oracle-priced merge ejection (budget `EJECT_TIME`, s) |
| `DENSITY_REPAIR=1` | bin-density eviction with combined timing+density pricing |
| `ALT_ROUNDS=2` | interleaved operator rounds |
| `EVAL_ANCHOR=1` | **v2**: evaluator-exact oracle semantics (parse-true anchors, tie-cell unfreeze, unreachable-drop, primary-output-only arcs) |
| `CONV_TERM=1` + `CONV_RATE` | convergence termination for budgeted stages (time caps degrade to a 3× fuse) |
| `ADAPT_STACK=1` (+`ADAPT_STACK_TH`) | single-axis adaptive operator stacking: enables downstream-priced banking + oracle-priced cross-slot moves when the design's TNS share exceeds the threshold (default 0.05) |
| `DP_SLOT_ORACLE=1` | exact-oracle accept gate on cross-MBFF slot windows |
| `REBANK_MODES` (+`REBANK_HR_FLOOR`) | rebank move set: 1=2b+2b→4b, 2=4×1b→4b, 4=2b+1b+1b→4b, 8=1b+1b→2b; optional headroom-floor candidate filter |
| `LNS_KICK=1` | large-neighborhood regional rebundle transactions (experimental) |
| `EVAL_CHECKPOINT=<prefix>` | dump a scoreable solution at every stage boundary |
| `PROBE_MOVE` / `PROBE_CELL` | single-cell perturbation harnesses (model-vs-evaluator audits) |

---

## Repository layout

```
src/ inc/ main.cpp     flow + refinement implementation (C++14, OpenMP)
evaluator/ checker/    official contest scoring and legality tools
lib/lemon              vendored LEMON (max-weight matching)
scripts/               benchmark / sweep helpers
docs/                  charts
```

Branches: `main` = this release line; `v3_experimental` = development head.

## Credits

Base flow: [coherent17/2024-ICCAD-Problem-B](https://github.com/coherent17/2024-ICCAD-Problem-B) (ICCAD 2024 contest reference-flow implementation). Contest benchmarks, evaluator, and checkers © ICCAD 2024 CAD Contest organizers.
