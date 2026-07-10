# Reference STA vs C++ Oracle — Per-FF Slack Diff (2026-07-11)

**Type**: diagnosis
**Scope**: offline reference STA (`refsta.py`) built to the exactly-decoded
preliminary-evaluator semantics, validated against the evaluator binary, then
diffed per logical FF bit against `Manager::computeAccurateTNS` (V3, EVAL_ANCHOR=1).

Artifacts (session scratchpad, `/tmp/claude-5115/-vdahome-member-sweetcamper-ICCAD-Project/9381340c-d956-45d8-adf9-8fe6a9322fba/scratchpad/refsta/`):
`refsta.py` (reference STA), `ref_slack.tsv` (evaluator-exact per-bit slack),
`ref_slack_full.tsv` (same but full OUT2 propagation), `our_slack.tsv` (oracle dump),
`bsearch.py` (evaluator-bisection driver), `cadb_dump` (instrumented binary),
`dump_base.out` (byte-identical to `dac0/base_ea.out` — determinism verified with cmp).
New micro-testcases: `evsem/t6.txt`, `t7.txt`, `t8.txt`, `t9.txt` + solutions.

C++ change (V3 workspace, **not committed**): `src/Manager.cpp` —
`computeAccurateTNS` now honors `EVAL_DIAG_DUMP=<path>`: writes
`<origInst>/<origDpin>\t<slack>` per logical bit (keyed by original input names via
reversed `FF_list_Map`, same mapping Dumper uses), overwritten on each call so the
final `TNS_ORACLE_VALIDATE` call wins.

---

## 1. Three NEW evaluator semantics discovered (beyond the 2026-07-11 blackbox report)

All verified with hand-built micro-testcases that the evaluator matches to every printed digit:

1. **float32 timing arithmetic** (t7: banking + nonzero pin offsets).
   DisplacementDelay, per-arc `dd*HPWL` product, and node arrival accumulation are all
   computed in `float`, not `double`. A pure-double model is off by ~1e-4 per deep pin;
   the f32 pipeline reproduces `11302.0049014022` exactly.
2. **Unreachable D pins are dropped from TNS** (t6, t9). A D pin whose fanin cone
   contains no launch point (FF Q / input port) contributes nothing — even with a
   negative parse `TimingSlack` (t6: slack −100 pin scored as 0). "No timed arrival"
   ⇒ pin excluded, not "delta = 0".
3. **Only `OUT1` propagates a gate's arrival; `OUT2+` arcs are dead** (t9, plus tc2
   pin-level probes). A net driven by a gate's second output pin carries *no arrival*:
   a D pin fed only via an OUT2 arc is unreachable/dropped (t9: slack −450 scored 0,
   insensitive to FF moves), and at reconvergent gates the OUT2-side branch never
   competes in the max (tc2 `C50129`: evaluator max = the OUT1-chain branch
   23.888/26.677, refuting the stronger 28.041/27.427 OUT2-side branch to 14 decimal
   places on the isolated-pin probe). tc2 has 262 OUT2-driven nets (439 gate sinks,
   0 direct FF-D sinks).

Also confirmed: gates whose *cell* has zero IN pins (tie/constant cells, tc2 `G393`/
`G394`, 139 instances) are **not** launch points — their fanout is untimed
(consistent with rule 2).

## 2. Score validation (refsta vs evaluator)

- 22/22 micro-runs (t1–t9, all variants) match the evaluator to every printed digit.
- tc2 (`testcase2_0812.txt` × `base_ea.out`):
  - evaluator: **761159.492822677**
  - refsta: **761159.484631615** → |Δ| = 0.0082 (1.1e-8 relative)
  - decomposition of the 0.0082 residual (both parts are float-emulation noise, not semantics):
    - constant tail: evaluator's exact power+area tail is 682654.626871139 (measured
      with an all-slacks-=+1e9 input variant, TNS=0); double arithmetic gives
      682654.620800 (−0.00607). Per-item float rounding of `Gamma`/power inside the
      binary; no double/float combination we tried reproduces it exactly.
    - TNS: refsta 7850.486383 vs evaluator-implied 7850.486595 (−2.7e-8 relative) —
      f32 op-order differences in deep cones (our emulation double-rounds).
  - Sanity: refsta TNS 7850.49 matches the previously logged "eval-true = 7850.5" for
    this run; per-pin isolated probes (evaluator run with all other slacks +1e9) match
    refsta to f32 noise (e.g. C107265/D: eval −3.7890244 vs refsta −3.789023).

Given the per-FF diff threshold of 1e-2 used below, this precision is more than sufficient.

## 3. Per-FF diff: refsta (evaluator-exact) vs C++ oracle

Join key: original `inst/Dpin`. 21164 bits on both sides, none missing.

| |our−ref| bucket | bits |
|---|---|
| ≤1e-6 | 5458 |
| 1e-6..1e-4 | 15274 (f32-vs-double noise, no TNS impact) |
| 1e-4..1e-2 | 1 |
| 1e-2..1 | 35 |
| >1 (incl. dropped) | 396 |

