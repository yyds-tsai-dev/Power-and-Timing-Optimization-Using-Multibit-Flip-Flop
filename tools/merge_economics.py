#!/usr/bin/env python3
"""Static per-case FF merge economics: best cell per bit-width, raw score gain
per k->2k merge, Qpd delta, and the TNS breakeven (gain/alpha).

Usage: python3 tools/merge_economics.py [testcase_dir]
Written for the Phase 2f graveyard-revival worksheet (2026-07-11, server B).
"""
import sys

CASES = {
    'tc1': 'testcase1_0812.txt', 'tc2': 'testcase2_0812.txt', 'tc3': 'testcase3.txt',
    'hc01': 'hiddencase01.txt', 'hc02': 'hiddencase02.txt',
    'hc03': 'hiddencase03.txt', 'hc04': 'hiddencase04.txt',
}


def analyze(path, tag):
    ffs, weights, inst_bits = {}, {}, {}
    with open(path) as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] in ('Alpha', 'Beta', 'Gamma', 'Lambda'):
                weights[t[0]] = float(t[1])
            elif t[0] == 'FlipFlop':
                ffs[t[2]] = dict(bits=int(t[1]), w=int(t[3]), h=int(t[4]))
            elif t[0] == 'QpinDelay' and t[1] in ffs:
                ffs[t[1]]['qpd'] = float(t[2])
            elif t[0] == 'GatePower' and t[1] in ffs:
                ffs[t[1]]['power'] = float(t[2])
            elif t[0] == 'Inst' and t[2] in ffs:
                b = ffs[t[2]]['bits']
                inst_bits[b] = inst_bits.get(b, 0) + 1
    a, b_, g = weights['Alpha'], weights['Beta'], weights['Gamma']
    best = {}
    for n, c in ffs.items():
        if 'power' not in c or 'qpd' not in c:
            continue
        cost = b_ * c['power'] + g * c['w'] * c['h']
        k = c['bits']
        if k not in best or cost < best[k][1]:
            best[k] = (n, cost, c)
    print(f"{tag}: alpha={a} beta={b_} gamma={g} | input FF insts by bits: "
          f"{dict(sorted(inst_bits.items()))}")
    for k in sorted(best):
        n, cost, c = best[k]
        print(f"   best {k}-bit: {n:8s} beta*P+gamma*A={cost:10.3f} "
              f"(per-bit {cost/k:8.3f}) qpd={c.get('qpd')}")
    for k in sorted(best):
        if 2 * k in best:
            gain = 2 * best[k][1] - best[2 * k][1]
            dqpd = best[2 * k][2]['qpd'] - best[k][2]['qpd']
            print(f"   merge 2x{k}b->{2*k}b: raw gain {gain:9.3f}/merge | "
                  f"dQpd {dqpd:+.4f} | TNS breakeven {gain/a:8.3f}")
    print()


if __name__ == '__main__':
    tcdir = sys.argv[1] if len(sys.argv) > 1 else 'testcase'
    for tag, fn in CASES.items():
        analyze(f"{tcdir}/{fn}", tag)
