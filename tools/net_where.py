#!/usr/bin/env python3
"""Which static symbol holds an address (for netplay snapshot diffs, docs/NETPLAY.md).

    tools/net_where.py build/web/fsa.map 0xb3ab4 0xcd5a1 ...
"""
import bisect
import re
import sys


def main():
    syms = []
    sec = None
    for line in open(sys.argv[1]):
        p = line.split()
        if len(p) == 4 and re.fullmatch(r'\.[a-z_]+', p[3]):  # an output section (not a .L symbol)
            sec = p[3]
            continue
        if sec not in ('.data', '.bss', '.rodata') or len(p) < 4:
            continue
        m = re.match(r'(.*?):\((\.[^)]*)\)', p[3])
        if not m:
            continue
        a, n = int(p[0], 16), int(p[2], 16)
        obj = m.group(1).split('/')[-1]
        syms.append((a, n, obj, m.group(2)))
    syms.sort()
    starts = [s[0] for s in syms]
    for x in sys.argv[2:]:
        v = int(x, 16)
        i = bisect.bisect_right(starts, v) - 1
        if i >= 0 and syms[i][0] <= v < syms[i][0] + max(syms[i][1], 1):
            a, n, obj, sec = syms[i]
            print(f'{x}: {obj} {sec} +0x{v - a:x} (size 0x{n:x})')
        else:
            print(f'{x}: ?')


if __name__ == '__main__':
    main()
