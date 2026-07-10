---
name: Adaptive MATCH_K banking default
description: shipped V1 — MATCH_K defaults to 8 when β≤500 else 15; low-β cases (tc2/hc03 β=400) want tight neighborhood; high-β cases need wider
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Banking.cpp matching K-neighbors default is conditional on `FF::beta`:
- `FF::beta <= 500` → K=8
- else → K=15 (legacy default, preserves prior byte-exact)

Per-case β for 7 contest testcases (from testcase headers):
- tc1 β=2000, tc2 β=400, tc3 β=10000
- hc01 β=200000, hc02 β=40000, hc03 β=400, hc04 β=10000

Only tc2 and hc03 trigger K=8. Scores after ship (BANKING_MODE=matching PRODUCTION=1):
- tc2: 773,416.790 → 765,703.640 (**-0.997%**)
- hc03: 55,860,579 → 55,871,388 (+0.019%, noise)
- other 5 cases: byte-exact unchanged

**Why:** K=15 over-samples for β=400 cases (low-power-weighted cost fn). Max-weight matching commits distant pairs whose marginal power saving is negligible (β·Δpower dominated by α·ΔTNS penalty of displacement). K=8 keeps matching focused on geometric-locality wins.

**Why not fixed K=8 everywhere:** regressed hc02 +3.21%, hc01 +0.85% — high-β cases need K=15 because β>>α makes far-displacement matches profitable.

**Override:** `MATCH_K=15` reproduces prior byte-exact on tc2/hc03. `MATCH_K=<n>` still arbitrary-override for experiments.

**Untested mid-band:** β∈(500, 5000] — tc1 (β=2000) might tolerate K=10–12. Not swept; default K=15 applies.

**Location:** [Banking.cpp:900](../../../ICCAD_Project/2024-ICCAD-Problem-B/src/Banking.cpp#L900)
