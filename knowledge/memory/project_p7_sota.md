---
name: P7 LBR and NTU ntu-113-2 thesis are the same SOTA work
description: Cheng-Yen Li's DAC 2025 LBR (P7) and the 67-page NTU Master's thesis (ntu-113-2-fixed.pdf) are the same NTU Yao-Wen Chang group work — ICCAD 2024 contest winner; thesis has the fuller methodology
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Same author (Cheng-Yen Li), same advisor (Yao-Wen Chang, NTU EDA Lab), same methodology, same benchmark:
- P7 = "Late Breaking Results: Multi-Objective Multi-Bit Flip-Flop Placement Considering Pre-Placed Cells," DAC 2025, 2 pages, DOI 10.1109/DAC63849.2025.11133043.
- Thesis = Project Knowledge/NTU/ntu-113-2-fixed.pdf, 67 pages, NTU Aug 2025, doi:10.6342/NTU202503564. Abstract: "our team ranked first" in ICCAD 2024 contest.

Thesis has MORE than LBR:
- 5-step Stage B (vs 3 in LBR): adds §3.2.3 k-Bit Legalization (Algorithm 1) and §3.2.4 k-Bit Candidate Selection as distinct steps.
- S_space legalization score: D_{U,2}(i) − D_{U,1}(i) + c3·σ(−slack(i)/c4) — NOT in LBR.
- Slack-distribution formula: sp(i) = sp·WL_i/(WL_1+WL_2) — NOT in LBR.
- k-d tree explicitly stated for nearest-neighbor search (LBR just says "nearest").
- Ablation tables 4.4/4.5 showing per-step contribution.
- Refined scores: Table 4.3 gives −2.3%/−3.3%/−8.3% vs 1st/2nd/3rd (thesis post-contest refinement beats LBR's own −1.2%/−3.2%/−8.8%). Avg comp 0.977.

**Why**: User asked "P7 vs NTU/ntu-113-2 哪個是 SOTA". Verified by PyPDF2 extraction of the thesis (pdftotext unavailable on this box). They are the same work — thesis is long version.

**How to apply**:
- Cite P7 for peer-review (DAC'25 is peer-reviewed; thesis is institutional).
- Use thesis for implementation detail (Algorithm 1, S_space, slack distribution, ablations).
- Ceiling = thesis avg 0.977 (hidden2 at 0.890 is biggest gap, matches our hc02-slack-dominated pattern).
- Do NOT compare to P19 (Jiang group ISPD24), P1 (SJTU), P21 (CUHK), P4 (SNU) — different benchmarks/objectives, their numbers don't transfer.

Full analysis: Project Knowledge/sota_survey_2026-04-18.md


**2026-07-05 correction (from the actual DAC'25 LBR PDF, now at `ICCAD_Project/Paper_ref/`)**: the PUBLISHED LBR composite is **0.988** (avg ratio vs 1st place, their own machine re-run); 0.977 is the unpublished thesis number and must NOT be cited in papers. LBR Table I has full per-case scores (T1 738.8M / T2 738,400 / T3 728.8M / H1 31.27M / H2 12.76M / H3 55.92M / H4 726.9M). DATE'26 (Chen/NYCU, also in Paper_ref/) has exact top-3 per-case costs; its baseline differs from LBR's re-run (tc2 1st: 744,231 vs 748,000 — machine-dependent contest binaries). V3 unified config beats BOTH published methods head-to-head on all 7 cases; composite 0.953 (top-3-best baseline) / ~0.950 (LBR's vs-1st convention).