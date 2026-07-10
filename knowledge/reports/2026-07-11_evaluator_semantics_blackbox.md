# Preliminary-Evaluator TIMING Semantics — Black-Box Decode (2026-07-11)

Binary: `2024-ICCAD-Problem-B_V3/evaluator/preliminary-evaluator <input> <output>`
Method: hand-crafted minimal testcases (1-bit FFs, BUF/AND2 gates, one row, one bin, all pin offsets 0, DisplacementDelay=0.01, Alpha=10, Beta=Gamma=Lambda=1e-6). Every observed score matched exactly one candidate model with **0 arithmetic error** (not just <0.1%).

Artifacts: `/tmp/claude-5115/-vdahome-member-sweetcamper-ICCAD-Project/9381340c-d956-45d8-adf9-8fe6a9322fba/scratchpad/evsem/` (t1–t5 `.txt` inputs + `.out` solutions).

## Decoded semantics (pseudocode)

```
# Static timing with launch-side arrival only; required times are implicit
# via the parse-time slack anchor. Gates contribute ZERO intrinsic delay.

def arrival(D_pin, placement):                       # max over ALL fanin paths
    return max over every combinational path P from a launch point L
               (L = some FF Q pin, or a primary Input port) to D_pin of:
        Qpd(current libcell of L if FF else 0)
      + DisplacementDelay * sum over arcs (u→v) in P of
            HPWL_2pt(pos(u), pos(v))                 # per-sink two-point HPWL,
                                                     # pin pos = inst pos + pin offset;
                                                     # NOT net bounding box

def slack(D_pin, out_placement):
    return parseSlack(D_pin)                          # TimingSlack line, anchored at
         - ( arrival(D_pin, out_placement)            #   the INPUT-file placement
           - arrival(D_pin, parse_placement) )        # frozen parse-time reference

TNS   = sum over all FF D pins (per bit) of max(0, -slack(D))
Score = Alpha*TNS + Beta*sum(GatePower of FF cells)
      + Gamma*sum(area of FF cells)                   # gates excluded from power/area
      + Lambda*binDensityPenalty
```

Key properties proven:
1. **Max–max reconvergence**: at a multi-fanin D pin the evaluator compares
   `max` of current arrivals against `max` of parse arrivals. It is true STA —
   NOT per-path deltas summed, NOT a critical-launcher frozen at parse time.
   Making a non-critical path longer only hurts once it exceeds the current max;
   shortening the critical path only helps down to the second-most-critical path.
2. **Uncapped improvement**: slack may rise arbitrarily above its parse value
   (rectified only by `max(0,-slack)` inside TNS).
3. **Qpd**: launcher `QpinDelay` (of the *current* mapped cell type) enters the
   arrival; propagates through ≥2 gate levels; counted **once** even when the
   same launcher reconverges via two paths (max, no double count).
4. **Ties at parse** are harmless: pure max, no order-dependent launcher pick.
5. **Input-port launch points** work identically with Qpd = 0; moving an FF
   changes its own D-pin slack via the port→D arc.
6. Per-sink two-point HPWL confirmed structurally: a 2-fanout Q net charges each
   branch its own driver→sink HPWL (net-bbox model would have predicted TNS 550
   in T4 base; observed 450 = per-sink).

## Experiment log

Common lib: FFA (Qpd 10), FFB (Qpd 20, same 10x100 size, same power 0.01), BUF, AND2; DispDelay 0.01; Alpha 10. FF area term ≈ +0.001/FF, power ≈ +1e-8/FF (visible in the constant tails 0.002…/0.003…).

### T1 — calibration (IPT1(0,0) → F1(0,0) → G1(100000) → F2(110000); slacks F1=+1e5, F2=−3)
| Run | Move | Predicted TNS → score | Observed |
|---|---|---|---|
| t1_base | none | 3 → 30.002 | 30.00200002 ✓ |
| t1_m1 | F1 → x=100 | 2 → 20.002 | 20.00200002 ✓ |
| t1_m2 | F1 → x=50000 | slack −3+500=+497 → 0 → 0.002 | 0.00200002 ✓ (improvement crosses 0, uncapped) |
| t1_m3 | F2 → x=111000 | slack −13 → 13 → 130.002 | 130.00200002 ✓ |

### T2 — reconvergence discriminator (A(20000)→G1(90000), B(60000)→G2(80000), GS(100000)→FS(110000); parse arr: via A 910 (critical), via B 510; FS slack −450)
| Run | Move | max–max | per-path | frozen-critical | Observed |
|---|---|---|---|---|---|
| t2_base | none | 450 | 450 | 450 | 4500.003 = 450 |
| t2_a | B → x=0 (arrB 1110 > 910) | **650** | 1050 | 450 | 6500.003 = **650** → max–max |
| t2_b | A → x=89000 (arrA 220) | **50** (new max = arrB 510) | — | 0 | 500.003 = **50** → max–max |
| t2_c | A → x=20100 | **449** (uncapped) | — | 450 if capped | 4490.003 = **449** → uncapped |

### T3 — parse-time tie (B moved to 140000 in input ⇒ arrA = arrB = 910)
| Run | Move | Predicted (max) | Observed |
|---|---|---|---|
| t3_base | none | 450 | 4500.003 ✓ |
| t3_a | B → 150000 (arrB 1010) | 550 | 5500.003 ✓ |
| t3_b | A → 30000 (arrA 810; max stays 910) | 450 | 4500.003 ✓ (no frozen-launcher artifact) |

### T4 — Qpd propagation & double-count (single launcher A fans out to G1 and G2, reconverges at GS→FS; both paths 910; FS slack −450)
| Run | Change | once (max) | double-count | Observed |
|---|---|---|---|---|
| t4_base | none | 450 | — | 4500.002 ✓ (also rules out net-bbox HPWL, which predicts 550) |
| t4_swap | A: FFA→FFB (+10 Qpd) | **460** | 470 | 4600.002 = **460** → counted once, reaches sink through 2 gates |
| t2_swapA | critical launcher A→FFB | 460 | — | 4600.003 ✓ |
| t2_swapB | non-critical B→FFB (520<910) | 450 | — | 4500.003 ✓ (Qpd inside the max) |

### T5 — input-port arc + simultaneous multi-pin closure (F1 at 50000, slack −2; F2 slack −450)
| Run | Move | Predicted | Observed |
|---|---|---|---|
| t5_base | none | 2+450=452 → 4520.002 | 4520.002 ✓ |
| t5_m | F1 → 49000 | F1: −2+10=+8→0; F2: −460 → TNS 460 → 4600.002 | 4600.002 ✓ |

## Consequences for our internal cost model
- Any internal TNS oracle must do a real **arrival-max** propagation per D pin
  (per-sink 2-pt HPWL arcs, launcher Qpd of the *current* cell, zero gate delay),
  and diff against the parse-time arrival-max — not per-path/1-hop deltas.
  This is exactly the "1-hop timing" mispricing root cause tracked in
  `project_tc2_verified_diagnosis.md`: shortening the critical path is clipped
  by the 2nd-critical fanin, and lengthening a non-critical path is free until
  it becomes critical — both effects invisible to a single-path slack update.
