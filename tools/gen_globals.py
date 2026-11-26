#!/usr/bin/env python3
"""Definitions for the game's global data that no compiled unit defines.

In the matching build these come from dtk's split of the original DOL (`auto_*`
objects). The port has no such objects, so they are generated here, in two steps
that keep game bytes out of git and out of the built game:

    # 1. (developer, needs the decomp checkout) work out what to define and how
    #    each object is laid out: names, addresses, sizes, element widths and
    #    pointer targets. No game bytes. Writes tools/globals_layout.json.
    python3 tools/gen_globals.py layout --undefined build/undefined.txt \
        --decomp ../fsadecomp/fsa-decomp

    # 2. (every build; CMake runs it) the storage, from the layout alone:
    python3 tools/gen_globals.py emit build/web/generated

Step 2 writes globals.c: each object defined as a zero-filled, aligned byte
array of its size, and fsa_dol_blobs[], a table of (storage, DOL address, size,
element widths). At startup, before any constructor runs, platform/dol_data.cpp
copies each object's bytes from main.dol on the disc the player chose (read and
checked by platform/web/fsa_disc.js) and converts them from big-endian per
element. C++ does not mangle the names of global variables, so a byte array
links against the declaration of any type; the layout guarantees it gets that
type's bytes. Pointer tables are typed (`const void* const name[]`) so the
linker relocates them; what they point at (strings, byte tables) is copied
into fsa_dol_pool. The byte blobs the sources declare (ASSETS below) are filled
the same way.
"""
import argparse
import json
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LAYOUT = os.path.join(ROOT, 'tools', 'globals_layout.json')

# Element widths of the scalar and small-struct types that appear in declarations.
SCALARS = {
    's8': [1], 'u8': [1], 'char': [1], 'bool': [1], 'BOOL': [4], 'signed char': [1], 'unsigned char': [1],
    's16': [2], 'u16': [2], 'short': [2], 'unsigned short': [2],
    's32': [4], 'u32': [4], 'int': [4], 'long': [4], 'unsigned': [4], 'unsigned int': [4],
    'unsigned long': [4], 'f32': [4], 'float': [4],
    's64': [8], 'u64': [8], 'f64': [8], 'double': [8],
    'JUtility::TColor': [1, 1, 1, 1], 'GXColor': [1, 1, 1, 1],
    'JGeometry::TBox2f': [4] * 4, 'JGeometry::TBox2<f32>': [4] * 4, 'TBox2f': [4] * 4,
    'JGeometry::TVec3f': [4] * 3, 'JGeometry::TVec2f': [4] * 2, 'Vec': [4] * 3,
}


def parse_symbols(path):
    syms, by_addr = {}, {}
    for line in open(path):
        m = re.match(r'(\S+) = (\.\w+):0x([0-9A-Fa-f]+); // (.*)', line)
        if not m:
            continue
        name, sec, addr, rest = m.group(1), m.group(2), int(m.group(3), 16), m.group(4)
        size = re.search(r'size:0x([0-9A-Fa-f]+)', rest)
        e = {'name': name, 'section': sec, 'addr': addr, 'size': int(size.group(1), 16) if size else 0,
             'func': 'type:function' in rest}
        syms[name] = e
        by_addr.setdefault(addr, []).append(e)
    return syms, by_addr


def find_decls(names):
    decl = {}
    pat = re.compile(r'^[ \t]*extern\s+([^;(]*?)\b(\w+)\s*((?:\[[^\]]*\])*)\s*;', re.M)
    for root in ('game/include', 'game/src'):
        for dp, _, fs in os.walk(os.path.join(ROOT, root)):
            for f in sorted(fs):
                p = os.path.join(dp, f)
                for m in pat.finditer(open(p, errors='ignore').read()):
                    if m.group(2) in names and m.group(2) not in decl:
                        decl[m.group(2)] = (os.path.relpath(p, ROOT), m.group(1).strip(), m.group(3))
    return decl


