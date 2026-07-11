# DAC 2026 Proceedings/Program Sweep — MBFF Competition Scan (2026-07-11)

**Scope**: DAC 2026 program sweep for competition to our ICCAD-2024 Problem B / MBFF work, ahead of the DAC 2027 submission. Novelty claims being defended: (a) exact-oracle refinement, (b) black-box evaluator decode, (c) LNS destroy-repair, (d) headroom harvesting.

## 0. Timing correction (important)

**DAC 2026 has NOT taken place yet.** The 63rd DAC ("Chips to Systems") runs **July 26-29, 2026, Long Beach CA** — ~2 weeks from today. There are no proceedings yet; however the **full technical program with abstracts is already public** at `63dac.conference-program.com`, which is what this sweep is based on. Re-sweep after July 29 for final PDFs (ACM DL) and any LBR/WIP content not indexed in the program search.

---

## 1. B-Flex — NOT a Problem B / MBFF-banking competitor

- **Title**: *B-Flex: Exploration of Broader Flip-Flop Design Space Based on FSM Exhaustive Search*
- **ID/Session**: RESEARCH2394, session EDA5 "RTL/Logic Level and High-level Synthesis", Mon Jul 27, 12:03pm, Room 201B
- **Authors**: Wanyeong Jung (KAIST), Kyounghun Kang (KAIST), Hyunsung Jeong (UNIST), Jongeun Lee (UNIST)
- **Content**: Automated **flip-flop circuit/topology design** (cell design, not placement). Prior logic-based FF design methods are "limited to 2-bit finite state machines because of inefficient search space representation"; B-Flex expands the FSM exhaustive-search space to discover new FF circuit topologies for PPA. UNIST lab news frames it as "a new way to the flip-flop design problem ... EDA in the age of AI".
- **Problem B / MBFF banking relevance**: **None found.** No mention of multi-bit flip-flops, banking, placement, or ICCAD contest benchmarks in the abstract. It sits in the logic-synthesis track, not physical design.
- **Threat level: LOW (none).** Different problem entirely (transistor/FSM-level FF design vs. post-placement banking). Zero overlap with our four novelty claims. At most a citation in intro ("FF-related PPA optimization spans cell design [B-Flex] to physical banking [ours]").

## 2. NTU winner-team LBR — the real competitive item at DAC 2026

- **Title**: *Late Breaking Results: A Unified Analytical Framework for MBFF Clustering and Placement for Timing, Power, and Area Co-Optimization*
- **ID/Session**: LBR060, LBR poster session (sess308), Mon Jul 27, 5:35pm, Exhibit Hall
- **Authors**: Chuan-Chi Su, Yu-Sheng Yang, Cheng-Yen Li, Shao-Hsiang Chen, **Yao-Wen Chang** (all National Taiwan University) — this is the **ICCAD 2024 Problem B winning-team lineage** (Cheng-Yen Li = P7/LBR author, the 0.977-composite ceiling we track).
- **Content (from abstract)**: Unified **analytical** MBFF clustering + placement framework addressing "industrial compatibility constraints" while jointly optimizing timing/power/area. Components: (1) efficient timing and power models, (2) **prioritized multi-density maps**, (3) a **clustering-affinity mechanism** guiding FFs toward compatible merging regions.
- **Key claim**: "**Outperforms all teams in the 2025 ICCAD CAD Contest on Power and Timing Optimization Using Multi-Bit Flip-Flops**, being the only approach to generate valid solutions across all testcases." They also "refined the imbalanced contest library to better reflect clustering performance" and claim consistent superiority under the refined library.
- **Threat level: HIGH (strategic), MEDIUM (technical overlap).**
  - Strategic: (i) confirms **ICCAD 2025 re-ran Problem B as an MBFF power/timing contest** (invited paper: IEEE Xplore 11240779) — DAC 2027 reviewers will likely expect evaluation on the **2025 benchmark suite**, not just 2024; (ii) NTU keeps publishing yearly LBRs (DAC 2025 LBR on pre-placed cells → DAC 2026 LBR on unified analytical framework) — expect a **full paper by ICCAD 2026 / DAC 2027** that could collide with our submission window; (iii) "beats all 2025 teams" resets the SOTA narrative.
  - Technical: mechanism is analytical/global (density maps + affinity), **not** exact-oracle refinement, not evaluator decode, not LNS destroy-repair, not headroom harvesting. Our four claims remain unclaimed by them, but "TPA co-optimization on ICCAD MBFF contest, beats winners" as a headline is now contested ground.
  - Action: obtain the 2025 contest benchmarks + evaluator; plan a 2025-suite column in DAC 2027 experiments; cite this LBR and differentiate on mechanism (oracle-exact incremental costing vs. approximate analytical models — their models are still the same 1-hop/analytical class our diagnosis showed is the root blocker).

