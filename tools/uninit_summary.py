#!/usr/bin/env python3
"""Summarise an "uninit" diagnostics stream (docs/DIAGNOSTICS.md).

  tools/uninit_summary.py logs/diag/<run>/uninit.jsonl [more.jsonl ...] [--json OUT]

The stream repeats a finding whenever its count changes; this keeps the last
line per key, merges runs (counts add up, first scene kept) and prints the
read-before-write findings first, grouped by class."""
import argparse, collections, json, sys

ORDER = {'read-before-write': 0, 'never-written-after-ctor': 1, 'poison-value-outside-heap': 2}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='+')
    ap.add_argument('--json', help='write the merged findings here (one per line)')
    ap.add_argument('--all', action='store_true', help='also print never-written and outside-heap findings')
    a = ap.parse_args()
    merged = {}
    for path in a.files:
        last = {}
        for line in open(path):
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except ValueError:
                continue
            k = r.get('key')
            if k in last:
                scene = last[k].get('scene')
                last[k] = r
                if scene is not None:
                    r['scene'] = scene
            else:
                last[k] = r
        for k, r in last.items():
            if k in merged:
                m = merged[k]
                m['count'] += r.get('count', 0)
                if 'objects' in r:
                    m['objects'] = m.get('objects', 0) + r['objects']
                m['runs'] += 1
            else:
                r = dict(r)
                r['runs'] = 1
                merged[k] = r
    rows = sorted(merged.values(), key=lambda r: (ORDER.get(r['evidence'], 9), str(r.get('class')), r.get('field', ''), -r['count']))
    if a.json:
        with open(a.json, 'w') as f:
            for r in rows:
                f.write(json.dumps(r) + '\n')
    by = collections.Counter(r['evidence'] for r in rows)
    print('findings:', ', '.join(f'{n} {e}' for e, n in sorted(by.items(), key=lambda x: ORDER.get(x[0], 9))))
    for r in rows:
        if r['evidence'] != 'read-before-write' and not a.all:
            continue
        cls = r.get('class') or ('(no ctor; allocated by %s)' % r.get('alloc_site', '?'))
        what = r.get('where') or r.get('ctor') or ''
        extra = f" objects={r['objects']}" if 'objects' in r else ''
        loc = r.get('field') or r.get('address')
        print(f"{r['evidence'][:5]} {cls} +{loc} size {r['size']} count {r['count']}{extra}  in {what}"
              f"  {json.dumps(r.get('scene'))}")

if __name__ == '__main__':
    main()
