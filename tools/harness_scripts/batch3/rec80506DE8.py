#!/usr/bin/env python3
"""Decode the lbl_80506DE8 records (cls_804AA858Sub11C[4], 0x68 each) in the
port's memory (clang, little-endian: bitfields allocated from the low bit of
each u32 container), from fntrace records of fn_80306A94 / the constructor.
Prints which decoded fields changed per call, with their old -> new values.
Batch 3: grouped by the call's player argument (P<player> [record]); only the
record of the call's own player is shown (fn_80306A94 writes only that one)."""
import collections, json, sys


def dec(b):
    d = {}
    d['field_0x0'] = b[0]
    d['mMask'] = b[1] & 15
    d['field_0x1_0'] = b[1] >> 4
    d['mMode'] = b[2] & 3
    d['field_0x2_5'] = (b[2] >> 2) & 1
    d['mValid'] = (b[2] >> 3) & 7
    d['mPlayer'] = b[2] >> 6
    d['mKind'] = b[3] & 15
    d['field_0x3_2'] = (b[3] >> 4) & 3
    d['field_0x3_0'] = b[3] >> 6
    w = int.from_bytes(b[4:8], 'little')
    d['field_0x4.mWord'] = w
    for i in range(4):
        e = b[8 + 4 * i:12 + 4 * i]
        p = f'mEntries[{i}].'
        d[p + 'field_0x0'] = e[0]
        d[p + 'mY'] = e[1]
        d[p + 'mXLow'] = e[2]
        d[p + 'mScroll'] = e[3] & 15
        d[p + 'mScrollBit'] = (e[3] >> 4) & 1
        d[p + 'field_0x3_1'] = (e[3] >> 5) & 3
        d[p + 'mXHigh'] = e[3] >> 7
    for i in range(20):
        e = b[0x18 + 4 * i:0x1c + 4 * i]
        d[f'field_0x18[{i}].mWord'] = int.from_bytes(e, 'little')
    return d


changes = collections.defaultdict(collections.Counter)
values = collections.defaultdict(collections.Counter)
n = 0
for line in open(sys.argv[1]):
    r = json.loads(line)
    if r.get('record') == 'agg':
        continue
    for g in r.get('globals', []):
        if g['name'] != 'lbl_80506DE8':
            continue
        bi, bo = bytes.fromhex(g['in']), bytes.fromhex(g['out'])
        n += 1
        pl = r['args'][1]['v'] if len(r.get('args', [])) > 1 else -1
        for rec in range(len(bi) // 0x68):
            if pl >= 0 and rec != pl:
                continue
            a, b = dec(bi[rec * 0x68:(rec + 1) * 0x68]), dec(bo[rec * 0x68:(rec + 1) * 0x68])
            for k in a:
                values[(pl, rec, k)][b[k]] += 1
                if a[k] != b[k]:
                    changes[(pl, rec, k)][(a[k], b[k])] += 1
print(f'{n} logged calls')
for (pl, rec, k), c in sorted(changes.items()):
    tr = ', '.join(f'{o:#x}->{v:#x} x{m}' if 'mWord' in k else f'{o}->{v} x{m}' for (o, v), m in c.most_common(6))
    vs = values[(pl, rec, k)]
    print(f'P{pl} [{rec}].{k}: changed in {sum(c.values())}; {tr}{" ..." if len(c) > 6 else ""}; values after: '
          + ', '.join((f'{v:#x}' if 'mWord' in k else str(v)) + f' x{m}' for v, m in vs.most_common(6)))
