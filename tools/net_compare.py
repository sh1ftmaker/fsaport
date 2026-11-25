#!/usr/bin/env python3
"""Compare the browsers of one netplay room (docs/NETPLAY.md, "Measuring").

Finds the diagnostics run directories whose net.jsonl names the room, prints
each one's last netplay report, and compares every browser's sync/state/inputs
streams with the first one's (tools/harness_compare.py, per read, up to the
shorter run). All browsers of a room must be identical.

    tools/net_compare.py logs/diag-net ROOMCODE
"""
import json
import os
import subprocess
import sys


def main():
    root, room = sys.argv[1], sys.argv[2]
    runs = []
    for d in sorted(os.listdir(root)):
        p = os.path.join(root, d, 'net.jsonl')
        if not os.path.exists(p):
            continue
        start, last = None, None
        for line in open(p):
            try:
                o = json.loads(line)
            except ValueError:
                continue
            if o.get('t') == 'start' and o.get('room') == room:
                start = o
            elif start and 'head' in o:
                last = o
        if start:
            runs.append((start['seat'], os.path.join(root, d), last))
    runs.sort()
    if len(runs) < 2:
        print(f'room {room}: {len(runs)} run(s) found in {root}')
        return 1
    keys = ['head', 'delay', 'rollbacks', 'resimFrames', 'maxDepth', 'stalls', 'stallMs', 'stallMax', 'predicted',
            'mispredicted', 'gatedFrames', 'timeSyncWaits', 'refused', 'desyncs', 'snapAvgMs', 'restoreAvgMs',
            'resimAvgMs', 'leadAvg', 'advantage', 'rtt', 'rttDev', 'hashesSent', 'relayDesyncs', 'bytesOut', 'bytesIn']
    for seat, d, last in runs:
        r = {k: (round(last[k], 2) if isinstance(last.get(k), float) else last.get(k)) for k in keys if last and k in last}
        print(f'seat {seat + 1}: {os.path.basename(d)} {json.dumps(r)}')
    tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'harness_compare.py')
    ok = True
    for seat, d, _ in runs[1:]:
        out = subprocess.run([sys.executable, tool, runs[0][1], d], capture_output=True, text=True).stdout
        lines = [l for l in out.splitlines() if l.startswith(('sync:', 'state:', 'inputs:'))]
        print(f'seat 1 vs seat {seat + 1}: ' + ' | '.join(lines))
        if any(' 0 differ' not in l for l in lines if 'differ' in l):
            ok = False
    print('IDENTICAL' if ok else 'DIFFERENT')
    return 0 if ok else 2


if __name__ == '__main__':
    sys.exit(main())
