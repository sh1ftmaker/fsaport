#!/usr/bin/env python3
"""Table of a batch-3 stage sweep (sweep.sh output lines "stage run-dir"):
drawSprite_801F06F0's mLayer_0x4 values, drawSobSprite_801EF574 calls, and the
actors on layers >= 8 in the state stream.

  python3 tools/harness_scripts/batch3/sweep_table.py SWEEP.log
"""
import collections, json, os, sys

for line in open(sys.argv[1]):
    parts = line.split()
    if len(parts) != 2 or not os.path.isdir(parts[1]):
        continue
    stage, run = parts
    layers, sob, sob_calls = collections.Counter(), 0, 0
    names = collections.Counter()
    p = os.path.join(run, 'fntrace_agg.jsonl')
    agg = {}
    if os.path.exists(p):
        for l in open(p):
            o = json.loads(l)
            agg[o['fn']] = o
    for o in agg.values():
        if o['fn_sym'].startswith('drawSprite_801F06F0'):
            for w in o.get('words', []):
                if w['off'] == 4:
                    for v, n in w.get('entry', []):
                        layers[int(v, 16)] += n
            for pa in o.get('ptrs', []):
                for s_, n in pa.get('strings', []):
                    names[s_] += n
        if o['fn_sym'].startswith('drawSobSprite_801EF574'):
            sob_calls += o['calls']
    hi = collections.Counter()
    p = os.path.join(run, 'state.jsonl')
    if os.path.exists(p):
        for l in open(p):
            o = json.loads(l)
            for a in o.get('actors') or []:
                if a['mLayer_0x4'] >= 8:
                    hi[(a['mActorId'], a['mLayer_0x4'])] += 1
    scene = ''
    p = os.path.join(run, 'sync.jsonl')
    if os.path.exists(p):
        sc = [json.loads(l) for l in open(p) if '"scene"' in l]
        if sc:
            scene = f'mode {sc[-1]["mode"]} {sc[-1]["stage"]}-{sc[-1]["level"]}'
    print(f'{stage:5} {scene:14} drawSprite layers {dict(sorted(layers.items()))}  drawSobSprite {sob_calls}  '
          f'sob names {dict(names)}  actors on >=8: {sorted(set(hi))}')
