# ICCAD 2025 CAD Contest Problem B — Intelligence Report

Date: 2026-07-11
Scope: final results, benchmark/evaluator availability, format diff vs 2024, competitor papers.

## TL;DR

- 2025 Problem B ("Power and Timing Optimization Using Multibit Flip-Flop", Synopsys) is officially described in its spec as **an extension of the 2024 ICCAD Problem B**. The optimization problem is the same (banking/debanking, minimize α·TNS + β·Power + γ·Area), but the I/O moved from the 2024 custom flat `.txt` to **industry formats (Verilog + DEF + SDC + .tf + Synopsys .nlib, separate weight file)** and scoring moved from a standalone evaluator binary to a **Synopsys ICC2-based TCL flow with MCMM (3 corners)**. So "format-only changes" is essentially right for the problem model, but the *evaluation infrastructure* is now tool-based, which matters for reproducibility.
- Winners were announced 2025-11-19. **1st place cadb1029 (advisor Prof. Yu-Cheng Lin); 2nd place cadb1021 = NTU Yao-Wen Chang's group, including Cheng-Yen Li — the 2024 Problem B champion (our P7/LBR SOTA author).**
- The **2025 contest site is still fully live** at `https://www.iccad-contest.org/2025/` (the root now serves 2026, but the year-scoped path works). Spec, Q&A, cost scripts, compatible-cell list, and slides are downloadable from Google Drive links there (verified 2026-07-11). Testcases are **not** on the public page but are mirrored in team GitHub repos.
- No winner-team paper/preprint for the 2025 edition found yet (as of 2026-07-11). The invited problem paper is on IEEE Xplore (doc 11240779).

## 1. Final results (official Winners page, iccad-contest.org/2025/Winners.html)

Announced 2025-11-19 (News page). 2025 contest overall: 247 teams (record; overview paper IEEE 11240649).

| Rank | Team ID | Members | Advisor(s) |
|---|---|---|---|
| 1st | cadb1029 | Yung-An Chen, Bing-Hsuan Shih, Chen-Wei Fan, Guan-Ting Lu | Prof. Yu-Cheng Lin |
| 2nd | cadb1021 | Chuan-Chi Su, **Cheng-Yen Li**, Shao-Hsiang Chen, Yu-Sheng Yang | Prof. **Yao-Wen Chang** (NTU) |
| 3rd | cadb1011 | Yi-Chi Tsai, Yu-Han Hu | Prof. Ching-Lueh Chang, Prof. Yu-Cheng Lin |
| HM | cadb1006 | Yi-Chun Hsu, Chang-Xun Li, Yu-Chi Cheng | Prof. Wai-Kei Mak (NTHU) |
| HM | cadb1075 | Ching-Wei Yao, Yu-De Lai, Yu-Chen Li | Prof. Ting-Chi Wang (NTHU) |
| HM | cadb1026 | Chi-Hua Yang, Tzu-Han Lin, Yu-Hung Wu, Cheng-Xun Song | Prof. Yung-Chih Chen |
| HM | cadb1045 | Po-Chien Chiu, Tsung-Ju Tsai, Dong-Yuan Ho, Shan Fu Liu | Prof. Hung-Ming Chen, Prof. Chien-Nan Liu (NYCU) |

Notes:
- **Cheng-Yen Li in the 2nd-place team** is the same person as the ICCAD 2024 Problem B winner / NTU 113-2 thesis (our tracked SOTA, avg comp 0.977). He did *not* repeat 1st under the new format.
- Prof. Yu-Cheng Lin advised both 1st and 3rd place; co-advises with Ching-Lueh Chang (Yuan Ze Univ.) — affiliation not confirmed in public sources I could reach; likely YZU-adjacent. Worth resolving before DAC 2027 related-work section.
- Problem A 1st: NTU (Ric Huang / R. Jiang). Problem C joint 1st: cadc1001 (CUHK/NTHU/Giga DA, Wuqian Tang et al.) and cadc1051 (CUHK, E. Young) after an official runtime-bias re-ranking (Dec 2025).

## 2. Benchmark + evaluator availability

The official 2025 archive is still served — the root `iccad-contest.org` redirects to 2026 content, but year paths are intact:

