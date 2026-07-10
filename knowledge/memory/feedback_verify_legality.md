---
name: Always verify output legality with manual evaluator
description: getEvaluatorCost() internal mechanism is buggy (temp.out race); always run evaluator binary manually on final output to verify legality and score
type: feedback
originSessionId: 38f20923-5a09-4802-8317-2d02f0cc3551
---
Always verify output legality by running the evaluator binary manually on the final output file, NOT by relying on `getEvaluatorCost()` scores from non-PRODUCTION runs.

**Why:** `getEvaluatorCost()` dumps to `temp.out` and runs evaluator on it, but this mechanism returns DBL_MAX (appears "ILLEGAL") even when the final `dump(argv[2])` output is perfectly legal. This caused us to wrongly reject cost-aware GlobalSwap and ChangeCell as "illegal" when they actually produced valid improvements (t2_0812 -2.02%, hc02 -0.74%).

**How to apply:** After any benchmark run, always do:
```
./evaluator/preliminary-evaluator testcase/<case>.txt output_<case>.txt 2>&1 | tail -1
```
Never trust the `[EVALUATOR] Score:` line from non-PRODUCTION runs as ground truth for legality.
