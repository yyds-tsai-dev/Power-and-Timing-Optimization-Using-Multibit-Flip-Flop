---
name: Thesis baseline comparability constraint
description: Published MBFF SOTA papers don't share benchmark/cost-function with ICCAD 2024 Problem B; comparison table must be scoped accordingly
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
SOTA MBFF papers (P1 Revisit MBFF ASP-DAC'25, P9 GNN-MBFF DAC'23/TODAES'23, P13 Capacitated K-means GLSVLSI'24) all use **commercial EDA flows** (Synopsys/Cadence + 28nm or ASAP7 7nm PDK) with **WNS/TNS/Power post-route metrics** — NOT ICCAD 2024 Problem B's `α·TNS + β·Power + γ·Area + λ·Density` cost function on contest benchmarks.

**Why:** Contest benchmarks are synthetic + displacement-delay-based; published papers evaluate on industrial flows.

**How to apply:**
- Thesis comparison table: only B1 (contest starter), B2 (our shipped baseline), B3 (coherent17 base) are **apples-to-apples** on ICCAD Problem B benchmarks
- P1/P9/P13 can only be cited as **relative improvement claims** ("P13 reports ~1.12% power overhead vs FTray on ASAP7") — not as table rows
- To get B4–B6 numbers, would need to reimplement their methods inside our codebase — 2-4 month effort each
- Don't over-promise comparison scope in thesis proposal

**Also:** P13 uses **LEMON** for min-cost flow → confirms LEMON is right choice for our Stage B max-weight matching (not Blossom V).
