---
name: Always set BANKING_MODE=matching when benchmarking
description: Shipped config uses LEMON matching banking (BANKING_MODE=matching); omitting this env var falls back to greedy doClustering() which gives ~3% worse scores and makes DP results non-comparable to recorded baselines
type: feedback
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Benchmarks MUST include `BANKING_MODE=matching` (and `PRODUCTION=1` for speed). Without it, Banking falls back to `doClustering()` instead of `doMatchingClustering()`, producing a completely different intermediate result. This caused a full session of DP experiments to compare against the wrong baseline (~822k vs correct 800k on t2_0812).

**Why:** The env var gate is in `Banking::run()` — default path is greedy clustering, matching is opt-in. The previous session recorded all baselines with `BANKING_MODE=matching PRODUCTION=1` but this wasn't documented, causing silent regression in the next session.

**How to apply:** Every benchmark command should be:
```
BANKING_MODE=matching PRODUCTION=1 timeout 120 ./cadb_0015_final testcase/X.txt output_X.txt
```
Then verify with `./evaluator/preliminary-evaluator testcase/X.txt output_X.txt`.