def struct_fields(typename):
    """Element widths of a struct defined in a game source (simple POD fields only)."""
    pat = re.compile(r'struct\s+' + re.escape(typename) + r'\s*\{(.*?)\};', re.S)
    for root in ('game/src', 'game/include'):
        for dp, _, fs in os.walk(os.path.join(ROOT, root)):
            for f in fs:
                m = pat.search(open(os.path.join(dp, f), errors='ignore').read())
                if not m:
                    continue
                widths, off, maxa = [], 0, 1
                for line in m.group(1).split('\n'):
                    line = re.sub(r'//.*|/\*.*?\*/', '', line).strip()
                    if not line:
                        continue
                    fm = re.match(r'(.+?)\s+(\w+)\s*((?:\[(?:0x[0-9A-Fa-f]+|\d+)\])*)\s*;$', line)
                    if not fm or fm.group(1).strip() not in SCALARS:
                        return None
                    elem = SCALARS[fm.group(1).strip()]
                    n = 1
                    for d in re.findall(r'\[(0x[0-9A-Fa-f]+|\d+)\]', fm.group(3)):
                        n *= int(d, 0)
                    a = max(elem)
                    while off % a:
                        widths.append(1)
                        off += 1
                    widths += elem * n
                    off += sum(elem) * n
                    maxa = max(maxa, a)
                while off % maxa:  # tail padding, so the pattern repeats per element
                    widths.append(1)
                    off += 1
                return widths
    return None


def type_widths(base):
    """(widths, is_pointer) for the declared base type, or (None, _) if unknown."""
    t = re.sub(r'\bconst\b|\bvolatile\b|\bstatic\b', '', base).strip()
    t = re.sub(r'\s+', ' ', t)
    if t.endswith('*'):
        return [4], True
    if t in SCALARS:
        return SCALARS[t], False
    w = struct_fields(t)
    return w, False


def cmd_layout(a):
    syms, by_addr = parse_symbols(os.path.join(a.decomp, 'config/G4SE01/symbols.txt'))
    names = []
    txt = open(a.undefined).read()
    for group in txt.split('\n## ')[1:]:
        head, *lines = group.split('\n')
        for l in lines:
            l = l.strip()
            if not l:
                continue
            name = l.split(None, 1)[1]
            if '(' in name or (name in syms and syms[name]['func']):
                continue
            if head.startswith('game:') or re.search(r'_8[0-9A-F]{7}$', name):
                names.append(name)
    # Objects laid out before stay: once globals.c is built they no longer show as undefined.
    if os.path.exists(LAYOUT):
        old = json.load(open(LAYOUT))
        names += [e['name'] for e in old['objects']] + [e['name'] for e in old['unresolved']]
    names = sorted(set(names))
    decls = find_decls(set(names))
    fetch = read_dol(a.dol)
    name_starts = {int(n[-8:], 16) for n in names if re.search(r'_8[0-9A-F]{7}$', n)}
    starts = sorted(set(syms_e['addr'] for syms_e in syms.values()) | name_starts)
    out, problems = [], []
    for name in names:
        d = decls.get(name)
        s = locate(name, syms, d, starts, name_starts)
        if s is None:
            problems.append((name, 'not in symbols.txt and not inside any object there'))
            continue
        e = {'name': name, 'section': s['section'], 'addr': s['addr'], 'size': s['size']}
        if s.get('inside'):
            e['inside'] = s['inside']  # a part of a larger dtk object, defined separately
        e['decl'] = f'{d[1]} {name}{d[2]}' if d else None
        e['declared_in'] = d[0] if d else None
        if s['section'] in ('.bss', '.sbss', '.sbss2'):
            e['kind'] = 'bss'
        elif d is None:
            problems.append((name, 'no declaration found'))
            continue
        else:
            widths, is_ptr = type_widths(d[1])
            if is_ptr:
                e['kind'] = 'ptrs'
                e['targets'] = [pointer_target(v, syms, starts, fetch)
                                for v in struct.unpack(f'>{s["size"] // 4}I', fetch(s['addr'], s['size']))]
                out.append(e)
                continue
            if widths is None:
                problems.append((name, f'unknown layout for type "{d[1]}"'))
                continue
            if s['size'] % sum(widths):
                problems.append((name, f'size 0x{s["size"]:X} is not a multiple of "{d[1]}" ({sum(widths)} bytes)'))
                continue
            e['kind'] = 'data'
            e['widths'] = widths
        out.append(e)
    json.dump({'objects': out, 'unresolved': [{'name': n, 'why': w} for n, w in problems]},
              open(LAYOUT, 'w'), indent=1)
    print(f'{len(out)} objects laid out, {len(problems)} unresolved -> {LAYOUT}')
    for n, w in problems:
        print(f'  UNRESOLVED {n}: {w}')


