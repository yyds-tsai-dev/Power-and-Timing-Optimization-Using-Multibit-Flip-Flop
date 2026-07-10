# Diagnosis: Oracle-vs-Evaluator TNS Gap (OPEN — highest-priority next session)

**Trigger**: C6 checkpoint experiment (sim-review demand): 63 paired dumps, median |oracleCost−eval| 0.048%, but **max 1.12% and systematic** — oracle UNDERSTATES evaluator on every case, worst on TNS-heavy ones (tc2 final 0.92%, hc02 1.12%, hc01 0.33%).

## Established facts (all verified today)

1. **Incremental accounting is EXACT vs our own full model**: [BIT_CHK] diff=0.000000 with the full stack incl. INTRA+BATCH+RESCAN (A-arm bisect). The drift is NOT in incr bookkeeping.
2. **Our full model ≠ evaluator**: at the same tc2 state (tri.out): clean-base-1hop TNS = 4,285.71 = internal getTNS; multihop computeAccurateTNS = 4,708.63; **evaluator-implied TNS = 5,333.7** (α=10; P/A+bins backed out via oracleCostSnapshot 682,654.6; eval score 735,991.07). NONE of our three metrics matches; evaluator is ~13% ABOVE multihop.
3. **This is the OLD ghost**: validateTNSOracle() hardcodes `[eval-true=7850.5]` vs contemporaneous multihop 5,864 — the 6/14 diagnosis ("internal 5864 vs eval 7850") was about the ACCURATE model, not the crude proxy. The 6/15 "faithful oracle" is faithful to computeAccurateTNS, which never closed the evaluator gap. Sim-review R1-W1 ("self-referential validation") was correct.
4. **Evaluator internals** (strings dump): `updateTiming(InstDB,float,bool)`, `updateArrival(int, vector<Pin>&, TimingVisitor*)`, `SinkDelay::arrival(int, const vector<Pin>&)`, per-pin setArrival/setSlack, "error propagation ... CLK pin" — it DOES forward-propagate arrivals (not 1-hop-from-anchor). So both propagate; the divergence is in semantics details.
5. Official paper numbers UNAFFECTED (all evaluator-measured). Current paper §3.4 wording already hedged (self-consistency + external evaluator closure).

## Candidate divergence mechanisms (untested)

- Required-time derivation: required_i = origArrival_i + origSlack_i — whose origArrival? If evaluator derives origArrival by propagating the ORIGINAL design and we anchor per-FF parse slacks differently at reconvergent nodes, MAX-path switches diverge.
- Gate arc constants (f.cnst) vs evaluator's recomputed base arcs.
- Multi-clock / CLK-pin exclusion semantics.
- Slack of bits on NEW cells (rebank-created): evaluator maps orig FF -> new pin; base-slack anchor may differ from ours.
- NOT the bin term (calculateBinDensityCost is ground-truth recompute; P/A trivially exact) — but verify bin-grid rounding semantics anyway.

## Next-session plan (highest priority)

1. **Single-FF displacement probes**: take official tc2 final .out; displace ONE chosen FF by one site (text edit); evaluator delta vs our models' predicted deltas. Choose probes: (a) FF with pure 1-hop FF sink; (b) FF driving 2-hop-through-gate sink; (c) FF at reconvergent fanin. Three probes should separate the hypotheses.
2. Per-FF slack diff harness: brute-force per-FF eval... evaluator gives only total — use probe deltas instead.
3. When mechanism found: fix computeAccurateTNS + incr oracle to TRUE evaluator semantics -> re-run unified config (expect score GAINS: better pricing) -> update paper §3/C6 with near-zero agreement + rerun official 3x.
4. Paper deadline fallback (if unfixed by 7/15): §3.1 goal wording already softened; C6 reported honestly as median 0.05% / max 1.1%; claims scoped to self-consistency + external closure. Weak-accept path intact.

## Artifacts
- tri.out + drift_b dumps in scratchpad; [CKPT] pairs in /tmp/v1_*_ckptrun.log + queue output bduo4a49p.
