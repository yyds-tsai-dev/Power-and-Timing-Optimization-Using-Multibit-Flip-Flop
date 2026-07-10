---
name: ComputeOptimalPosition doesn't improve 2-bit matching
description: Slack-weighted median of driver/load positions tested as replacement for geometric median in CostCompare and graph build; FF pre-placement already near-optimal so pulling toward drivers/loads hurts more than helps
type: project
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Tested 2026-04-17 (commit f681cda). ComputeOptimalPosition computes slack-weighted median of driver/load positions (with target cell pin offsets) as MBFF placement target.

**Results on t2_0812:**
| Variant | Score | vs pin-offset-only |
|---|---|---|
| Pin-offset fix only (median position) | 772,711 | baseline |
| OptPos in graph build + commit | 1,458,233 | **+89%** |
| OptPos in graph build only, median commit | 788,829 | +2.1% |
| Dual-position commit (median + optPos, pick better) | 994,974 | +28.8% |

**Why it fails:** FF positions after pre-placement already balance HPWL to drivers/loads + density constraints. ComputeOptimalPosition pulls MBFF toward timing-critical drivers, creating large displacement from FFs' current positions. This displacement penalty exceeds the HPWL reduction benefit.

**How to apply:** Don't use ComputeOptimalPosition for 2-bit matching placement or graph build. It may be useful for a future global position optimizer that re-places ALL MBFFs jointly (not implemented yet). The function is defined in Banking.cpp but not called in the default flow.
