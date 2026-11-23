#!/usr/bin/env python3
"""Split `T x = e;` into `T x; x = e;` at each 'jump bypasses variable initialization'
note in build/compile_report.txt (MWCC lets a case label skip an initialised local)."""
import re, sys, collections
ROOT = '/bigdata/agent-tom/projects/fsaport/game/'
notes = collections.defaultdict(set)
for l in open(sys.argv[1]):
    m = re.match(r'(\S+?):(\d+):\d+: note: jump bypasses variable initialization', l)
    if m:
        notes[m.group(1).replace(ROOT, '')].add(int(m.group(2)))
decl = re.compile(r'^(\s*)((?:const\s+)?(?:unsigned\s+|signed\s+)?[A-Za-z_][\w:]*(?:<[^;=()]*>)?(?:\s*\*+\s*|\s+))(\w+)\s*=\s*(.+);(\s*//.*)?$')
for f, lines in notes.items():
    src = open(ROOT + f).read().split('\n')
    for n in sorted(lines):
        if n > len(src):
            print(f"SKIP {f}:{n}"); continue
        line = src[n - 1]
        m = decl.match(line)
        if not m:
            print(f'MANUAL {f}:{n}: {line.strip()}')
            continue
        ind, typ, name, init, cmt = m.groups()
        typ = re.sub(r'^const\s+', '', typ).rstrip()
        sep = '' if typ.endswith('*') else ' '
        src[n - 1] = f'{ind}{typ}{sep}{name};\n{ind}{name} = {init};{cmt or ""}'
    open(ROOT + f, 'w').write('\n'.join(src))
    print(f'fixed {f}: {len(lines)}')
