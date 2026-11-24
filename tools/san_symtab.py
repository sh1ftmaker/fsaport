#!/usr/bin/env python3
"""fsa.map (wasm-ld --Map) -> fsa_symtab.json for the sanitizer build.

    python3 tools/san_symtab.py build/san/fsa.map game/units.txt build/san/fsa_symtab.json [symbols.txt]

{demangled: [mangled, unit, sym]}, keyed by the demangled name because that
is what Chrome's wasm stack frames show:
  mangled  the clang (Itanium) symbol
  unit     the source file the function was compiled in (repo-relative, e.g.
           "game/src/game/connect_screen_803F0A54.cpp"), or "lib.a(object.o)"
           outside the game
  sym      the decomp's symbol (symbols.txt spelling, CodeWarrior mangling)
           when tools/diag_symtab.py's re-mangling finds it verbatim, or the
           only symbols.txt function with the same qualified name; "" otherwise
The sanitizer runtime (platform/sanitize/sanitize.cpp) fetches it to name the
function a check fired in."""
import json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diag_symtab as ds  # noqa: E402  (mwcc_mangle, load_symbols)

mapfile, units, out = sys.argv[1:4]
symfile = sys.argv[4] if len(sys.argv) > 4 else ''
syms, _, bykey = ds.load_symbols(symfile) if symfile and os.path.exists(symfile) else ({}, {}, {})

byobj = {}
for line in open(units):
    path = line.split('\t')[0]
    byobj[os.path.basename(path) + '.o'] = path.replace('@GENERATED@/', 'build/generated/') if path.startswith('@') else 'game/' + path


def decomp_sym(nm, unit, demangled):
    if not syms:
        return ''
    if not nm.startswith('_Z'):
        return nm if nm in syms else ''
    key = None
    try:
        mw, key = ds.mwcc_mangle(nm, unit)
        if mw in syms:
            return mw
    except Exception:
        pass
    q = re.sub(r'\(.*$', '', demangled)
    for k in (key, q):
        c = bykey.get(k) if k else None
        if c and len(c) == 1:
            return c[0]
    return ''


inp = re.compile(r'^\s+-\s+[0-9a-f]+\s+[0-9a-f]+\s+(\S+?\.a)\(([^)]+)\):\((\S+)\)$')
tab = {}
pending = None
for line in open(mapfile, errors='replace'):
    line = line.rstrip('\n')
    m = inp.match(line)
    if m:
        lib, obj, sym = m.groups()
        unit = byobj.get(obj) if lib.startswith('libfsa_units_') else None
        pending = [sym, unit or f'{lib}({obj})', unit is not None]
        continue
    if pending:
        parts = line.split(None, 3)
        if len(parts) == 4 and parts[0] == '-' and parts[3] not in tab:
            nm, unit, game = pending
            tab[parts[3]] = [nm, unit, decomp_sym(nm, unit, parts[3]) if game else '']
        pending = None
json.dump(tab, open(out, 'w'), separators=(',', ':'))
print(f'{len(tab)} symbols ({sum(1 for v in tab.values() if v[2])} with a decomp symbol) -> {out}')
