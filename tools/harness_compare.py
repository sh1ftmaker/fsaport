#!/usr/bin/env python3
"""Compare the behaviour-harness streams of two runs (docs/HARNESS.md).

    python3 tools/harness_compare.py logs/diag/<runA> logs/diag/<runB> [--upto N] [--show 3]

Reads sync.jsonl and state.jsonl from each run directory (as tools/serve.py
--diag writes them), keys every record by (n, sync kind), and compares the
records both runs have up to the shorter run's last pad read (or --upto N).
Prints matching/differing counts per stream and the first differences, field
by field. "start" and "inputs_end" sync lines are not compared (they say how
the run was started). Exit status 0 when nothing differs.

    python3 tools/harness_compare.py A B --anchor scene [--show 3]

Scene-anchored: for runs whose loads take different numbers of reads (the
port and Dolphin). The anchors are the "scene" and "step" sync lines, in
stream order; the two runs' anchor sequences are aligned (a diff of their
identities: scene (mode, stage, level) or step index), and every record is
re-keyed as (anchor, reads since that anchor, sync kind), belonging to the
latest anchor at or before its read. Each matched anchor's segment is
compared up to the shorter of the two segments (a segment runs to the next
anchor or the end of the run); anchors only one run has are listed. Segment
lengths that differ are reported but are not a difference (that is the
timing the anchors absorb). Exit status 0 when every anchor matches and no
compared record differs.
"""
import argparse, difflib, json, os, sys


def load(run, stream):
    path = os.path.join(run, stream + '.jsonl')
    out = {}
    if not os.path.exists(path):
        return out
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                o = json.loads(line)
            except json.JSONDecodeError:
                continue
            if o.get('sync') in ('start', 'inputs_end'):
                continue
            out[(o.get('n'), o.get('sync'))] = o
    return out


def diff(a, b, path=''):
    """Yields (path, a, b) for every leaf that differs."""
    if isinstance(a, dict) and isinstance(b, dict):
        for k in sorted(set(a) | set(b)):
            yield from diff(a.get(k, '<missing>'), b.get(k, '<missing>'), f'{path}.{k}' if path else k)
    elif isinstance(a, list) and isinstance(b, list):
        if len(a) != len(b):
            yield (path + '.length', len(a), len(b))
        for i, (x, y) in enumerate(zip(a, b)):
            yield from diff(x, y, f'{path}[{i}]')
    elif a != b:
        yield (path, a, b)


FTOL = None  # --ftol: relative tolerance for float fields (off by default)


def split_diffs(d):
    """(real differences, tolerated count, max relative error of the tolerated
    ones): with --ftol, a field where both sides are numbers, at least one not
    an integer, within FTOL relative error, is tolerated (float rounding, e.g.
    MWCC's fused multiply-add vs wasm's separate rounding), not a difference."""
    if FTOL is None:
        return d, 0, 0.0
    real, tol, worst = [], 0, 0.0
    for p, x, y in d:
        if (isinstance(x, (int, float)) and isinstance(y, (int, float)) and not isinstance(x, bool)
                and not isinstance(y, bool) and not (float(x).is_integer() and float(y).is_integer())):
            rel = abs(x - y) / max(abs(x), abs(y), 1e-30)
            if rel <= FTOL:
                tol += 1
                worst = max(worst, rel)
                continue
        real.append((p, x, y))
    return real, tol, worst


def load_list(run, stream):
    """The stream's records in file order (without "start" / "inputs_end")."""
    path = os.path.join(run, stream + '.jsonl')
    out = []
    if not os.path.exists(path):
        return out
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                o = json.loads(line)
            except json.JSONDecodeError:
                continue
            if o.get('sync') in ('start', 'inputs_end') or not isinstance(o.get('n'), int):
                continue
            out.append(o)
    return out


SEED_KEY = 'gGameRoot_8053AAF8->mRandom.seed'
SEED_STEP_LIMIT = 1 << 21


def lcg_steps(a, b, limit=SEED_STEP_LIMIT):
    """Steps of JMath::TRandom_fast_ (x = x * 0x19660D + 0x3C6EF35F) from a to b, or None."""
    x = a
    for k in range(limit + 1):
        if x == b:
            return k
        x = (x * 0x19660D + 0x3C6EF35F) & 0xFFFFFFFF
    return None


