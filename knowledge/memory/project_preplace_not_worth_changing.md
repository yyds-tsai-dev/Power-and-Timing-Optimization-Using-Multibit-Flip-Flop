---
name: Pre-placement stage not worth changing
description: Current Polak-Ribière CG + log-sum-exp HPWL pre-placement is adequate; ePlace Nesterov would need full density model rewrite for marginal gain; Banking is higher ROI
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Analyzed 2026-04-17. Pre-placement uses Conjugate Gradient (Polak-Ribière) with log-sum-exp HPWL approximation, followed by weighted K-means (MeanShift). Replacing with ePlace-style Nesterov placement would require implementing electrostatic density modeling — large effort for uncertain gain since the current pipeline's bottleneck is Banking quality, not pre-placement quality.

**Why:** Cost breakdown shows Banking accounts for the largest quality delta. Pre-placement provides initial positions that Banking then clusters — improving clustering decisions (CostCompare accuracy, matching quality) has higher ROI.

**How to apply:** Prioritize Banking-stage improvements (Method D, CostCompare accuracy) over pre-placement rewrites.