- Problems page: `https://www.iccad-contest.org/2025/Problems.html`
- Winners: `https://www.iccad-contest.org/2025/Winners.html`

Problem B downloads (Google Drive, all links from the official Problems page; spec verified downloadable via `drive.google.com/uc?export=download&id=<ID>` on 2026-07-11):

| Item | Drive file ID |
|---|---|
| Problem spec PDF (847 KB, PDF 1.3) | `1nEBboO_XxyG7uiWIVG2OYXnX-emkTMN9` |
| Q&A | `1DNS9e3rFfc0DCebKp1-vrDI0O5kBUAzz` |
| `public_cost.tcl` (ICC2 cost/eval script) | `1QS5PhTKPC3dcJHN-GWM7qFsw_kPLp3xP` |
| `parse_report_qor_summary.py` | `1smNg6mnRo8rijkau7ei8XX2sRflOdW4N` |
| `parse_report_power.py` | `1N-YP1xaLf8N2T8VuoQqTwmRZizUuibjL` |
| Compatible cell list (Drive folder) | folder `1Ea5iGGg-63lwmgYJEWyrc4Sf-UaErHTB` |
| Alpha test result | `1RlpYhi9sWjLAIWzkMeiy-pFjvH8_31Tf` |
| Beta test result | `1F8Qm0gxAttZH__byUsjEkD6hqCWfzvW3` |
| Initial-weight-score revision | `1_QHTqIv7xygfjAq6TRBkGzbi0ykn8yF6` |
| Problem presentation slides (2025-11-05) | `1LqhuQ-nlAUpiRO0odUHuUpZLak4nQdUa` |

**Testcases are NOT linked on the public Problems page** (they went to registered teams). Live GitHub mirrors found:

- `a9706888/ICCAD2025_Power-and-Timing-Optimization-Using-Multibit-Flip-Flop` — official `spec.pdf` (same 847,274 bytes) + `testcase/testcase1/` with `testcase1.def` (4.2 MB), `testcase1.sdc` (472 KB), `testcase1.tf` (103 KB), `testcase1.v` (4.5 MB), `testcase1_weight` (62 B). README documents the full flow (8-stage pipeline; debanking → grouping → banking → legalization; 60-min limit/case).
- `steven109511094/2025-ICCAD-Problem-B` — "derivative of 2024-ICCAD-Problem-B" (i.e., someone else also ported a 2024 codebase); includes `MyLIB.lib` (1.2 MB Liberty).
- `trix0831/MBFF_Miracle` — has a `2025/` directory with def/lef/lib/sdc/verilog/weight parsers (confirms the input format list) plus the 2024 `.txt` testcases.

**Caveat:** the evaluation library `SNPSDesign.nlib` (Synopsys binary design library opened by `public_cost.tcl`) is not in any public mirror I found, and scoring requires an **IC Compiler II license** (`report_qor`, `report_power`, MCMM scenarios). A fully offline replica of the 2025 scoring is therefore not possible from public artifacts alone; the closest substitutes are the parse scripts + weight files + the alpha/beta score sheets.

## 3. Input/output format vs 2024

The spec's own words (revision history + intro, extracted from the PDF): *"This contest is an extension of the 2024 ICCAD CAD Contest Problem B."* Revision log shows the format churn during the contest: added SDC as input (2025-06), added all-registers file input, revised data input format / aligned legality info with testcase (2025-08-01), revised output format + added TRANSFORM rev + changed CPU to 16 cores (2025-08-14), revised submit format (2025-05).

