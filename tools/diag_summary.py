#!/usr/bin/env python3
"""Collapse a diagnostics run (docs/DIAGNOSTICS.md) to one record per function.

    python3 tools/diag_summary.py logs/diag/<run> [more runs...] \
        [--structural ../fsadecomp/fsa-decomp/build/G4SE01/structural.json] [--top 25]

The "coverage" and "stubs" streams carry cumulative counts, re-sent whenever
they change, so the last record per id is the total. This writes, into each
run directory (or into --out for several runs merged):

  coverage_final.jsonl   last record per id (port function)
  coverage_by_sym.jsonl  one record per decomp symbol: {"sym", "names", "ids",
                         "calls" (max over the port functions that map to it:
                         clang's ctor/dtor variants call each other, so a sum
                         would count one CodeWarrior call twice), "calls_sum",
                         "first_scene", "first_frame", "units", "kinds"}; plus
                         with --structural the decomp's status for the symbol
  stubs_final.jsonl      last record per id of the "stubs" stream

and prints a short report.
"""
import argparse, collections, json, os, sys


def last_per_id(path):
    out = {}
    if not os.path.exists(path):
        return out
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except json.JSONDecodeError:
            continue
        if 'id' in r:
            out[(r.get('_run'), r['id'])] = r
    return out


def load_run(d, tag):
    cov, stubs = {}, {}
    for k, v in last_per_id(os.path.join(d, 'coverage.jsonl')).items():
        cov[(tag, k[1])] = v
    for k, v in last_per_id(os.path.join(d, 'stubs.jsonl')).items():
        stubs[(tag, k[1])] = v
    scenes = []
    p = os.path.join(d, 'scenes.jsonl')
    if os.path.exists(p):
        scenes = [json.loads(l) for l in open(p) if l.strip()]
    return cov, stubs, scenes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('runs', nargs='+')
    ap.add_argument('--structural')
    ap.add_argument('--out')
    ap.add_argument('--top', type=int, default=20)
    a = ap.parse_args()
    out = a.out or (a.runs[0] if len(a.runs) == 1 else None)
    if not out:
        sys.exit('several runs: pass --out DIR')
    os.makedirs(out, exist_ok=True)

    cov, stubs, scenes = {}, {}, {}
    for d in a.runs:
        tag = os.path.basename(os.path.normpath(d))
        c, s, sc = load_run(d, tag)
        cov.update(c)
        stubs.update(s)
        scenes[tag] = sc

    # merge several runs per id: calls add up, first hit is the earliest run listed
    per_id = {}
    for (tag, i), r in cov.items():
        r = dict(r, run=tag)
        if i in per_id:
            p = per_id[i]
            p['calls'] += r['calls']
            p.setdefault('runs', [p['run']]).append(tag)
        else:
            per_id[i] = r
    with open(os.path.join(out, 'coverage_final.jsonl'), 'w') as f:
        for i in sorted(per_id):
            f.write(json.dumps(per_id[i]) + '\n')

    status = {}
    if a.structural:
        st = json.load(open(a.structural))
        for unit, fns in st.get('units', {}).items():
            for sym, v in fns.items():
                status[sym] = {'unit': unit, 'match_percent': v[0], 'size': v[1], 'class': v[2]}
        for unit, fns in st.get('struct_fns', {}).items():
            for sym, v in fns.items():
                status.setdefault(sym, {'unit': unit})['structural'] = v

    by = {}
    for i, r in per_id.items():
        key = r['sym'] or ('?' + r['name'])
        b = by.setdefault(key, {'sym': r['sym'], 'names': [], 'ids': [], 'calls': 0, 'calls_sum': 0,
                                'first_scene': r.get('first_scene'), 'first_frame': r.get('first_frame', 0),
                                'first_run': r['run'], 'units': [], 'kinds': []})
        b['names'].append(r['name'])
        b['ids'].append(i)
        b['calls'] = max(b['calls'], r['calls'])
        b['calls_sum'] += r['calls']
        if (r['run'], r.get('first_frame', 0)) < (b['first_run'], b['first_frame']):
            b['first_scene'], b['first_frame'], b['first_run'] = r.get('first_scene'), r.get('first_frame', 0), r['run']
        if r['unit'] not in b['units']:
            b['units'].append(r['unit'])
        if r.get('kind') and r['kind'] not in b['kinds']:
            b['kinds'].append(r['kind'])
    for b in by.values():
        if b['sym'] in status:
            b['decomp'] = status[b['sym']]
    with open(os.path.join(out, 'coverage_by_sym.jsonl'), 'w') as f:
        for k in sorted(by, key=lambda k: -by[k]['calls']):
            f.write(json.dumps(by[k]) + '\n')

    st_final = {}
    for (tag, i), r in stubs.items():
        if i in st_final:
            st_final[i]['calls'] += r['calls']
        else:
            st_final[i] = dict(r, run=tag)
    with open(os.path.join(out, 'stubs_final.jsonl'), 'w') as f:
        for i in sorted(st_final, key=lambda i: -st_final[i]['calls']):
            f.write(json.dumps(st_final[i]) + '\n')

    # report
    hit = len(per_id)
    mapped = [r for r in per_id.values() if r['sym']]
    game = [r for r in per_id.values() if r['unit'].split(':')[-1].startswith('game/')]
    gmapped = [r for r in game if r['sym']]
    print(f'functions hit: {hit} (game/JSystem {len(game)}, platform {hit - len(game)})')
    print(f'  with a decomp symbol: {len(mapped)}; distinct symbols: {sum(1 for k in by if not k.startswith("?"))}')
    strong = [r for r in game if not r['unit'].startswith('inline:')]
    sun = [r for r in strong if not r['sym']]
    iun = [r for r in game if r['unit'].startswith('inline:') and not r['sym']]
    print(f'  game/JSystem hits without a symbol: {len(game) - len(gmapped)} of {len(game)}: '
          f'{len(sun)} of {len(strong)} out-of-line definitions ({100.0 * len(sun) / max(1, len(strong)):.1f}%; '
          f'decomp-only helpers the original inlined), {len(iun)} inline/template functions from headers')
    for tag, sc in scenes.items():
        print(f'scenes in {tag}: ' + ', '.join(f"{s['scene']['id']}=mode {s['scene'].get('mode')} stage {s['scene'].get('stage')}"
                                                  f" level {s['scene'].get('level')}" for s in sc))
    firsts = collections.Counter(json.dumps(r.get('first_scene', {}).get('id')) for r in per_id.values())
    print('first hits per scene id: ' + ', '.join(f'{k}: {v}' for k, v in sorted(firsts.items())))
    hits = [r for r in st_final.values() if r['calls']]
    print(f'stubs: {len(st_final)} known, {len(hits)} reached')
    for r in sorted(hits, key=lambda r: -r['calls'])[:a.top]:
        print(f"  {r['calls']:>10}  {r['kind']:<11} {r['sym'] or r['name']}")
    if status:
        open_hit = [b for b in by.values() if b.get('decomp') and b['decomp'].get('class') and b['decomp'].get('structural') != 'done']
        print(f'hit symbols the decomp lists as not matching: {len(open_hit)}; most called:')
        for b in sorted(open_hit, key=lambda b: -b['calls'])[:a.top]:
            d = b['decomp']
            print(f"  {b['calls']:>10}  {d.get('match_percent', 0):6.2f}% {d.get('class', ''):<24} {b['sym']}")


if __name__ == '__main__':
    main()
