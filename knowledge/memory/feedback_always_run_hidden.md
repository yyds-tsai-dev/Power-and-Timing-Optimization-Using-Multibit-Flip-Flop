---
name: Always run hidden cases in benchmarks
description: When benchmarking any optimization change, always include hiddencase01-04 alongside the main testcases — user flagged this as easy to forget
type: feedback
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Always include hiddencase01-04 when benchmarking optimization changes, not just the main 5 testcases (t1_0812, t2_0812, t3, t1_MBFF, t2_MBFF).

**Why:** Hidden cases have very different cost profiles (hc01 is 96% Power, hc02 is 34% TNS + 62% Power) and can show improvements or regressions invisible in the main cases. User flagged this as easy to forget.

**How to apply:** Any time running a comparison sweep (greedy vs matching, before/after optimization, etc.), run all 9 testcases. The hidden cases are at `testcase/hiddencase01.txt` through `testcase/hiddencase04.txt` in the repo.