| Aspect | 2024 | 2025 |
|---|---|---|
| Input | single custom `.txt` (die, bins, cells, insts, nets, timing, weights) | **Verilog netlist (.v) + DEF + SDC + technology file (.tf) + Synopsys .nlib/Liberty + separate `*_weight` file (α, β, γ)** |
| Output | custom `.out` (inst list + pin mapping) | **new `.def` + functionally-equivalent `.v` + `.list` pin-mapping/operation log** |
| Cost | α·TNS + β·Power + γ·Area + **λ·DensityPenalty** | α·|TNS| + Σ_FF(β·TotalPower_FF + γ·Area_FF) — **no density λ term in `public_cost.tcl`** |
| Timing | contest evaluator: per-sink DisplacementDelay·HPWL, single ideal clock | **ICC2 `update_timing -full` with real SDC, MCMM: 3 corners (cold.BC/cold.WC/norm.BC; 0.85 V; −40 °C/25 °C), setup analysis on cold.WC/norm.BC** |
| Power | Liberty-style per-FF power from testcase file | ICC2 `report_power -cell_power` on all sequential cells (leakage+dynamic per scenario) |
| Legality/sanity | `sanity` + `placement_checker` binaries | in-flow checks: overlap/on-site, comb cells untouched, **functional equivalence** vs input netlist (UNMATCH = 0 score) |
| Scan chains | absent | **present** — scan FFs (SI/SE/SO pins) must be handled; Liberty `single_bit_degenerate` / banking-target concepts |
| Compute budget | 1 thread implied by our baseline era | `set_host_options -max_cores 16`; 60-min per testcase; runtime factor ±10% |

Implication for us: the algorithmic core (banking/debanking, placement, TNS/power/area trade-off) transfers directly, which supports the user's "format-only changes" recollection. But two *semantic* additions exist beyond format: (a) scan-chain preservation constraints, (b) tool-timed multi-corner TNS instead of the closed-form HPWL delay — the latter makes the 2025 edition effectively an **oracle-based** version of the exact model-accuracy problem we root-caused on tc2 (internal-vs-evaluator TNS gap). The 2025 setup is a natural motivation citation for our DAC 2027 incremental-timing-oracle story.

## 4. Competitor papers / preprints

- **Invited problem paper**: "2025 ICCAD CAD Contest Problem B: Power and Timing Optimization Using Multibit Flip-Flop", IEEE Xplore document 11240779 (ICCAD 2025 proceedings). Organizers (per CADathlon 2025 Problem-3 chairs, same Synopsys Taiwan "OPTO Physical Multibit & Hold" team): Wei-Che Tseng, Ting-Wei Lee, Jhih-Wei Hsu, Sheng-Wei Yang, Chin-Fang Cindy Shen.
- **Overview paper**: "Overview of 2025 CAD Contest at ICCAD", IEEE Xplore 11240649 (247 teams, record participation).
- **No 2025-winner-team paper or preprint found** on arXiv / DAC 2026 / ISPD 2026 / DATE 2026 as of 2026-07-11. Watch: Yao-Wen Chang group (Cheng-Yen Li — historically publishes the winning method, cf. his 2024-based LBR/thesis line), and Yu-Cheng Lin's group (1st + 3rd).
- Adjacent competitor to track: **TIMBER: A Fast Algorithm for Timing and Power Optimization using Multi-bit Flip-flops** (Tsung-Wei Huang's group, ASP-DAC paper, PDF at tsung-wei-huang.github.io/papers/2025-ASPDAC-TIMBER.pdf) — fast MBFF algorithm evaluated on the ICCAD contest benchmark family; directly in our DAC 2027 lane.

## 5. Sources

- https://www.iccad-contest.org/2025/Winners.html (winners, fetched raw HTML 2026-07-11)
- https://www.iccad-contest.org/2025/Problems.html and /News.html /FAQ.html (downloads, timeline)
- Official spec PDF + `public_cost.tcl` (downloaded from the Drive IDs above)
- https://github.com/a9706888/ICCAD2025_Power-and-Timing-Optimization-Using-Multibit-Flip-Flop
- https://github.com/steven109511094/2025-ICCAD-Problem-B , https://github.com/trix0831/MBFF_Miracle
- https://ieeexplore.ieee.org/document/11240779/ , https://ieeexplore.ieee.org/document/11240649/
- https://www.cpr.cuhk.edu.hk/en/press/cuhk-joint-team-wins-global-championship-at-2025-iccad-cad-contest/ (Problem C context)
- https://2025.iccad.com/cadathlon-iccad-2025 (Synopsys organizer names)
- https://tsung-wei-huang.github.io/papers/2025-ASPDAC-TIMBER.pdf (TIMBER)

Local artifacts saved in scratchpad (`/tmp/claude-5115/.../scratchpad/`): `specB.bin` (spec PDF), `specB.txt` (partial text extraction; subset-font cipher, key phrases decoded), `public_cost.tcl`, `winners25.html`, `problems25.html`, `news25.html`.