def seed_within_segments(seg):
    """Replaces the RNG seed in each segment's state records by how far the
    generator advanced since the segment's first record: the runs reach a scene
    after menus of different lengths (the port's card writes are instant), so
    their seeds differ there, but what happens inside the scene can still be
    compared. A seed the generator cannot reach is a reseed: marked, and
    counting restarts from it."""
    for recs in seg.values():
        base = prev = None
        total = 0
        for key in sorted(k for k in recs if True):
            g = recs[key].get('globals')
            if not isinstance(g, dict) or not isinstance(g.get(SEED_KEY), int):
                continue
            v = g[SEED_KEY] & 0xFFFFFFFF
            if base is None:
                base = prev = v
                g[SEED_KEY] = 'advances 0'
                continue
            k = lcg_steps(prev, v)
            if k is None:
                total = 0
                g[SEED_KEY] = 'reseeded'
            else:
                total += k
                g[SEED_KEY] = f'advances {total}'
            prev = v


def anchor_id(o):
    if o['sync'] == 'scene':
        return ('scene', o.get('mode'), o.get('stage'), o.get('level'))
    return ('step', o.get('step'))


def anchor_name(a):
    return f'scene({a[1]},{a[2]},{a[3]})' if a[0] == 'scene' else f'step {a[1]}'


class AnchoredRun:
    """A run's records re-keyed by (anchor ordinal, reads since the anchor, sync kind)."""

    def __init__(self, run, seed_mode='within'):
        sync = load_list(run, 'sync')
        state = load_list(run, 'state')
        self.anchors = []  # (identity, n)
        recs = {'sync': [], 'state': []}
        for o in sync:
            if o['sync'] in ('scene', 'step'):
                self.anchors.append((anchor_id(o), o['n']))
                recs['sync'].append((len(self.anchors) - 1, o))
            else:
                recs['sync'].append((None, o))
        for o in state:
            recs['state'].append((None, o))
        last = max([o['n'] for o in sync + state], default=-1)
        ns = [n for _, n in self.anchors]
        # Segment k: reads ns[k] .. (next anchor's read or the run's end) - 1.
        self.length = [(ns[k + 1] if k + 1 < len(ns) else last + 1) - ns[k] for k in range(len(ns))]
        # A record belongs to the last anchor at or before its read (stream order
        # puts a read's anchor lines before its other lines).
        self.seg = {'sync': {}, 'state': {}}
        for stream, rows in recs.items():
            for k, o in rows:
                if k is None:
                    k = self._owner(ns, o['n'])
                    if k is None:
                        continue
                off = o['n'] - ns[k]
                rec = {key: v for key, v in o.items() if key != 'n'}
                self.seg[stream].setdefault(k, {})[(off, o['sync'])] = rec
        if seed_mode == 'within':
            seed_within_segments(self.seg['state'])

    @staticmethod
    def _owner(ns, n):
        lo, hi = 0, len(ns)  # last index with ns[i] <= n
        while lo < hi:
            mid = (lo + hi) // 2
            if ns[mid] <= n:
                lo = mid + 1
            else:
                hi = mid
        return lo - 1 if lo else None


