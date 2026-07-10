---
name: Method D not a good fit for current architecture
description: Method D (slack-sigmoid + conflict-partition + slack-release) regresses tc2 by +1.55% even with Step 5; root cause is Steps 3+4 mechanism mismatch with this codebase's banking objective and FF distribution
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Method D = Phase A approach: Step 3 (SLACK_SIGMOID) + Step 4 (BATCH_MATCHING via ConflictPartition) + Step 5 (SLACK_RELEASE inter-batch credit). All three landed code-wise but the mechanism does not fit our codebase.

**tc2 score map (BANKING_MODE=matching PRODUCTION=1, all our shipped DP/banking on)**:
- plain: 771,385 (the target)
- sigmoid only (Step 3 SCALE=10): 781,318 (+1.29%)
- batch default (Step 4): 789,813 (+2.39%)
- sig + batch: 789,813 (sigmoid masked by partition)
- batch + Step 5 release option(a): 783,353 (+1.55%, recovered 0.84% but still net regression)

**Why it doesn't work here**:
1. Step 3 sigmoid trades TNS protection for power/area cost. Our cost fn is α·TNS + β·power + γ·area; tc2 is power/area dominated → sigmoid hurts.
2. Step 4 partition splits 21164 FFs into 1 giant (15886) + 7 small batches. Small batches lose access to giant batch's spatial pool → suboptimal partners.
3. Sigmoid bit-identically masked when batch on: partition pulls timing-similar FFs into same batch → uniform σ factor within batch → MWM invariant to uniform scaling.
4. Step 5 cross-batch credit structurally weak when one batch dominates (75% of FFs in single batch → most upstream/downstream stays intra-batch).

**Decision (2026-04-19)**: Method D code is **kept env-gated for ablation** in thesis (story: "we tried Phase A direction and characterized why it doesn't fit our problem profile"). Production path is Phase B (NTU outer loop, S_space slack-aware legalization).

**Why**: Validated experimentally on tc2 + parameter sweep over scale/hops/threshold/K — no config recovers plain baseline. Mechanism diagnosis above is reproducible from prior session logs.

**How to apply**:
- Do NOT enable SLACK_SIGMOID / BATCH_MATCHING / SLACK_RELEASE in production sweeps
- DO keep them as env-gated paths so thesis ablation table can show Phase A attempts and their failure modes
- When making banking-cost changes, sanity-check on tc2 (most fragile to Step 3-style edge-weight tweaks)
- Future banker work should target Phase B (post-banking) not Phase A (banking-time edge weights)

Commit checkpoint of Method D state preserved as the "snug-juggling-planet" plan implementation; superseded by phase-b-ntu-outer-loop.md.
