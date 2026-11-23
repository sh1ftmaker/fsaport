#!/usr/bin/env python3
"""Generate byte-swap tables for file-image structs from clang's record layouts.

    python3 tools/gen_swap_tables.py OUT.h TYPE [TYPE ...]

Compiles a unit that takes sizeof() of each TYPE with the port's flags and
-fdump-record-layouts, flattens every record into its multi-byte scalar leaves
(offset, size), and writes OUT.h with one table per type:

    static const unsigned short kPortSwap_TYPE[] = { offset, size, ... };
    static const unsigned int kPortSwapSize_TYPE = sizeof(TYPE);

The struct layouts are the decomp's (they mirror the file), so swapping these
leaves converts one element of a big-endian table to host order in place.
Needs emscripten on PATH (em++).
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, 'game')
HEADERS = [
    'JSystem/J3D/J3DFileBlock.h', 'JSystem/J3D/J3DMaterialFactory.h', 'JSystem/J3D/J3DTypes.h',
    'JSystem/J3D/J3DTexMtx.h', 'JSystem/J3D/J3DPE.h', 'JSystem/J3D/J3DMaterial.h',
    'JSystem/J3D/J3DJointFactory.h', 'JSystem/J3D/J3DShape.h', 'JSystem/J3D/J3DShapeFactory.h',
    'JSystem/J3D/J3DModel.h', 'JSystem/J3D/J3DInd.h', 'dolphin/gx.h',
]
# Array typedefs clang prints by name: (element type, element count).
ARRAY_TYPEDEFS = {'Mtx23': ('f32', 6), 'Mtx': ('f32', 12), 'Mtx44': ('f32', 16), 'Mtx33': ('f32', 9),
                  'Mtx3P': ('f32', 9), 'Vec': ('f32', 3), 'Quaternion': ('f32', 4), 'S16Vec': ('s16', 3),
                  'GXColor': ('u8', 4), 'Vec2': ('f32', 2)}
SCALAR = {
    'u8': 1, 's8': 1, 'char': 1, 'unsigned char': 1, 'signed char': 1, 'bool': 1, '_Bool': 1, 'GXBool': 1,
    'u16': 2, 's16': 2, 'short': 2, 'unsigned short': 2,
    'u32': 4, 's32': 4, 'f32': 4, 'int': 4, 'unsigned int': 4, 'long': 4, 'unsigned long': 4, 'float': 4,
    'u64': 8, 's64': 8, 'f64': 8, 'double': 8, 'long long': 8, 'unsigned long long': 8,
}


def dump(types):
    src = ''.join(f'#include "{h}"\n' for h in HEADERS)
    src += 'unsigned int port_sizes[] = {' + ', '.join(f'sizeof({t})' for t in types) + '};\n'
    with tempfile.NamedTemporaryFile('w', suffix='.cpp', delete=False) as f:
        f.write(src)
        path = f.name
    cmd = ['em++', '-fsyntax-only', '-std=gnu++17', '-nostdinc', '-fno-rtti', '-fno-exceptions', '-fsigned-char',
           '-Wno-everything', '-DFSA_PORT=1', '-DNDEBUG=1', '-include', os.path.join(GAME, 'port/port_prefix.h'),
           '-I' + os.path.join(GAME, 'libs/sdk/include'), '-I' + os.path.join(GAME, 'libs/sdk/msl_include'),
           '-Xclang', '-fdump-record-layouts-complete', path]
    out = subprocess.run(cmd, capture_output=True, text=True)
    os.unlink(path)
    if out.returncode:
        sys.exit(out.stderr)
    return out.stdout


def parse(text):
    """{record name: (size, [(offset, depth, type, name)])}"""
    records = {}
    for block in text.split('*** Dumping AST Record Layout')[1:]:
        lines = [l for l in block.split('\n') if '|' in l]
        head = re.match(r'\s*(\d+)\s*\|\s*(?:struct|class|union)\s+(.+?)\s*$', lines[0])
        if not head:
            continue
        name = head.group(2)
        m = re.search(r'\[sizeof=(\d+)', block)
        size = int(m.group(1)) if m else 0
        fields = []
        for l in lines[1:]:
            m = re.match(r'\s*(\d+)(?::\d+-\d+)?\s*\|(\s*)(.*)$', l)
            if not m:
                continue
            fields.append((int(m.group(1)), len(m.group(2)) - 1, m.group(3).strip()))
        records[name] = (size, fields, 'union' in lines[0].split('|')[1])
    return records


def leaves(records, name, base=0, seen=None):
    """Multi-byte scalar leaves of a record: [(offset, size)]."""
    size, fields, is_union = records[name]
    out = []
    if not fields:
        return out
    top = min(d for _, d, _ in fields)
    for off, depth, decl in fields:
        if depth != top:
            continue  # nested members: expanded through their own record
        decl = re.sub(r'^(struct|class|union|enum)\s+', '', decl)
        if decl.startswith('(') or 'vtable pointer' in decl or decl.endswith(' (base)') or decl.endswith('(primary base)'):
            if decl.endswith('(base)') or decl.endswith('(primary base)'):
                bname = re.sub(r'\s*\((primary )?base\)$', '', decl)
                bname = re.sub(r'^(struct|class)\s+', '', bname)
                if bname in records:
                    out += leaves(records, bname, base + off)
            continue
        m = re.match(r'(.+?)\s+(\w+)((?:\[\d+\])*)$', decl)
        if not m:
            continue
        typ = m.group(1).strip()
        # clang prints array dimensions on the type: "u16[2] mFoo"
        dims = [int(x) for x in re.findall(r'\[(\d+)\]', typ + m.group(3))]
        typ = re.sub(r'\s*\[\d+\]', '', typ).strip()
        count = 1
        for d in dims:
            count *= d
        typ = re.sub(r'^(const|volatile)\s+', '', typ)
        if typ in ARRAY_TYPEDEFS:
            typ, n = ARRAY_TYPEDEFS[typ]
            count *= n
        if typ.endswith('*'):
            esize, sub = 4, None
        elif typ in SCALAR:
            esize, sub = SCALAR[typ], None
        elif typ.startswith('_GX') or typ.startswith('GX') and typ not in records:
            esize, sub = 4, None  # an enum
        elif typ in records:
            esize, sub = records[typ][0], typ
        elif re.sub(r'^\w+::', '', typ) in records:
            sub = re.sub(r'^\w+::', '', typ)
            esize = records[sub][0]
        else:
            # typedefs such as Mtx (f32[3][4]) are printed resolved; anything else is an error
            sys.exit(f'{name}: unknown field type {typ!r} in {decl!r}')
        for i in range(count):
            o = base + off + i * esize
            if sub:
                out += leaves(records, sub, o)
            elif esize > 1:
                out.append((o, esize))
    if is_union:
        # a union's members overlap: keep the first member's leaves only
        firsts = [f for f in fields if f[1] == top]
        return leaves_first(records, name, base) if firsts else out
    return out


def leaves_first(records, name, base):
    size, fields, _ = records[name]
    top = min(d for _, d, _ in fields)
    first = [f for f in fields if f[1] == top][:1]
    saved = records[name]
    records[name] = (size, [f for f in fields if f[1] != top or f in first], False)
    try:
        return leaves(records, name, base)
    finally:
        records[name] = saved


def main():
    out_path, types = sys.argv[1], sys.argv[2:]
    records = parse(dump(types))
    lines = ['// Generated by tools/gen_swap_tables.py from clang record layouts. Do not edit.',
             '// Each table lists (offset, size) of the multi-byte scalars of one element.', '#pragma once', '']
    for t in types:
        if t not in records:
            # an empty struct (a placeholder in the decomp): nothing to swap
            print(f'{t}: no layout (empty struct), nothing swapped')
            lines.append(f'static const unsigned short kPortSwap_{t}[] = {{ 0, 0 }};')
            lines.append(f'static const unsigned int kPortSwapCount_{t} = 0;')
            lines.append(f'static const unsigned int kPortSwapSize_{t} = 0;')
            continue
        lv = sorted(set(leaves(records, t)))
        flat = ', '.join(f'{o}, {s}' for o, s in lv) or '0, 0'
        lines.append(f'static const unsigned short kPortSwap_{t}[] = {{ {flat} }};')
        lines.append(f'static const unsigned int kPortSwapCount_{t} = {len(lv)};')
        lines.append(f'static const unsigned int kPortSwapSize_{t} = {records[t][0]};')
        print(f'{t}: size {records[t][0]}, {len(lv)} multi-byte fields')
    open(out_path, 'w').write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
