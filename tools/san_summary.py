#!/usr/bin/env python3
"""Collapse sanitizer streams to one record per site and print a table.

    python3 tools/san_summary.py logs/diag/<run>/sanitizer.jsonl [more.jsonl ...]
        [--json OUT] [--kind KIND] [--grep TEXT]

The runtime (platform/sanitize/sanitize.cpp) posts a site's record when it is
first hit and again while its count grows; the last line per "key" wins.
Several files (e.g. a title run and a ?stage=1 run) are merged: counts add up,
the first file's first_scene is kept. --json writes the merged records (one
JSON object per line, sorted by kind then file:line)."""
import argparse, collections, json

ap = argparse.ArgumentParser()
ap.add_argument('files', nargs='+')
ap.add_argument('--json')
ap.add_argument('--kind')
ap.add_argument('--grep')
a = ap.parse_args()

merged = {}
for f in a.files:
    last = {}
    for line in open(f):
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        last[r.get('key') or (r['kind'], r['file'], r['line'], r['col'])] = r
    for k, r in last.items():
        if k in merged:
            merged[k]['count'] += r['count']
        else:
            merged[k] = dict(r)

recs = sorted(merged.values(), key=lambda r: (r['kind'], r['file'], r['line'], r['col']))
if a.kind:
    recs = [r for r in recs if r['kind'] == a.kind]
if a.grep:
    recs = [r for r in recs if a.grep in json.dumps(r)]
if a.json:
    with open(a.json, 'w') as o:
        for r in recs:
            o.write(json.dumps(r) + '\n')
by = collections.Counter(r['kind'] for r in recs)
print(f'{len(recs)} sites: ' + ', '.join(f'{k} {n}' for k, n in by.most_common()))
for r in recs:
    print(f"{r['kind']:24} {r['count']:>9}  {r['file']}:{r['line']}:{r['col']}  {r['function'][:70]}\n"
          f"{'':36}{r['detail'][:150]}  [{r.get('first_scene', '')}]")
