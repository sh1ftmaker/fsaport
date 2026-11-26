#!/usr/bin/env python3
"""Check that built files carry none of the game data the port fills from main.dol.

    python3 tools/check_no_game_data.py path/to/main.dol BUILD_DIR_OR_FILE... [--expect-present]

For every object in tools/globals_layout.json and every blob in
tools/gen_globals.py ASSETS (plus each pointer table's strings and byte
tables), takes the DOL's bytes, both as stored (big-endian) and converted the
way the build used to embed them (little-endian per element), and looks for
them in each file: the whole object, and every 16-byte window with at least 6
distinct byte values.

Short strings and small index tables also occur in the game's own source
(a file name another unit spells out, a table a unit defines too). So a hit
in fsa.wasm is attributed through fsa.map (next to it) to the input section
that holds it: hits inside a unit other than generated/globals.c are that
unit's own source data and pass. Any hit in globals.c, or one the map does
not explain, fails. For other files (fsa.js, ...), an object fails when the
whole of it, or more than a quarter of its windows, is found.

A directory argument checks its fsa.wasm and fsa.js. Exit status 1 when
anything fails (with --expect-present: when nothing is found at all, to show
the check sees data that is embedded).
"""
import argparse
import bisect
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gen_globals  # noqa: E402

WINDOW = 16


def to_little(blob, widths):
    out, i, k = bytearray(), 0, 0
    while i < len(blob):
        w = widths[k % len(widths)]
        out += blob[i:i + w][::-1]
        i += w
        k += 1
    return bytes(out)


def probes(dol):
    fetch = gen_globals.read_dol(dol)
    lay = json.load(open(gen_globals.LAYOUT))
    out = []  # (label, bytes)
    for e in lay['objects']:
        if e['kind'] == 'data':
            b = fetch(e['addr'], e['size'])
            out.append((e['name'], b))
            out.append((e['name'] + ' (LE)', to_little(b, e['widths'])))
        elif e['kind'] == 'ptrs':
            for t in e['targets']:
                if t['kind'] == 'string':
                    out.append((f'{e["name"]} -> string @{t["addr"]:08X}', fetch(t['addr'], t['len'])))
                elif t['kind'] == 'bytes':
                    out.append((f'{e["name"]} -> bytes @{t["addr"]:08X}', fetch(t['addr'], t['size'])))
    for name, addr, size, _ in gen_globals.ASSETS:
        out.append((name, fetch(addr, size)))
    return out


def interesting(w):
    return len(set(w)) >= 6


def find_all(data, needle):
    i = data.find(needle)
    while i >= 0:
        yield i
        i = data.find(needle, i + 1)


def leb(b, i):
    v = sh = 0
    while True:
        x = b[i]
        i += 1
        v |= (x & 0x7F) << sh
        sh += 7
        if x < 0x80:
            return v, i


def wasm_segments(data):
    """The data section's active segments: (file offset, length, memory address)."""
    segs, i = [], 8
    while i < len(data):
        sid = data[i]
        size, j = leb(data, i + 1)
        if sid == 11:
            n, k = leb(data, j)
            for _ in range(n):
                flags, k = leb(data, k)
                if flags == 2:
                    _, k = leb(data, k)
                addr = None
                if flags in (0, 2):
                    assert data[k] == 0x41  # i32.const
                    addr, k = leb(data, k + 1)  # (non-negative addresses: unsigned read is fine)
                    assert data[k] == 0x0B
                    k += 1
                ln, k = leb(data, k)
                if addr is not None:
                    segs.append((k, ln, addr))
                k += ln
        i = j + size
    return segs


def load_map(path, data):
    """Data symbols of the wasm-ld map, by memory address, and the wasm's segments.
    (The map's file offsets are wasm-ld's, before wasm-opt; memory addresses hold.)"""
    secs = []
    for line in open(path):
        m = re.match(r'\s*([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+(\S.*\(\.[^)]*\))$', line)
        if m:
            secs.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3).strip()))
    secs.sort()
    return secs, [s[0] for s in secs], wasm_segments(data)


def attribute(mp, off):
    secs, starts, segs = mp
    for fo, ln, addr in segs:
        if fo <= off < fo + ln:
            va = addr + off - fo
            i = bisect.bisect_right(starts, va) - 1
            if i >= 0 and secs[i][0] <= va < secs[i][0] + secs[i][1]:
                return secs[i][2]
            return f'(data at 0x{va:x}, no symbol)'
    return None


def check_file(path, pr, mp):
    data = open(path, 'rb').read()
    found = fails = 0
    lines = []
    for label, b in pr:
        wins = [i for i in range(0, max(0, len(b) - WINDOW + 1), 4) if interesting(b[i:i + WINDOW])]
        hits = [i for i in wins if b[i:i + WINDOW] in data]
        whole = len(b) >= 8 and interesting(b) and b in data
        if not hits and not whole:
            continue
        found += 1
        if mp:
            where = set()
            needles = [b] if whole else [b[i:i + WINDOW] for i in hits]
            for nd in needles:
                for off in find_all(data, nd):
                    where.add(attribute(mp, off) or f'(outside the data section: file offset 0x{off:x})')
            bad = [w for w in where if 'globals.c' in w or w.startswith('(')]
            ok = not bad
            how = ', '.join(sorted(where))
        else:
            ok = not whole and len(hits) * 4 <= len(wins)
            how = 'no map'
        fails += not ok
        lines.append(f'  {"ok  " if ok else "FAIL"} {label} ({len(b)} bytes): '
                     f'{"whole object, " if whole else ""}{len(hits)}/{len(wins)} windows; in {how}')
    print(f'{path}: {len(pr)} objects checked, {found} with matching bytes, {fails} failing')
    for ln in lines:
        print(ln)
    return found, fails


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('dol')
    ap.add_argument('paths', nargs='+')
    ap.add_argument('--expect-present', action='store_true')
    a = ap.parse_args()
    pr = probes(a.dol)
    files = []
    for p in a.paths:
        files += [os.path.join(p, 'fsa.wasm'), os.path.join(p, 'fsa.js')] if os.path.isdir(p) else [p]
    found = fails = 0
    for f in files:
        mpath = os.path.join(os.path.dirname(f), 'fsa.map')
        mp = load_map(mpath, open(f, 'rb').read()) if f.endswith('.wasm') and os.path.exists(mpath) else None
        fo, fa = check_file(f, pr, mp)
        found += fo
        fails += fa
    if a.expect_present:
        sys.exit(0 if found else 1)
    print('PASS: no game data from main.dol' if not fails else f'FAIL: {fails} objects')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
