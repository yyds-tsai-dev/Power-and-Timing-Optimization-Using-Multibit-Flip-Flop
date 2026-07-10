---
name: DP_SLOT_ASSIGN adaptive default
description: DetailAssignmentMBFF defaults to adaptive (mode 2 when beta<=500, mode 3 else); per-iter pre-pass wins slack-dominated cases
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
V1 DetailPlacement ships with `DP_SLOT_ASSIGN=-1` (adaptive) as default: mode 2 (post-pass) when `mgr.beta <= 500`, mode 3 (per-iter pre-pass) otherwise. Commit 80c79f2 (2026-04-21).

**Why:** mode 3 calls DetailAssignmentMBFF at the start of each DP iteration (before GlobalSwap+ChangeCell), keeping MBFF slot labels tight while row state evolves. Slack-dominated cases win (hc02 -0.332%, hc01 -0.055%, tc1 -0.012%, tc3 -0.004%). Power/area-dominated profiles (beta<=500: tc2, hc03) regress or stay byte-exact, so gate routes them to mode 2.

**How to apply:** adaptive is default — no env needed. Override with `DP_SLOT_ASSIGN={0,2,3,4,5,6}` to pin a mode for ablation. `DP_SLOT_ASSIGN=0` restores pre-ship byte-exact. `DP_SLOT_INTRA_ONLY=1` remains default (cross-MBFF branch dormant; has hc02 -3.32% on table but cascades per prior experiments).

**7-case verified (80c79f2):** tc1 -0.012%, tc2 byte-exact, tc3 -0.004%, hc01 -0.055%, hc02 -0.332%, hc03 byte-exact, hc04 +0.001% (13 ppm, below threshold). 4 strict wins, 2 byte-exact, 1 noise-level regress. Checker passes on all outputs.
