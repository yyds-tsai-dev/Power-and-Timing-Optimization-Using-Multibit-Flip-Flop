---
name: Quality over engineering cost
description: For this thesis work, optimize for solution quality only; engineering cost / full rewrites are not a reason to pick a weaker design
type: feedback
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
For this ICCAD Problem B thesis work, optimize for solution-quality improvement only. Engineering cost is not a factor when choosing between designs.

**Why:** The user explicitly said "現在能優化的事，不該考慮工程量，只考慮結果，如果solution quality能明顯improve，整個codebase 翻掉也值得，反正可以revert回去". The thesis bar requires beating the SOTA (NTU 0.977) on a public contest benchmark — any design compromise made to save implementation effort directly eats into the headroom the thesis needs. Git revert is always available, so the downside of an over-ambitious refactor is bounded.

**How to apply:**
- When presenting options, do not rank by implementation effort. Rank by expected quality ceiling.
- Prefer v2 strong-guarantee designs (e.g., safety-gated MILP with post-commit recheck and rollback-the-rollback) over v1-simple variants, even when v2 is 2-3× more code.
- Do not suggest "ship v1 first, iterate to v2" unless the user explicitly asks to stage it. Go straight to the quality-optimal design.
- A proposal like "rewrite most of Banking.cpp" is on the table if it unlocks quality. Flag it; don't hide it.
- Exception: if a design is purely speculative and the quality upside is unclear, say so — don't dress up a guess as the quality-optimal choice just to justify scope.
