#!/usr/bin/env python3
"""Offline reference STA for ICCAD-2024 Problem B, per exactly-decoded
preliminary-evaluator semantics (2026-07-11 blackbox report).

arrival(D) = max over combinational fanin paths from launch points
             (FF Q: Qpd of CURRENT mapped cell; Input port: 0)
             of launchQpd + Dd * sum per-arc 2-pt HPWL(driver pin, sink pin).
slack' = parseSlack - (curMaxArr - parseMaxArr), uncapped.
TNS = sum over FF D pins of max(0, -slack').
Score = A*TNS + B*sum(FF power) + G*sum(FF area) + L*(#bins util>BinMaxUtil).

Usage: refsta.py input.txt solution.out [--dump ref_slack.tsv]
"""
import sys
from collections import defaultdict
import numpy as np

_f32 = np.float32


def F(x):
    """round to float32 (evaluator does its timing arithmetic in float)"""
    return float(_f32(x))


import os
OUT2_MODE = os.environ.get('OUT2_MODE', 'dead')  # dead | zero | full
NEG = float('-inf')


def parse_input(path):
    d = {
        'ports': {},        # name -> (x, y)  (both Input and Output)
        'inputs': set(),
        'lib': {},          # cell -> dict(kind, bits, w, h, pins{name:(x,y)})
        'insts': {},        # name -> (cell, x, y)
        'nets': [],         # list of (netname, [pinref,...])
        'qpd': {},          # ffcell -> Qpd
        'slack': {},        # (inst, pin) -> parse slack
        'power': {},        # ffcell -> GatePower
        'rows': [],
    }
    with open(path) as f:
        lines = f.read().split('\n')
    i, n = 0, len(lines)
    cur_cell = None
    cur_net = None
    while i < n:
        t = lines[i].split()
        i += 1
        if not t:
            continue
        k = t[0]
        if k == 'Alpha':
            d['alpha'] = float(t[1])
        elif k == 'Beta':
            d['beta'] = float(t[1])
        elif k == 'Gamma':
            d['gamma'] = float(t[1])
        elif k == 'Lambda':
            d['lambda'] = float(t[1])
        elif k == 'DieSize':
            d['die'] = tuple(float(x) for x in t[1:5])
        elif k == 'Input':
            d['ports'][t[1]] = (float(t[2]), float(t[3]))
            d['inputs'].add(t[1])
        elif k == 'Output':
            d['ports'][t[1]] = (float(t[2]), float(t[3]))
        elif k == 'FlipFlop':
            cur_cell = {'kind': 'ff', 'bits': int(t[1]), 'w': float(t[3]),
                        'h': float(t[4]), 'pins': {}}
            d['lib'][t[2]] = cur_cell
        elif k == 'Gate':
            cur_cell = {'kind': 'gate', 'bits': 0, 'w': float(t[2]),
                        'h': float(t[3]), 'pins': {}}
            d['lib'][t[1]] = cur_cell
        elif k == 'Pin' and cur_net is None:
            cur_cell['pins'][t[1]] = (float(t[2]), float(t[3]))
        elif k == 'NumInstances':
            cur_cell = None
        elif k == 'Inst':
            d['insts'][t[1]] = (t[2], float(t[3]), float(t[4]))
        elif k == 'NumNets':
            cur_net = 'armed'
        elif k == 'Net':
            cur = (t[1], [])
            d['nets'].append(cur)
            cur_net = cur
        elif k == 'Pin':
            cur_net[1].append(t[1])
        elif k == 'BinWidth':
            d['binw'] = float(t[1])
        elif k == 'BinHeight':
            d['binh'] = float(t[1])
        elif k == 'BinMaxUtil':
            d['binmax'] = float(t[1])
        elif k == 'PlacementRows':
            d['rows'].append(tuple(float(x) for x in t[1:]))
        elif k == 'DisplacementDelay':
            d['dd'] = float(t[1])
        elif k == 'QpinDelay':
            d['qpd'][t[1]] = float(t[2])
        elif k == 'TimingSlack':
            d['slack'][(t[1], t[2])] = float(t[3])
        elif k == 'GatePower':
            d['power'][t[1]] = float(t[2])
    return d


def parse_out(path):
    insts = {}   # newname -> (cell, x, y)
    pmap = {}    # (originst, origpin) -> (newinst, newpin)
    with open(path) as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] == 'Inst':
                insts[t[1]] = (t[2], float(t[3]), float(t[4]))
            elif len(t) == 3 and t[1] == 'map':
                a = t[0].split('/')
                b = t[2].split('/')
                pmap[(a[0], a[1])] = (b[0], b[1])
    return insts, pmap


