#!/usr/bin/env python3
"""Replay ActorBase::fn_801F172C from the trace: entry *pos / *box, and the
nested isBlockedOrOutOfView_0x50 probes (position + result) of each logged
call. Checks the probe positions against the replay, derives the hit counters
and which clause of each of the four move conditions fired, and checks the
replayed *pos / *out against the exit snapshot.

  branch172c.py FN_801F172C.jsonl ISBLOCKED.jsonl [GETTILECOLLISIONCODEALT.jsonl]

Batch 3 (logs/fntrace-answers/batch3) adds the third file: the corner block
(`moved && out && flag`) is replayed from the getTileCollisionCodeAlt_802265FC
probes logged under the call (their corner point and code), and the bent
direction is checked against *out at exit."""
import collections, json, math, struct, sys

F = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]
def vec(h, n):
    b = bytes.fromhex(h)
    return [struct.unpack('<f', b[4 * i:4 * i + 4])[0] for i in range(n)]

prim, nest = {}, collections.defaultdict(list)
for line in open(sys.argv[1]):
    r = json.loads(line)
    if r.get('record') == 'agg':
        continue
    prim[r['seq']] = r
for line in open(sys.argv[2]):
    r = json.loads(line)
    if r.get('record') == 'agg' or 'parent' not in r:
        continue
    nest[r['parent']].append(r)
corner = collections.defaultdict(list)
if len(sys.argv) > 3:
    for line in open(sys.argv[3]):
        r = json.loads(line)
        if r.get('record') == 'agg' or 'parent' not in r:
            continue
        corner[r['parent']].append(r)
corner_stats = collections.Counter()
corner_examples = []

def i32(x):
    return int(x)  # C truncation toward zero