def decl_size(d):
    """Byte size from the declaration when every dimension is given, else None."""
    if d is None:
        return None
    widths, is_ptr = type_widths(d[1])
    if widths is None:
        return None
    dims = re.findall(r'\[([^\]]*)\]', d[2])
    n = 1
    for x in dims:
        if not x.strip():
            return None
        n *= int(x, 0)
    return n * sum(widths)


def locate(name, syms, d, starts, name_starts):
    """Section, address and size of a symbol, including parts of larger dtk objects
    (the decomp names lbl_804A1460 where dtk has one lbl_804A1440 of 0x1F0 bytes)."""
    if name in syms:
        s = dict(syms[name])
        # A dtk object the decomp splits: this name covers only up to the next part.
        nxt = sorted(x for x in name_starts if s["addr"] < x < s["addr"] + s["size"])
        if nxt:
            s['size'] = nxt[0] - s['addr']
        return s
    m = re.search(r'_(8[0-9A-F]{7})$', name)
    if not m:
        return None
    addr = int(m.group(1), 16)
    box = next((s for s in syms.values() if not s['func'] and s['addr'] < addr < s['addr'] + s['size']), None)
    if box is None:
        return None
    end = box['addr'] + box['size']
    size = decl_size(d)
    if size is None:
        size = min(next(x for x in starts if x > addr), end) - addr
    return {'section': box['section'], 'addr': addr, 'size': min(size, end - addr), 'inside': box['name']}


def pointer_target(v, syms, starts, fetch):
    """What a pointer in an initialised table points at, from its value in the DOL."""
    if v == 0:
        return {'kind': 'null'}
    for n in (256, 64, 16, 4, 1):
        try:
            b = fetch(v, n)
            break
        except SystemExit:
            continue
    else:
        raise SystemExit(f'pointer 0x{v:08X} is outside the DOL')
    nul = b.find(b'\0')
    if nul > 0 and all(0x20 <= c < 0x7F or c in (9, 10) for c in b[:nul]):
        return {'kind': 'string', 'addr': v, 'len': nul}
    exact = [s for s in syms.values() if s['addr'] == v and not s['func'] and s['size']]
    size = exact[0]['size'] if exact else next(x for x in starts if x > v) - v
    return {'kind': 'bytes', 'addr': v, 'size': size}


def read_dol(path):
    data = open(path, 'rb').read()
    offs = struct.unpack('>18I', data[0x00:0x48])
    addrs = struct.unpack('>18I', data[0x48:0x90])
    sizes = struct.unpack('>18I', data[0x90:0xD8])
    secs = [(ad, o, sz) for o, ad, sz in zip(offs, addrs, sizes) if sz]

    def fetch(addr, size):
        for ad, o, sz in secs:
            if ad <= addr and addr + size <= ad + sz:
                return data[o + addr - ad:o + addr - ad + size]
        raise SystemExit(f'0x{addr:08X}+0x{size:X} is not in the DOL (wrong main.dol?)')
    return fetch


# The byte blobs the sources declare and the build fills from main.dol like the
# objects above (fonts, a message file, DSP microcode: no source form). The
# sources declare them extern; (name, address, size, alignment), from the
# decomp's config/G4SE01/config.yml `extract:` and symbols.txt.
ASSETS = [
    ('lbl_804985D0', 0x804985D0, 0x380, 16),               # dolphin/gba/GBAKey.c: GBA key-exchange DSP microcode
    ('lbl_80479CA0', 0x80479CA0, 0x260, 8),                # game/tu_80400DC4.cpp: the disc error screen's MESGbmg1
    ('lbl_80479F00', 0x80479F00, 0x12260, 8),              # game/tu_80400DC4.cpp: ...and its FONTbfn1
    ('JUTResFONT_Ascfont_fix12', 0x8045EEE0, 0x4160, 32),  # JUtility's built-in ASCII font
]


