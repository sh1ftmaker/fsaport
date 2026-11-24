#!/usr/bin/env python3
"""fsa.map (wasm-ld --Map) -> fsa_symtab.json for the sanitizer build.

    python3 tools/san_symtab.py build/san/fsa.map game/units.txt build/san/fsa_symtab.json

{demangled: [mangled, unit]} (keyed by the demangled name: that is what
Chrome's wasm stack frames show): unit is the game source file (repo-relative,
e.g. "game/src/game/connect_screen_803F0A54.cpp") the function was
compiled in, or "lib.a(object.o)" outside the game. The sanitizer runtime
(platform/sanitize/sanitize.cpp) fetches it to name the function a check fired
in and its unit."""
import json, os, re, sys

mapfile, units, out = sys.argv[1:4]
byobj = {}
for line in open(units):
    path = line.split('\t')[0]
    byobj[os.path.basename(path) + '.o'] = path.replace('@GENERATED@/', 'build/generated/') if path.startswith('@') else 'game/' + path

inp = re.compile(r'^\s+-\s+[0-9a-f]+\s+[0-9a-f]+\s+(\S+?\.a)\(([^)]+)\):\((\S+)\)$')
tab = {}
pending = None
for line in open(mapfile, errors='replace'):
    line = line.rstrip('\n')
    m = inp.match(line)
    if m:
        lib, obj, sym = m.groups()
        unit = byobj.get(obj) if lib.startswith('libfsa_units_') else None
        pending = [sym, unit or f'{lib}({obj})']
        continue
    if pending:
        parts = line.split(None, 3)
        if len(parts) == 4 and parts[0] == '-':
            tab.setdefault(parts[3], pending)
        pending = None
json.dump(tab, open(out, 'w'), separators=(',', ':'))
print(f'{len(tab)} symbols -> {out}')
