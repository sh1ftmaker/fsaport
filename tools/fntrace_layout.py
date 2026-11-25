#!/usr/bin/env python3
"""Class layouts for the function traces: the port's (clang wasm32) offset of
each field, and the decomp offset its name carries (`field_0x2388`,
`mHeightGridIndex_0x2388`). The two differ where clang lays a class out
differently, e.g. after Aurora's larger GX structures (TileMap_804AA858: from
mSpriteTexObjs_0xb5c on, port = decomp + 0x2D00).

  tools/fntrace_layout.py --build build/diag CLASS [--unit SOURCE] [--json]
  tools/fntrace_layout.py --build build/diag --sym update__33PlayerSwordLevelCheckNPC_804AA590Fv

The layout comes from clang's -fdump-record-layouts-complete over the unit that
defines the function (fsa.diag.tsv `unit`), compiled with the build's own
command (ninja -t commands). Cached in BUILD/layouts/<class>.json.

Library use (tools/fntrace_run.py, tools/fntrace_summary.py):
  lay = layout_for_sym(build, sym)      # or layout(build, cls, unit)
  to_port(lay, decomp_off), to_decomp(lay, port_off) -> (decomp_off|None, field)
"""
import argparse, json, os, re, subprocess, sys

LINE = re.compile(r'^\s*(\d+)(?::\d+-\d+)? \|( +)(.*)$')


def ninja_command(build, unit):
    out = subprocess.run(['ninja', '-C', build, '-t', 'commands', 'fsa'], capture_output=True, text=True).stdout
    for line in out.split('\n'):
        if re.search(r' -c \S*' + re.escape(unit) + r'(\s|$)', line):
            return line
    return None


def dump(build, unit, cls):
    cmd = ninja_command(build, unit)
    if not cmd:
        raise SystemExit(f'no compile command for {unit} in {build}')
    cmd = re.sub(r' -o \S+', '', cmd)
    cmd = re.sub(r' -MD -MT \S+ -MF \S+', '', cmd)
    cmd += ' -fsyntax-only -Xclang -fdump-record-layouts-complete'
    txt = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=build).stdout
    lines = txt.split('\n')
    start = None
    for i, l in enumerate(lines):
        if re.match(r'^\s*0 \| (class|struct|union) ' + re.escape(cls) + r'$', l):
            start = i
            break
    if start is None:
        raise SystemExit(f'{cls}: no layout in {unit}')
    fields, size = [], None
    parents = []  # (indent, is_base)
    for l in lines[start + 1:]:
        if 'sizeof=' in l:
            m = re.search(r'sizeof=(\d+)', l)
            size = int(m.group(1))
            break
        m = LINE.match(l)
        if not m:
            continue
        off, ind, rest = int(m.group(1)), len(m.group(2)), m.group(3)
        depth = (ind - 1) // 2
        while parents and parents[-1][0] >= depth:
            parents.pop()
        is_base = rest.endswith('(base)') or rest.endswith('(primary base)') or rest.endswith('(virtual base)')
        # A direct member: depth 1, or a direct member of a base (chain of bases).
        direct = all(b for _, b in parents)
        parents.append((depth, is_base))
        if is_base or not direct:
            continue
        if rest.startswith('('):
            fields.append(dict(off=off, name=rest.strip('()'), type='', decomp=None))
            continue
        name = rest.split()[-1]
        typ = rest[:-len(name)].strip()
        d = re.search(r'_0x([0-9a-fA-F]+)$', name)
        fields.append(dict(off=off, name=name, type=typ, decomp=int(d.group(1), 16) if d else None))
    fields.sort(key=lambda f: f['off'])
    # Infer each field's decomp offset when its name has none: same delta as the
    # closest named field before it.
    delta = 0
    for f in fields:
        if f['decomp'] is not None:
            delta = f['off'] - f['decomp']
        f['delta'] = delta
    for k, f in enumerate(fields):
        f['size'] = (fields[k + 1]['off'] if k + 1 < len(fields) else (size or f['off'] + 4)) - f['off']
    return dict(cls=cls, unit=unit, size=size, fields=fields)