def cmd_emit(a):
    """Storage only: every object at its size and alignment, zero-filled, plus
    fsa_dol_blobs[], the recipe platform/dol_data.cpp follows at startup (before
    any constructor) to copy each object's bytes from the player's main.dol and
    make them little-endian per element. No game bytes are written here."""
    lay = json.load(open(LAYOUT))
    os.makedirs(a.outdir, exist_ok=True)
    lines, blobs, pool, pooled = [], [], [0], {}

    def pool_at(addr, size, align):
        """Offset in fsa_dol_pool of the copy of DOL bytes [addr, addr+size), plus a NUL."""
        if addr not in pooled:
            off = (pool[0] + align - 1) // align * align
            pooled[addr] = off
            pool[0] = off + size + 1
            blobs.append((f'fsa_dol_pool + {off}', addr, size, '1'))
        return pooled[addr]

    def align_of(size):
        return 8 if size % 8 == 0 else 4 if size % 4 == 0 else 2 if size % 2 == 0 else 1

    for e in lay['objects']:
        n, size = e['name'], e['size']
        align = align_of(size)
        if e['kind'] == 'bss':
            lines.append(f'__attribute__((aligned({align}))) unsigned char {n}[{size}];')
        elif e['kind'] == 'data':
            lines.append(f'__attribute__((aligned({align}))) unsigned char {n}[{size}];')
            blobs.append((n, e['addr'], size, ''.join(str(w) for w in e['widths'])))
        else:
            items = []
            for t in e['targets']:
                if t['kind'] == 'null':
                    items.append('0')
                elif t['kind'] == 'string':
                    items.append(f'fsa_dol_pool + {pool_at(t["addr"], t["len"], 1)}')
                else:
                    items.append(f'fsa_dol_pool + {pool_at(t["addr"], t["size"], 8)}')
            lines.append(f'const void* const {n}[{len(items)}] = {{{", ".join(items)}}};')
    for n, addr, size, align in ASSETS:
        lines.append(f'__attribute__((aligned({align}))) unsigned char {n}[{size}];')
        blobs.append((n, addr, size, '1'))
    out = ['/* Generated by tools/gen_globals.py from tools/globals_layout.json. */',
           '/* Storage only (no game bytes): platform/dol_data.cpp fills it from the player\'s main.dol. */',
           '',
           f'__attribute__((aligned(8))) unsigned char fsa_dol_pool[{max(pool[0], 1)}];',
           ''] + lines + [
           '',
           '/* dst, DOL address, size, element widths (repeating; each element is byte-swapped). */',
           'struct fsa_dol_blob { void* dst; unsigned addr; unsigned size; const char* widths; };',
           'const struct fsa_dol_blob fsa_dol_blobs[] = {']
    out += [f'  {{{d}, 0x{ad:08X}u, {sz}u, "{w}"}},' for d, ad, sz, w in blobs]
    out += ['};', f'const unsigned fsa_dol_nblobs = {len(blobs)};']
    open(os.path.join(a.outdir, 'globals.c'), 'w').write('\n'.join(out) + '\n')
    print(f'wrote {len(lay["objects"])} objects, {len(ASSETS)} assets, {len(blobs)} blobs '
          f'({sum(b[2] for b in blobs)} bytes from main.dol at startup) to {a.outdir}/globals.c')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    l = sub.add_parser('layout')
    l.add_argument('--undefined', default=os.path.join(ROOT, 'build/undefined.txt'))
    l.add_argument('--decomp', default=os.path.join(ROOT, '../fsadecomp/fsa-decomp'))
    l.add_argument('--dol', help='main.dol, to resolve pointer tables (default: the decomp\'s orig/sys/main.dol)')
    e = sub.add_parser('emit')
    e.add_argument('outdir')
    a = ap.parse_args()
    if a.cmd == 'layout' and not a.dol:
        a.dol = os.path.join(a.decomp, 'orig/sys/main.dol')
    cmd_layout(a) if a.cmd == 'layout' else cmd_emit(a)


if __name__ == '__main__':
    main()