stats = collections.Counter()
clauses = collections.Counter()
moves = collections.Counter()
mism = []
examples = []
for seq, r in sorted(prim.items()):
    p = {x['arg']: x for x in r['ptrs']}
    pos = vec(p[2]['in'], 3)
    pos_out = vec(p[2]['out'], 3)
    box = vec(p[3]['in'], 4)
    out_out = vec(p[4]['out'], 3)
    flag = r['args'][5]['v']
    probes = sorted(nest.get(seq, []), key=lambda x: x['seq'])
    if not probes:
        stats['no probes logged'] += 1
        continue
    it = iter(probes)
    bad = [False]
    def probe(x, y):
        try:
            q = next(it)
        except StopIteration:
            bad[0] = True
            return 0
        qx, qy, _ = vec(q['ptrs'][0]['in'], 3)
        if abs(qx - x) > 1e-3 or abs(qy - y) > 1e-3:
            bad[0] = True
        return q['ret']['v']
    left, top = box[0], box[1]
    right, bottom = F(box[2] - F(0.01)), F(box[3] - F(0.01))
    def edge(fixed_is_x, fixed, start, end, far):
        hits = [0, 0, 0]
        d = 0.0
        i = 0
        v = start
        while v < end:
            x, y = (fixed, v) if fixed_is_x else (v, fixed)
            if probe(x, y):
                hits[1 if i else 0] += 1
                d = far(x, y, False)
            i += 1
            v = F(v + 8.0)
        x, y = (fixed, end) if fixed_is_x else (end, fixed)
        if probe(x, y):
            hits[2] += 1
            d = far(x, y, True)
        return hits, d
    px, py = pos[0], pos[1]
    hL, dxL = edge(True, F(px + left), F(py + top), F(py + bottom),
                   lambda x, y, e: F(i32(F(F(0.125 * x) + 1.0)) * 8 - x))
    hR, dxR = edge(True, F(px + right), F(py + top), F(py + bottom),
                   lambda x, y, e: F(F(px + box[2]) - i32(F(0.125 * (F(px + box[2]) if e else x))) * 8))
    hT, dyT = edge(False, F(py + top), F(px + left), F(px + right),
                   lambda x, y, e: F(i32(F(F(0.125 * y) + 1.0)) * 8 - y))
    hB, dyB = edge(False, F(py + bottom), F(px + left), F(px + right),
                   lambda x, y, e: F(F(py + box[3]) - i32(F(0.125 * (F(py + box[3]) if e else y))) * 8))
    if next(it, None) is not None:
        bad[0] = True
    if bad[0]:
        stats['probe sequence differs from the replay'] += 1
        mism.append(seq)
        continue
    stats['replayed'] += 1
    def cond(name, a, o, ta, tb, da, db1, db2, oa, ob):
        # a = this edge's hits, o = opposite edge's hits; returns the first true clause or None
        c = [a[1] != 0 and o[1] == 0,
             a[0] != 0 and a[2] != 0,
             a[0] != 0 and ta == 0 and da < db1 and (oa == 0 or ob == 0),
             a[2] != 0 and tb == 0 and da < db2 and (ob == 0 or oa == 0)]
        for k, v in enumerate(c):
            if v:
                return k + 1
        return None
    cL = cond('L', hL, hR, hT[2], hB[2], dxL, dyT, dyB, hR[0], hR[2])
    cR = cond('R', hR, hL, hT[0], hB[0], dxR, dyT, dyB, hL[0], hL[2])
    cT = cond('T', hT, hB, hL[2], hR[2], dyT, dxL, dxR, hB[0], hB[2])
    cB = cond('B', hB, hT, hL[0], hR[0], dyB, dxL, dxR, hT[0], hT[2])
    nx = ny = 0.0
    ex, ey = px, py
    if cL: ex = F(ex + dxL); nx = 1.0
    if cR: ex = F(ex - dxR); nx = -1.0
    if cT: ey = F(ey + dyT); ny = 1.0
    if cB: ey = F(ey - dyB); ny = -1.0
    moved = bool(cL or cR or cT or cB)
    ok = abs(ex - pos_out[0]) < 1e-3 and abs(ey - pos_out[1]) < 1e-3 and moved == bool(r['ret']['v'])
    if moved and flag:
        # The corner block: the four corners of the entry box, each probed when its
        # edge hits and the push points away from it; codes 0xB, 0xA, 9, 8 bend it.
        cps = sorted(corner.get(seq, []), key=lambda x: x['seq'])
        x0, y0 = F(px + box[0]), F(py + box[1])
        x1, y1 = F(px + box[2]), F(py + box[3])
        want = [(hL[0] and (nx > 0 or ny > 0), x0, y0, 0xB, (1, 1), 'TL'),
                (hR[0] and (nx < 0 or ny > 0), x1, y0, 0xA, (-1, 1), 'TR'),
                (hL[2] and (nx > 0 or ny < 0), x0, y1, 9, (1, -1), 'BL'),
                (hR[2] and (nx < 0 or ny < 0), x1, y1, 8, (-1, -1), 'BR')]
        dx = dy = 0.0
        k = 0
        cbad = False
        seen = []
        for take, cx, cy, code, (ax, ay), nm in want:
            if not take:
                continue
            if k >= len(cps):
                cbad = True
                break
            q = cps[k]
            k += 1
            qx, qy = vec(q['ptrs'][0]['in'], 2) if q.get('ptrs') and q['ptrs'][0].get('in') else (None, None)
            if qx is None or abs(qx - cx) > 1e-3 or abs(qy - cy) > 1e-3:
                cbad = True
            got = q['ret']['v']
            seen.append((nm, got))
            corner_stats[f'probe {nm} code {got}'] += 1
            if got == code:
                dx += ax
                dy += ay
                corner_stats[f'bend {nm}'] += 1
        if k != len(cps):
            cbad = True
        corner_stats['corner block ran' + (' (probe sequence differs)' if cbad else '')] += 1
        if dx or dy:
            nx, ny = dx, dy
            corner_stats['direction bent'] += 1
        else:
            corner_stats['direction not bent'] += 1
        if (dx or dy or seen) and len(corner_examples) < 6:
            corner_examples.append(dict(seq=seq, pos_in=pos[:2], box=box, probes=seen, bent=[dx, dy], out=out_out))
    elif moved:
        corner_stats['moved, flag 0' if not flag else 'moved'] += 1
    if moved:
        l = math.hypot(nx, ny)
        okn = abs(out_out[0] - nx / l) < 1e-4 and abs(out_out[1] - ny / l) < 1e-4
        ok = ok and okn
    stats['replay matches exit *pos/*out/ret' if ok else 'replay differs at exit'] += 1
    for nm, c in (('L', cL), ('R', cR), ('T', cT), ('B', cB)):
        clauses[(nm, c)] += 1
    moves[''.join(nm for nm, c in (('L', cL), ('R', cR), ('T', cT), ('B', cB)) if c) or '-'] += 1
    if moved and len(examples) < 6:
        examples.append(dict(seq=seq, pos_in=pos[:2], box=box, hits=dict(L=hL, R=hR, T=hT, B=hB),
                             d=dict(dxL=dxL, dxR=dxR, dyT=dyT, dyB=dyB),
                             clause=dict(L=cL, R=cR, T=cT, B=cB), pos_out=pos_out[:2], out=out_out))
print(json.dumps(dict(stats=stats, moves=moves, corner=corner_stats, corner_examples=corner_examples,
                      clauses={f'{k[0]}:{k[1]}': v for k, v in sorted(clauses.items(), key=str)},
                      mismatched=mism[:10], examples=examples), indent=1))
