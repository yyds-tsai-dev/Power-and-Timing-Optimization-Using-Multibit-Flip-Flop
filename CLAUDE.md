# Workspace V1 — Score at All Costs

**Branch**: `main`
**Role**: the scoring workspace. Everything here exists to drive **per-case score down** on the ICCAD 2024 contest set.
**Mindset**: no effort is too much if it buys score. Compile time, code ugliness, maintenance cost — irrelevant. The only metric that matters is: **do we beat NTU on every contest case**.

---

## The target

Beat NTU (ICCAD 2024 winner) on **all 7 contest cases**:
`testcase1_0812`, `testcase2_0812`, `testcase3`, `hiddencase01`, `hiddencase02`, `hiddencase03`, `hiddencase04`.

NTU per-case ceiling lives in [Project Knowledge/reports/shared/ntu_baseline.md](../Project%20Knowledge/reports/shared/ntu_baseline.md). Consult it before every change — don't eyeball, don't re-derive, don't trust memory of the numbers.

Contest average-composite target: **beat 0.988** (P7 LBR) → stretch goal **0.977** (NTU thesis). Average is not the gate; **per-case win is the gate**.

---

## Testcases we skip

`testcase1_MBFF.txt`, `testcase2_MBFF.txt`, `sampleCaseMBFF`, and other `*_MBFF*` / `*_ALL0` / `*_NEG` / `*_L` / `*_upright` variants are **our own internal testcases**, not contest cases. **Do not benchmark them by default.** NTU has no published number for them, so "winning" is meaningless. Only touch them if explicitly asked.

Default sweep set = 7 contest cases above. Nothing else.

---

## Scope (what belongs here)

If the change moves score on any contest case, it belongs in V1. Concretely:

- Parameter sweeps, coefficient fits, threshold tuning
- DP / Legalizer bug fixes (memory `feedback_dp_optimization`: ChangeCell OMP race, GlobalSwap rtree bug, K-nearest search)
- B1 / B2 / B3 competitor trick absorption
- CostCompare internals
- Refactors that preserve architecture but unlock tuning
- Ugly, case-specific tricks — allowed if they don't regress other cases
- Anything that ships under an env gate with default-off byte-exact

**Effort ceiling: none.** If a 0.1% win on hc02 costs a week of work, do it. The thesis value is competing at the top of the board; engineering debt is not a reason to stop.

## Out of scope (belongs in V2)

- New algorithmic families (Berman 3-swap, LP rounding, Lagrangian, SA)
- Architecture rewrites (defer-and-batch restructuring, new commit loops)
- Path B local-search refinement
- GPU acceleration experiments
- Any change that **cannot** ship byte-exact when its gate is off

Rule of thumb: if the change has publishable novelty, it belongs in V2. V1 is execution.

---

## Env gate conventions

Existing `ALG1_*` gates are the right home for new tuning knobs. Only invent a new gate when the change genuinely needs one.

Gates currently in use (authoritative list in `Banking.cpp` / `ParamMgr.cpp`):
- `BANKING_MODE=matching PRODUCTION=1` — **mandatory** for scoring (memory `feedback_banking_mode_env`)
- `ALG1_2B`, `ALG1_HB`, `ALG1_C5`, `ALG1_C3/C4/C6`, `ALG1_LOOKAHEAD_K`
- `MATCH_HIGHER_BIT` — default OFF; enabling causes +254% hc02 (memory `project_match_higher_bit`)

---

## Sweep discipline

Before claiming a win:
1. Run all 7 contest cases (5 main + 4 hidden)
2. Compare per-case score vs NTU table **and** vs prior V1 HEAD
3. Verify legality by invoking evaluator binary — not `getEvaluatorCost()` (memory `feedback_verify_legality`)
4. If any contest case regresses > 0.5%, investigate before shipping
5. Record per-case numbers in the `improvements.md` entry, not just deltas

## One plan → one report

When a plan in `Project Knowledge/plans/active/` concludes, write a report in `Project Knowledge/reports/v1/` before archiving the plan. Naming rule and folder conventions live in [Project Knowledge/reports/README.md](../Project%20Knowledge/reports/README.md).

- Shipped → `exp` report with per-case numbers and why it works; also append to `improvements.md`
- Dead-ended → `postmortem` report capturing the lesson
- Investigation without fix → `diagnosis` report

No report ⇒ the plan stays in `active/`. Don't archive an unexamined outcome.

---

## Output conventions

- Binary output: `/tmp/v1_${case}_${tag}.out`
- Logs: `/tmp/v1_${case}_${tag}.log`
- Evaluator results: `/tmp/v1_${case}_${tag}.evaluator`

Keeps V1 artifacts from clobbering V2's.

---

## Safety rules

1. Every env gate must be **byte-exact when off**
2. Never skip the 4 hidden cases on a full sweep
3. Never `git push --force` to `main`
4. Never commit `.out` / `.log` / `.o` files
5. Legality: `checker/sanity` + `checker/placement_checker` must pass on every shipped commit