## 3. TIMBER (ASP-DAC 2026, already published Jan 2026) — direct benchmark competitor

- **Title**: *TIMBER: A Fast Algorithm for Timing and Power Optimization using Multi-bit Flip-flops* (IEEE Xplore 11420570; PDF: tsung-wei-huang.github.io/papers/2025-ASPDAC-TIMBER.pdf)
- **Authors**: Aditya Das Sarma (UW-Madison), Shui Jiang (CUHK), Wan Luan Lee (UW-Madison), Tsung-Yi Ho (CUHK), Tsung-Wei Huang (UW-Madison)
- **Content**: Post-placement MBFF banking/debanking **directly on the ICCAD 2024 Problem B formulation and official benchmarks** (they even acknowledge team cadb0027 for sharing the winning binary). Pipeline: register-candidate formation → window-based (15×15) segment/interval placement-candidate generation (Chow/Pui/Young DAC'16-style) → greedy candidate evaluation (local cost = λ·BDV + α·ω·displacement) → queue-based local legalization. Region-partitioned multithreading (up to 72× speedup).
- **Headline claim**: avg **13.08× better final score than the 2024 first-place winner**, zero BDVs, 5.06× faster, 3.56× less memory.
- **Reality check (from their Table III)**: the 13.08× is almost entirely **BDV arbitrage** — at their checker settings the 1st-place binary incurs 1-14 bin-density violations at λ=1e8 each (case4: 14 BDVs → 1.43e9 vs their 3.46e7 → "41×"). On pure power/area/timing geomeans they are only "comparable" (power geomean actually worse: 0.025 vs 0.017), and they **lose to the winner on case2** (1.43e6 vs 7.47e5) — the same low-β timing-dominated case that is our sole gap. Their claimed 1st-place BDVs also disagree with official contest scoring (winner passed the contest checker), suggesting a different ϵ/BW/BH setting in their runs — reproducible-check candidate.
- **Threat level: MEDIUM.** Same benchmarks, "beats the winner" headline, published first. But: no oracle, no evaluator decode, no LNS, no headroom harvesting — it is a fast greedy single-pass method whose win is density-constraint handling and runtime. For DAC 2027 we must cite it and (ideally) run it or its numbers alongside ours; our positioning: TIMBER trades quality for speed and wins on a penalty term, whereas we beat the winner on the actual PPA cost (V3 composite 0.977 territory). Their normalization methodology (Monte-Carlo TNS max) is also easy to critique.
- **Bonus intel**: cadb0027 (2024 1st place) shares their binary on request — useful for our own head-to-head tables.

## 4. LATTE (DAC 2026 research paper) — adjacent, timing-driven DP

- **Title**: *LATTE: Legality-Assured Differentiable Timing-Driven Detailed Placement*
- **ID/Session**: RESEARCH373 (sess320 listing; talk in EDA6 session), Wed Jul 29, 3:30pm, Room 202AB
- **Authors**: Yi-Chen Lu (NVIDIA), Jing Mai (Peking University), et al.
- **Content**: Differentiable timing-driven **detailed placement** with legality assurance, "leveraging gradients that encompass full-chip context".
- **Threat level: LOW-MEDIUM.** Overlaps our "timing-driven detailed placement" territory but via differentiable gradients, not exact/evaluator-in-loop costing, and not MBFF/banking-aware. Must-cite related work for the DP chapter of DAC 2027; no claim collision with oracle-exact refinement (our DP moves are scored by the exact cost, theirs by smoothed surrogates — a clean differentiation axis).

## 5. Other DAC 2026 program hits (checked, non-threats)

- *Split-and-Sync Bayesian Learning-Driven SRAM Compiler: Automatic Design Tuning with Optimal Banking* (WIP poster, Kwangwoon U./Yonsei) — SRAM macro banking, irrelevant.
- Program searches for **"ICCAD"**, **"contest"**, **"detailed placement" (exact phrase)**, **"register clustering"**, **"neighborhood"/"destroy-repair"/LNS** returned **no matching papers** — i.e., no one at DAC 2026 is visibly publishing ICCAD-contest-benchmark work besides the NTU LBR, and **nobody claims LNS destroy-repair or evaluator-in-loop/exact-oracle costing for MBFF**.
- Timing-driven placement session (EDA6, Wed Jul 29) is crowded but orthogonal: FPGA GP (learning-based slack-aware), 3D-IC TDP (PKU/CUHK, Southeast U.), net-pin weighting for TDGP (Fuzhou U.), MediaTek RL macro placement — none touch FF banking or contest scoring.

## 6. Non-DAC background items surfaced during sweep

- **2025 ICCAD CAD Contest Problem B: Power and Timing Optimization Using Multibit Flip-Flop** (invited paper, IEEE Xplore 11240779) — Problem B was **re-run at ICCAD 2025**. Winners for 2025 Problem B not found in public sources during this sweep (iccad-contest.org winners page now shows the 2026 cycle, "winner not announced yet"); CUHK+NTHU+Giga won 2025 Problem C. TODO: pull the 11240779 PDF via institutional access to get 2025 benchmark deltas and top-3 teams.
- *Timing-Driven Multi-Bit Flip-Flop Allocation Utilizing Design-Technology Co-Optimization Techniques* (IEEE 11310917) and *Multi-Bit Flip-Flop Based Timing and Power Optimization under Advanced Technology Nodes* (IEEE 11044076) — 2025 venues, DTCO/cell-level angle; low threat, cite-if-relevant.
- NTU DAC 2025 LBR (Li, Su, Z.-W. Chen, S.-H. Chen, Chang): *Multi-Objective MBFF Placement Considering Pre-Placed Cells* — predecessor of LBR060, already known (P7 lineage).

## 7. Bottom line for DAC 2027 novelty claims

| Our claim | Contested at DAC 2026? | By whom |
|---|---|---|
| Exact-oracle refinement (evaluator-exact incremental costing) | **No** | — |
| Black-box evaluator decode | **No** | — |
| LNS destroy-repair for MBFF banking | **No** | — |
| Headroom harvesting | **No** | — |
| "Beats ICCAD 2024 Problem B winner" headline | **Yes (partially)** | TIMBER (ASP-DAC'26, BDV-driven 13×), NTU LBR060 (beats all *2025* teams) |
| Benchmark currency | **At risk** | 2025 Problem B re-run exists; NTU already evaluates on it |

**Actions**: (1) acquire ICCAD 2025 Problem B benchmarks/evaluator and add to our experiment matrix; (2) cite + differentiate TIMBER (BDV-arbitrage critique, case2 loss) and LATTE (surrogate vs exact costing); (3) monitor NTU (Yao-Wen Chang group) for the full-paper version of LBR060 — highest collision risk for DAC 2027; (4) re-sweep after July 29 when DAC 2026 proceedings + LBR PDFs land on ACM DL.

## Sources

- https://iccl.unist.ac.kr/2026/03/paper-accepted/ (B-Flex acceptance)
- https://63dac.conference-program.com/ program searches: `?post_type=page&s=flip-flop`, `s=MBFF`, `s=placement`, `s=banking`, `s=legalization`, presentation LBR060/sess308, RESEARCH2394 (B-Flex)
- https://dac.com/2026 (dates/venue)
- https://tsung-wei-huang.github.io/papers/2025-ASPDAC-TIMBER.pdf (full text read, 7 pp.)
- https://ieeexplore.ieee.org/document/11420570/ (TIMBER), https://ieeexplore.ieee.org/document/11240779/ (2025 Problem B invited paper)
- http://iccad-contest.org/Winners.html (2026 cycle, no winners yet)
- https://www.cpr.cuhk.edu.hk/en/press/cuhk-joint-team-wins-global-championship-at-2025-iccad-cad-contest/ (2025 Problem C winners)
