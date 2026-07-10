---
name: Verify USE_ORTOOLS before any LP sweep
description: LPBanking::run is `#ifndef USE_ORTOOLS → return false` — plain `make release` produces a binary that silently falls through LP to legacy matching, byte-exact to V1 HEAD. Masked 4/7-case regression during Phase 1.5.b.
type: feedback
originSessionId: 337e23e6-9fa1-45e1-93b2-f42a788fbcff
---

Before running any ALG2_LP_BANK sweep, confirm the binary is OR-tools-linked:

```
ldd cadb_0015_final | grep -q libortools.so && echo OK || echo MISSING_ORTOOLS
```

If missing, rebuild with `make USE_ORTOOLS=1 release` (the Makefile already supports
this; plain `make release` drops the flag).

**Why:** `src/LPBanking.cpp:298` wraps the entire run in `#ifndef USE_ORTOOLS →
return false`. Without the flag, `ALG2_LP_BANK=1` is silently ignored — the dispatch
in `Banking.cpp:254-256` gets `handled=false` and falls through to
`doMatchingClustering()`. Output is byte-exact to V1 HEAD for every case.

**How to apply:** this turns a run-level failure into a silent measurement artifact.
Happened in Phase 1.5.b: "cap40k matches V1 HEAD on 4/7 cases" looked like a free
lunch; it was the binary not running LP at all on those cases. When properly linked,
the same sweep showed +4–18% regressions on those 4 cases.

Checklist before any LP experiment:
1. `ldd cadb_0015_final | grep libortools` — must print a path.
2. Run a known-LP-active case (hc01 or tc1) with `ALG2_LP_BANK=1` and expect
   `[ALG2_LP] ff1b=... OVER_CAP` in stderr. No ALG2_LP lines ⇒ binary is wrong.
3. Only then trust the 7-case numbers.

**Generalisable lesson:** any `#ifndef FEATURE_FLAG → return false` pattern in a
contest binary is a silent-failure hazard. When the build gate is orthogonal to the
runtime gate, accidental omission of the build flag looks indistinguishable from "the
feature didn't fire on that input". For every such pattern, put the `ldd` / feature-
detect check in front of the sweep script, not in the tester's head.