def hpwl(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])


def run(inp, out, dump=None):
    D = parse_input(inp)
    oinsts, pmap = parse_out(out)
    dd = D['dd']
    lib = D['lib']
    insts = D['insts']
    ports = D['ports']
    inputs = D['inputs']

    def inst_kind(iname):
        return lib[insts[iname][0]]['kind']

    # pin position, parse placement
    def ppos_parse(inst, pin):
        c, x, y = insts[inst]
        off = lib[c]['pins'][pin]
        return (x + off[0], y + off[1])

    # pin position, current placement (FF pins re-mapped; gates unchanged)
    def ppos_cur(inst, pin):
        m = pmap.get((inst, pin))
        if m is not None:
            ni, npn = m
            c, x, y = oinsts[ni]
            off = lib[c]['pins'][npn]
            return (x + off[0], y + off[1])
        return ppos_parse(inst, pin)

    # launcher Qpd
    def qpd_parse(inst):
        return D['qpd'][insts[inst][0]]

    def qpd_cur(inst, pin):
        m = pmap.get((inst, pin))
        if m is not None:
            return D['qpd'][oinsts[m[0]][0]]
        return qpd_parse(inst)

    # classify pin refs per net; build fanin lists
    # driver kinds: ('port', name) ('ffq', inst, pin) ('gout', inst, pin)
    gate_fanin = defaultdict(list)   # gate inst -> [(driver, sinkpin)]
    gate_fanout = defaultdict(list)  # gate inst -> [dependent gate inst]
    ffd_fanin = defaultdict(list)    # (inst, dpin) -> [driver]
    indeg = defaultdict(int)

    for netname, pins in D['nets']:
        drivers = []
        sinks = []   # (kind, inst, pin)
        for ref in pins:
            if '/' in ref:
                iname, pname = ref.split('/')
                kd = inst_kind(iname)
                if kd == 'ff':
                    if pname.startswith('Q'):
                        drivers.append(('ffq', iname, pname))
                    elif pname.startswith('D'):
                        sinks.append(('ffd', iname, pname))
                    # CLK ignored
                else:
                    if pname.startswith('OUT'):
                        drivers.append(('gout', iname, pname))
                    else:
                        sinks.append(('gin', iname, pname))
            else:
                if ref in inputs:
                    drivers.append(('port', ref, None))
                # Output port: pure sink, ignore
        if not drivers:
            continue
        for s in sinks:
            skind, si, sp = s
            for drv in drivers:
                if skind == 'gin':
                    gate_fanin[si].append((drv, sp))
                    if drv[0] == 'gout':
                        gate_fanout[drv[1]].append(si)
                        indeg[si] += 1
                else:
                    ffd_fanin[(si, sp)].append(drv)

    # arc delay both placements (float32 arithmetic like the evaluator)
    dd32 = F(dd)

    def arc(drv, sink_inst, sink_pin, sink_is_ff):
        dk, di, dp = drv
        if dk == 'port':
            dpp = ports[di]
            dpc = dpp
            base_p = 0.0
            base_c = 0.0
        elif dk == 'ffq':
            dpp = ppos_parse(di, dp)
            dpc = ppos_cur(di, dp)
            base_p = F(qpd_parse(di))
            base_c = F(qpd_cur(di, dp))
        else:  # gate out
            dpp = ppos_parse(di, dp)
            dpc = dpp
            if dp != 'OUT1' and OUT2_MODE != 'full':
                # evaluator quirk: only OUT1 carries the gate's arrival.
                if OUT2_MODE == 'dead':
                    base_p = NEG
                    base_c = NEG
                else:  # 'zero': wire-delay-only launch
                    base_p = 0.0
                    base_c = 0.0
            else:
                base_p = arr_p[di]
                base_c = arr_c[di]
        if sink_is_ff:
            spp = ppos_parse(sink_inst, sink_pin)
            spc = ppos_cur(sink_inst, sink_pin)
        else:
            spp = ppos_parse(sink_inst, sink_pin)
            spc = spp
        hp = F(F(abs(F(dpp[0]) - F(spp[0]))) + F(abs(F(dpp[1]) - F(spp[1]))))
        hc = F(F(abs(F(dpc[0]) - F(spc[0]))) + F(abs(F(dpc[1]) - F(spc[1]))))
        return (F(base_p + F(dd32 * hp)), F(base_c + F(dd32 * hc)))

    # topological pass over gates (Kahn)
    NEG = float('-inf')
    arr_p = {}
    arr_c = {}
    from collections import deque
    all_gates = set(gate_fanin) | set(gate_fanout)
    all_gates.update(i for i, (c, x, y) in insts.items()
                     if lib[c]['kind'] == 'gate')
    q = deque(g for g in all_gates if indeg.get(g, 0) == 0)
    seen = 0
    while q:
        g = q.popleft()
        seen += 1
        bp = bc = NEG
        for drv, sp in gate_fanin[g]:
            r = arc(drv, g, sp, False)
            if r is None:
                continue
            if r[0] > bp:
                bp = r[0]
            if r[1] > bc:
                bc = r[1]
        arr_p[g] = bp
        arr_c[g] = bc
        for h in gate_fanout[g]:
            indeg[h] -= 1
            if indeg[h] == 0:
                q.append(h)
    if seen != len(all_gates):
        print(f"WARNING: combinational cycle, {len(all_gates)-seen} gates unresolved",
              file=sys.stderr)
        for g in all_gates:
            if g not in arr_p:
                arr_p[g] = NEG
                arr_c[g] = NEG

    # FF D pins: all TimingSlack entries
    tns = 0.0
    rows_out = []
    for (inst, pin), psl in sorted(D['slack'].items()):
        best_p = best_c = NEG
        for drv in ffd_fanin.get((inst, pin), ()):
            r = arc(drv, inst, pin, True)
            if r is None:
                continue
            if r[0] > best_p:
                best_p = r[0]
            if r[1] > best_c:
                best_c = r[1]
        if best_p == NEG or best_c == NEG:
            # unreachable D pin: evaluator drops it from TNS entirely
            rows_out.append((inst, pin, float('inf')))
            continue
        sl = F(psl - F(best_c - best_p))
        if sl < 0:
            tns += -sl
        rows_out.append((inst, pin, sl))

    # power / area over solution FFs
    power = 0.0
    area = 0.0
    for name, (c, x, y) in oinsts.items():
        power += D['power'][c]
        area += lib[c]['w'] * lib[c]['h']

    # bin density: FF (solution) + gates (input) overlap; util% > BinMaxUtil
    x0, y0, x1, y1 = D['die']
    bw, bh = D['binw'], D['binh']
    import math
    nx = int(math.ceil((x1 - x0) / bw))
    ny = int(math.ceil((y1 - y0) / bh))
    binarea = defaultdict(float)

    def add_rect(rx, ry, rw, rh):
        ix0 = max(0, int((rx - x0) // bw))
        ix1 = min(nx - 1, int((rx + rw - x0 - 1e-9) // bw))
        iy0 = max(0, int((ry - y0) // bh))
        iy1 = min(ny - 1, int((ry + rh - y0 - 1e-9) // bh))
        for ix in range(ix0, ix1 + 1):
            bx0 = x0 + ix * bw
            ox = min(rx + rw, bx0 + bw) - max(rx, bx0)
            if ox <= 0:
                continue
            for iy in range(iy0, iy1 + 1):
                by0 = y0 + iy * bh
                oy = min(ry + rh, by0 + bh) - max(ry, by0)
                if oy > 0:
                    binarea[(ix, iy)] += ox * oy

    for name, (c, x, y) in oinsts.items():
        add_rect(x, y, lib[c]['w'], lib[c]['h'])
    for name, (c, x, y) in insts.items():
        if lib[c]['kind'] == 'gate':
            add_rect(x, y, lib[c]['w'], lib[c]['h'])
    nviol = 0
    for (ix, iy), a in binarea.items():
        if a / (bw * bh) * 100.0 > D['binmax']:
            nviol += 1

    A, B, G, L = D['alpha'], D['beta'], D['gamma'], D['lambda']
    score = A * tns + B * power + G * area + L * nviol
    print(f"TNS      = {tns:.9f}")
    print(f"Power    = {power:.9f}")
    print(f"Area     = {area:.9f}")
    print(f"ViolBins = {nviol}")
    print(f"terms: A*TNS={A*tns:.9f} B*P={B*power:.9f} "
          f"G*A={G*area:.9f} L*D={L*nviol:.9f}")
    print(f"Score    = {score:.9f}")

    if dump:
        with open(dump, 'w') as f:
            for inst, pin, sl in rows_out:
                f.write(f"{inst}/{pin}\t{sl:.9f}\n")
    return score


if __name__ == '__main__':
    dump = None
    args = [a for a in sys.argv[1:]]
    if '--dump' in args:
        i = args.index('--dump')
        dump = args[i + 1]
        del args[i:i + 2]
    run(args[0], args[1], dump)