TNS: refsta **7850.486** vs oracle **7446.094** → gap **−404.393**, i.e. exactly the
known 404.4 oracle underestimate. It decomposes into two mechanisms plus rounding dust:

### Mechanism A: parse-frozen "NULL" pins behind zero-input tie cells — **−398.71** (98.6% of gap)

- 427 bits have oracle slack **exactly equal to the parse `TimingSlack`** (arrChange=0).
  They are the oracle's `prev.instance == nullptr` bucket (EVALDIAG `NULL n=428`).
- Cause: `Preprocess::DelayPropagation` and `computeAccurateTNS`'s BFS both gate a
  node's pop on `visited/gateCnt == cell->getInputCount()`. The 139 tie-cell gates
  (cells `G393`/`G394`, **zero IN pins**) are never pushed (pushes happen only on event
  arrival), so every gate transitively fed by them never reaches its event count and
  freezes. FFs whose D-driver gate lies in the frozen cascade never get a
  `prevInstance` ⇒ `computeAccurateTNS` short-circuits to `origSlack`.
- Of those 427: **295** are genuinely unreachable (evaluator drops them; all have
  positive parse slack here except 0.60 TNS worth that the oracle *over*counts), but
  **132 are reachable through live paths** the evaluator does time. They are deep
  (depth 7–19), reconvergent, multi-launch (200–600 launch points) cones — the tie
  cell freezes the whole neighborhood even though the timed paths avoid it.
  Full-propagation TNS on these 132+295 bits = 399.31 vs oracle 0.60 → −398.71.
- Examples (our = oracle, ref = evaluator-exact):
  - `C100646/D`: our +0.0039 (=parse slack), true −15.171
  - `C100691/D`: our +0.0150, true −15.107
  - `C102390/D`: our −0.1094, true +12.575 (frozen the other way — oracle overcounts here)
- This is *not* evaluator-quirk emulation: with tie-freezing removed (refsta full
  propagation, double or f32) these 132 pins match the evaluator to ≤1e-4 —
  `oracle == refsta-full` on only 4/136 finite divergent pins, so the oracle's BFS
  freezing is a genuine bug, independent of the OUT2 quirk.

### Mechanism B: OUT2 arcs treated as live by the oracle — **−5.68**

- The oracle's netlist (`connectNet`) wires all `OUT*` pins identically and
  `computeAccurateTNS` propagates the per-gate arrival through every output pin.
  The evaluator kills OUT2 arcs (semantics #3).
- Net effect on tc2/base: 4 pins, −5.684 TNS. Example: `C107265/D`
  (parse slack +0.0626): evaluator −3.789 (OUT2-side parse-critical branch removed
  lowers the parse anchor, so current-side growth on the surviving OUT1 chain counts
  in full); full-propagation −0.386; oracle +0.0039* (this pin is *also* NULL-frozen —
  the two mechanisms overlap on it; the clean OUT2-only pins are the 4 where
  oracle==full≠eval).

### Residual

−404.393 = −398.708 (A) − 5.684 (B) − 0.0002 (f32 noise). Nothing unexplained.

## 4. Fix guidance for the oracle

In `computeAccurateTNS` (and mirroring in `refreshArrivalCorrections` /
`DelayPropagation` / the incremental cone recompute):

1. Seed zero-input gates into the BFS queue at start with arrival = −∞ sentinel so
   the cascade *unfreezes* (events flow) while tie-launched paths stay untimed —
   matching evaluator rule 2. On the FF step, an arrival that is still −∞ ⇒ drop the
   pin from TNS (do **not** fall back to `origSlack`); this also removes the +0.60
   overcount on the 295 unreachable pins.
2. When iterating `getOutputInstances()`, arcs from any output pin ≠ `OUT1` must
   deliver −∞ (dead), per evaluator rule 3.
3. For `prev.instance == nullptr` pins, consult `ffArr` first instead of freezing at
   `origSlack`.
4. Optional (last 2.7e-8): compute arrivals in `float` to mirror the evaluator's
   arithmetic; irrelevant next to A/B.

Affected cases beyond tc2: zero-IN-pin gate instances exist in testcase1_0812
(**1530 instances** — tc1 is one of our two losing cases, TNS-driven), testcase3 (21),
hiddencase03 (139), hiddencase04 (21). Expect mechanism A to be material on tc1.

## 5. Repro commands

```
# reference STA (OUT2_MODE=dead is default; zero/full for ablation)
python3 <scratch>/refsta/refsta.py testcase/testcase2_0812.txt base_ea.out --dump ref_slack.tsv

# oracle per-FF dump (binary rebuilt from V3 with the EVAL_DIAG_DUMP hook)
OMP_NUM_THREADS=8 BANKING_MODE=matching PRODUCTION=1 EVAL_ANCHOR=1 EVAL_DIAG=1 \
  EVAL_DIAG_DUMP=our_slack.tsv TNS_ORACLE_VALIDATE=1 \
  <scratch>/refsta/cadb_dump testcase/testcase2_0812.txt dump_base.out
```