def layout(build, cls, unit):
    cache = os.path.join(build, 'layouts', re.sub(r'[^A-Za-z0-9_]', '_', cls) + '.json')
    if os.path.exists(cache):
        return json.load(open(cache))
    lay = dump(build, unit, cls)
    os.makedirs(os.path.dirname(cache), exist_ok=True)
    json.dump(lay, open(cache, 'w'), indent=0)
    return lay


def sym_prefix(s):
    """'name__<len><class>[C]F...' -> 'name__<len><class>[C]F' (the symbol without its
    parameter list), or None. As platform/web/fsa_fntrace.js: a decomp symbol whose
    parameter list differs from the port's still names one port function."""
    i = s.find('__', 1)
    if i < 0:
        return None
    j = i + 2
    m = re.match(r'(Q\d)?(\d+)', s[j:])
    if m and m.group(1):
        return None
    if m:
        j += len(m.group(2)) + int(m.group(2))
    if s[j:j + 1] == 'C':
        j += 1
    return s[:j + 1] if s[j:j + 1] == 'F' else None


def tsv_row(build, sym):
    rows = [line.rstrip('\n').split('\t') for line in open(os.path.join(build, 'fsa.diag.tsv'))]
    for f in rows:
        if len(f) >= 6 and (f[1] == sym or f[2] == sym):
            return f
    pre = sym_prefix(sym)
    cand = [f for f in rows if pre and len(f) >= 6 and sym_prefix(f[1]) == pre]
    return cand[0] if len({f[1] for f in cand}) == 1 else None


def class_of(name):
    """'A::B<x>::f(int)' -> 'A::B<x>' (the demangled qualified name minus the function)."""
    depth, cut = 0, None
    head = name
    for i, c in enumerate(name):
        if c in '<(':
            if c == '(' and depth == 0:
                head = name[:i]
                break
            depth += 1
        elif c in '>)':
            depth -= 1
    depth = 0
    for i, c in enumerate(head):
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif depth == 0 and head.startswith('::', i):
            cut = i
    return head[:cut] if cut else None


def layout_for_sym(build, sym):
    row = tsv_row(build, sym)
    if not row:
        return None
    cls = class_of(json.loads('"' + row[2] + '"'))
    unit = row[5]
    if unit.startswith('inline:'):
        unit = unit[len('inline:'):]
    if not cls or not unit:
        return None
    try:
        return layout(build, cls, unit)
    except SystemExit as e:
        print(f'fntrace_layout: {sym}: {e}', file=sys.stderr)
        return None


def layout_for_type(build, sym, cls):
    """The layout of class `cls` as the unit that defines `sym` sees it (a pointer
    argument's pointee type)."""
    row = tsv_row(build, sym)
    unit = row[5] if row else ''
    if unit.startswith('inline:'):
        unit = unit[len('inline:'):]
    if not unit:
        return None
    try:
        return layout(build, cls, unit)
    except SystemExit as e:
        print(f'fntrace_layout: {cls} ({sym}): {e}', file=sys.stderr)
        return None


def to_port(lay, d):
    """Decomp offset -> port offset (through the last field whose decomp offset is <= d)."""
    best = None
    for f in lay['fields']:
        dd = f['decomp'] if f['decomp'] is not None else f['off'] - f['delta']
        if dd <= d:
            best = (f, dd)
    if not best:
        return d
    f, dd = best
    return f['off'] + (d - dd)


def field_at(lay, p):
    best = None
    for f in lay['fields']:
        if f['off'] <= p:
            best = f
        else:
            break
    return best


def to_decomp(lay, p):
    """Port offset -> (decomp offset, field dict or None)."""
    f = field_at(lay, p)
    if not f:
        return p, None
    return p - f['delta'], f


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cls', nargs='?')
    ap.add_argument('--build', default='build/diag')
    ap.add_argument('--unit')
    ap.add_argument('--sym')
    a = ap.parse_args()
    lay = layout_for_sym(a.build, a.sym) if a.sym else layout(a.build, a.cls, a.unit)
    if not lay:
        sys.exit('no layout')
    print(f'{lay["cls"]} ({lay["unit"]}): port sizeof {lay["size"]:#x}')
    for f in lay['fields']:
        d = f['off'] - f['delta']
        print(f'  port 0x{f["off"]:05x}  decomp 0x{d:05x}{"  (shift %+#x)" % f["delta"] if f["delta"] else ""}  '
              f'{f["type"]} {f["name"]}')


if __name__ == '__main__':
    main()