def compare_anchored(args):
    A, B = AnchoredRun(args.a, args.seed), AnchoredRun(args.b, args.seed)
    ida = [a for a, _ in A.anchors]
    idb = [b for b, _ in B.anchors]
    sm = difflib.SequenceMatcher(None, ida, idb, autojunk=False)
    pairs, only_a, only_b = [], [], []
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == 'equal':
            pairs += [(i1 + k, j1 + k) for k in range(i2 - i1)]
        else:
            only_a += range(i1, i2)
            only_b += range(j1, j2)
    totals = {s: [0, 0, 0, 0] for s in ('sync', 'state')}  # identical, differ, only A, only B
    tolerated = {s: [0, 0.0] for s in ('sync', 'state')}  # records with only --ftol float drift, max rel error
    shown = 0
    print(f'anchors: A {len(ida)}, B {len(idb)}; {len(pairs)} matched, {len(only_a)} only in A, {len(only_b)} only in B')
    for i, j in pairs:
        limit = min(A.length[i], B.length[j])
        counts = {}
        diffs = []
        for stream in ('sync', 'state'):
            sa, sb = A.seg[stream].get(i, {}), B.seg[stream].get(j, {})
            c = [0, 0, 0, 0]
            for key in sorted(k for k in set(sa) | set(sb) if k[0] < limit):
                if key not in sb:
                    c[2] += 1
                    diffs.append((stream, key, 'only in A', []))
                elif key not in sa:
                    c[3] += 1
                    diffs.append((stream, key, 'only in B', []))
                else:
                    d, ntol, worst = split_diffs(list(diff(sa[key], sb[key])))
                    if d:
                        c[1] += 1
                        diffs.append((stream, key, f'{len(d)} fields differ', d))
                    elif ntol:
                        tolerated[stream][0] += 1
                        tolerated[stream][1] = max(tolerated[stream][1], worst)
                    else:
                        c[0] += 1
            counts[stream] = c
            for t in range(4):
                totals[stream][t] += c[t]
        name = anchor_name(ida[i])
        lens = f'{A.length[i]}' if A.length[i] == B.length[j] else f'{A.length[i]}/{B.length[j]}'
        where = f'n={A.anchors[i][1]}' if A.anchors[i][1] == B.anchors[j][1] else f'n={A.anchors[i][1]}/{B.anchors[j][1]}'
        status = 'identical' if not diffs else 'DIFFERS'
        parts = []
        for stream in ('sync', 'state'):
            c = counts[stream]
            p = f'{stream} {c[0]}/{sum(c)}'
            if c[2] or c[3]:
                p += f' (+{c[2]} A only, +{c[3]} B only)'
            parts.append(p)
        print(f'  {"#%d/%d" % (i, j):<8} {name:<16} {where:<13} reads {lens:<9} {status}: {", ".join(parts)}')
        for stream, key, what, d in diffs:
            if shown >= args.show:
                break
            shown += 1
            print(f'      {stream} +{key[0]} {key[1]}: {what}')
            for p, x, y in d[:8]:
                print(f'        {p}: {json.dumps(x)} vs {json.dumps(y)}')
    for side, run, idx in (('A', A, only_a), ('B', B, only_b)):
        for k in idx:
            print(f'  only in {side}: #{k} {anchor_name(run.anchors[k][0])} at n={run.anchors[k][1]}')
    bad = len(only_a) + len(only_b)
    for stream in ('sync', 'state'):
        t = totals[stream]
        bad += t[1] + t[2] + t[3]
        tol = f', {tolerated[stream][0]} within --ftol (max rel {tolerated[stream][1]:.3g})' if FTOL is not None else ''
        print(f'{stream}: {t[0]} identical, {t[1]} differ{tol}, {t[2]} only in A, {t[3]} only in B (matched anchors, up to the shorter segment)')
    sys.exit(1 if bad else 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--upto', type=int, help='compare reads n <= UPTO only')
    ap.add_argument('--show', type=int, default=3, help='differing records to print per stream')
    ap.add_argument('--anchor', choices=['scene'], help='scene-anchored comparison (see above)')
    ap.add_argument('--seed', choices=['within', 'raw'], default='within',
                    help='with --anchor scene: compare the RNG seed as advances within each segment (default) or raw')
    ap.add_argument('--ftol', type=float, metavar='REL',
                    help='tolerate float fields within this relative error (e.g. 1e-5 for port vs Dolphin: '
                         'MWCC fuses a*b+c, wasm rounds twice); reported separately, never counted as identical. '
                         'Off by default: Dolphin vs Dolphin must stay bit-exact')
    args = ap.parse_args()
    global FTOL
    FTOL = args.ftol
    if args.anchor:
        compare_anchored(args)
    bad = 0
    for stream in ('sync', 'state'):
        A, B = load(args.a, stream), load(args.b, stream)
        if not A and not B:
            print(f'{stream}: no records')
            continue
        last = min(max((k[0] for k in A), default=-1), max((k[0] for k in B), default=-1))
        if args.upto is not None:
            last = min(last, args.upto)
        keys = sorted({k for k in set(A) | set(B) if k[0] is not None and k[0] <= last})
        same = only_a = only_b = differ = tolerated = 0
        tol_worst = 0.0
        shown = 0
        for k in keys:
            if k not in B:
                only_a += 1
                if shown < args.show:
                    print(f'  {stream} n={k[0]} {k[1]}: only in A')
                    shown += 1
                continue
            if k not in A:
                only_b += 1
                if shown < args.show:
                    print(f'  {stream} n={k[0]} {k[1]}: only in B')
                    shown += 1
                continue
            d, ntol, worst = split_diffs(list(diff(A[k], B[k])))
            if not d:
                if ntol:
                    tolerated += 1
                    tol_worst = max(tol_worst, worst)
                else:
                    same += 1
                continue
            differ += 1
            if shown < args.show:
                shown += 1
                print(f'  {stream} n={k[0]} {k[1]}: {len(d)} fields differ')
                for p, x, y in d[:8]:
                    print(f'    {p}: {json.dumps(x)} vs {json.dumps(y)}')
        bad += only_a + only_b + differ
        tol = f', {tolerated} within --ftol (max rel {tol_worst:.3g})' if FTOL is not None else ''
        print(f'{stream}: reads 0..{last}: {same} identical, {differ} differ{tol}, {only_a} only in A, {only_b} only in B')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
