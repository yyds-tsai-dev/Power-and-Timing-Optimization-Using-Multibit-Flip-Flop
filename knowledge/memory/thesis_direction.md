---
name: thesis direction — Beat NTU 0.977 via 6-stage architectural campaign
description: User committed (2026-04-20) to long-term plan_beat_ntu_longterm.md — 6 stages targeting geo-mean ≤ 0.95 vs NTU thesis; Stage 3 (LP-round MBFF banking, Chan-Lau 2+ε) is headline thesis chapter
type: project
originSessionId: 337e23e6-9fa1-45e1-93b2-f42a788fbcff
---
User's master's thesis on ICCAD 2024 Problem B. Direction history:
- Earlier: Method D (slack redistribution + max-weight matching)
- Mid: PCPD/FITP (Phase 3b Lagrangian) — shipped, essentially ties NTU contest 0.988 geo-mean
- Current (2026-04-20): **long-term 6-stage campaign** — see `Project Knowledge/plans/active/plan_beat_ntu_longterm.md`

**Why:** Phase 3b ~tied NTU contest 0.988 on geo-mean but real gaps remain (tc1 +2.5%, tc2 +5.0% vs NTU). PCPD architectural ceiling is ~NTU contest level; beating NTU thesis 0.977 requires work outside banking layer. User explicitly said "不計代價" (regardless of cost) and approved full architectural rewrites.

**How to apply:**
- Target: **geo-mean ≤ 0.95 vs NTU thesis**, decisively beat 0.977
- Entry condition: Phase 7 PCPD sweep concluded + PCPD retired
- Stage order (strict 0→1→2→3; 4 can overlap with 5 gating):
  - **Stage 0** (1wk): cherry-pick per-pin CostCompare + DP slot_assign=2 from main — baseline harden, no novelty
  - **Stage 1** (2-3wk): tree-based Steiner-slack CostCompare + downstream hop=2 — attacks tc1 gap
  - **Stage 2** (2-3wk): slack-driven pre-GP (CG objective + λ·TNS) — attacks tc2 gap
  - **Stage 3** (3-4wk): **LP-round MBFF banking (Chan-Lau 2012, 2+ε) — thesis main chapter**
  - **Stage 4** (2wk): joint banking+LG SA polish
  - **Stage 5** (4-6wk, risk-gated): full STA-grade incremental slack graph — only if Stage 4 hasn't reached 0.95
- Each stage: ablation + report in `reports/v2/` before next begins (V2 SOP)
- Stages 1/2/4 merge-back to V1 if 7-case strict win; Stages 3/5 stay V2-only as thesis
- Thesis headline is Stage 3's provable approximation bound
