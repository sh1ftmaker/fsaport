#!/usr/bin/env python3
"""Compare the behaviour-harness streams of two runs (docs/HARNESS.md).

    python3 tools/harness_compare.py logs/diag/<runA> logs/diag/<runB> [--upto N] [--show 3]

Reads sync.jsonl and state.jsonl from each run directory (as tools/serve.py
--diag writes them), keys every record by (n, sync kind), and compares the
records both runs have up to the shorter run's last pad read (or --upto N).
Prints matching/differing counts per stream and the first differences, field
by field. "start" and "inputs_end" sync lines are not compared (they say how
the run was started). Exit status 0 when nothing differs.
"""
import argparse, json, os, sys


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--upto', type=int, help='compare reads n <= UPTO only')
    ap.add_argument('--show', type=int, default=3, help='differing records to print per stream')
    args = ap.parse_args()
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
        same = only_a = only_b = differ = 0
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
            d = list(diff(A[k], B[k]))
            if not d:
                same += 1
                continue
            differ += 1
            if shown < args.show:
                shown += 1
                print(f'  {stream} n={k[0]} {k[1]}: {len(d)} fields differ')
                for p, x, y in d[:8]:
                    print(f'    {p}: {json.dumps(x)} vs {json.dumps(y)}')
        bad += only_a + only_b + differ
        print(f'{stream}: reads 0..{last}: {same} identical, {differ} differ, {only_a} only in A, {only_b} only in B')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
