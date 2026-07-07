#!/usr/bin/env python3
"""Generates figs/anytime.pdf: OFFICIAL evaluator score vs refinement wall-clock,
from evaluator runs on stage-boundary dumps (EVAL_CHECKPOINT mechanism).
Inputs: per-case ckpt run logs ([CKPT] lines) + a file mapping dump-basename ->
evaluator Final score (one 'CKPTEVAL <basename>: Final score:<v>' line each)."""
import re, sys
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt

EVALS = sys.argv[1]            # file with CKPTEVAL lines
LOGS  = {'testcase2_0812': sys.argv[2], 'hiddencase02': sys.argv[3]}
OUT   = sys.argv[4] if len(sys.argv) > 4 else 'anytime.pdf'

evals = {m.group(1): float(m.group(2)) for m in
         re.finditer(r'CKPTEVAL (\S+): Final score:([\d.]+)', open(EVALS).read())}
series = {}
for case, log in LOGS.items():
    pts = []
    for m in re.finditer(r'\[CKPT\] \S+ (?:alt=\S+ )?t_ms=(\d+) oracleCost=[\d.-]+ file=(\S+)', open(log).read()):
        base = m.group(2).split('/')[-1]
        if base in evals:
            pts.append((int(m.group(1)) / 1000.0, evals[base]))
    series[case] = sorted(pts)

plt.rcParams.update({'font.size': 6.5})
fig, ax = plt.subplots(figsize=(3.3, 1.6), dpi=300)
p = series['testcase2_0812']; t0 = p[0][0]
ax.plot([x - t0 for x, _ in p], [v / 1e3 for _, v in p], '-o', color='#20456e', lw=1.2, ms=2.8)
p2 = series['hiddencase02']; t02 = p2[0][0]
ax2 = ax.twinx()
ax2.plot([x - t02 for x, _ in p2], [v / 1e6 for _, v in p2], '--s', color='#8a8a8a', lw=1.1, ms=2.8)
ax2.set_ylabel('Hidden 2 (M)', fontsize=6.5, color='#666666'); ax2.tick_params(labelsize=6, colors='#666666')
ax.set_xlabel('refinement wall-clock (s)', fontsize=6.5)
ax.set_ylabel('Case 2 (K)', fontsize=6.5, color='#20456e')
ax.tick_params(labelsize=6); ax.spines['top'].set_visible(False)
fig.tight_layout(pad=0.2)
fig.savefig(OUT)
