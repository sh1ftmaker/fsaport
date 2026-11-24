#!/usr/bin/env python3
"""Static candidates for raw big-endian reads (docs/diag/endian_audit.md has the triaged list).

    python3 tools/endian_audit.py [--json] [paths...]   (default: game/src/game game/include/game)

Lists, outside `#ifndef FSA_PORT` / `#else` branches of `#ifdef FSA_PORT`:
  raw-cast   *(u16*)p, ((const s32*)p)[i], reinterpret_cast<u32*>(...) and the like
  overlay    a struct cast from a resource pointer (getResource, res, data, buf,
             ->ptr, file, header) whose multi-byte fields are not BE<>
  bitfield   bit fields (MWCC allocates from the high bit, clang from the low bit)
  union      unions that view one integer as bytes / bit fields / halves
These are candidates: GBA-derived data ("SOB ", "SCH ", "SCL ", "SMP ", "SCN ")
is little-endian and read byte by byte, and data swapped once at load is host order.
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INT = r'(?:u16|s16|u32|s32|f32|f64|u64|s64|int|short|float|double|long|unsigned\s+(?:short|int|long)|signed\s+(?:short|int|long)|OSTime|OSTick)'
RAW_CAST = re.compile(
    r'\*\s*\(\s*(?:const\s+)?(?:volatile\s+)?' + INT + r'\s*\*\s*\)'
    r'|\(\s*\(\s*(?:const\s+)?' + INT + r'\s*\*\s*\)\s*[A-Za-z_][\w.>-]*\s*\)\s*\['
    r'|\(\s*(?:const\s+)?' + INT + r'\s*\*\s*\)\s*\(?\s*[A-Za-z_][\w.>-]*\s*[+)]'
    r'|(?:reinterpret|static)_cast<\s*(?:const\s+)?' + INT + r'\s*\*\s*>')
RES_EXPR = re.compile(r'getResource|getGlbResource|JKRGetResource|loadToMainRAM|->ptr\b|\bres\b|\bdata\b|\bbuf\w*\b|\bfile\b|\bheader\b|\bbmg\b')
STRUCT_CAST = re.compile(r'(?:static_cast<|reinterpret_cast<|\(\s*)(?:const\s+)?([A-Z]\w+)\s*\*\s*(?:>\s*\(|\))\s*([^;]*)')
BITFIELD = re.compile(r'^\s*(?:unsigned|signed|u8|s8|u16|s16|u32|s32|int|bool|BOOL)\s+\w*\s*:\s*\d+\s*;')
FIELD = re.compile(r'^\s*(?:/\*[^*]*\*/\s*)?(?:const\s+)?(' + INT + r')\s+(\w+)(\s*\[[^\]]*\])?\s*;')
SKIP_TYPES = {'JKRHeap', 'JKRArchive', 'JKRFileLoader', 'JKRSolidHeap', 'JKRExpHeap', 'J3DModel', 'JUTTexture',
              'J2DScreen', 'J2DPane', 'J2DPicture', 'J2DTextBox', 'GXTexObj', 'Mtx', 'JGeometry', 'JKRAramArchive',
              'JKRMemArchive', 'ActorBase_804A5770', 'JKRDvdFile'}


def port_branches(lines):
    """True per line when the line is compiled in the port (FSA_PORT=1, FSA_ENDIAN_DIAG unset)."""
    stack = []  # (active, port_cond_kind)
    out = []
    for line in lines:
        s = line.strip()
        m = re.match(r'#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)', s)
        if m:
            kind, rest = m.group(1), m.group(2)
            if kind in ('ifdef', 'ifndef', 'if'):
                val = None
                if 'FSA_PORT' in rest or '__MWERKS__' in rest:
                    neg = kind == 'ifndef' or '!' in rest
                    is_port = 'FSA_PORT' in rest
                    val = (is_port != neg) if 'FSA_PORT' in rest else neg  # __MWERKS__ is false
                stack.append([val, val])
            elif kind == 'elif':
                if stack:
                    stack[-1][0] = None if stack[-1][1] is None else False
            elif kind == 'else':
                if stack and stack[-1][1] is not None:
                    stack[-1][0] = not stack[-1][1]
            elif kind == 'endif':
                if stack:
                    stack.pop()
            out.append(False)
            continue
        out.append(all(v is not False for v, _ in stack))
    return out


def struct_defs(files):
    """name -> (file, line, [(type, field, lineno)], bitfields, is_union)"""
    defs = {}
    for path, lines in files.items():
        active = port_branches(lines)
        i = 0
        while i < len(lines):
            m = re.match(r'\s*(struct|union|class)\s+(\w+)\s*(?::[^{]*)?\{\s*$', lines[i])
            if m and active[i]:
                name, kind, start = m.group(2), m.group(1), i
                depth, fields, bits = 0, [], 0
                j = i
                while j < len(lines):
                    depth += lines[j].count('{') - lines[j].count('}')
                    if j > i and active[j]:
                        fm = FIELD.match(lines[j])
                        if fm and depth == 1 and '(' not in lines[j]:
                            fields.append((fm.group(1), fm.group(2), j + 1))
                        if BITFIELD.match(lines[j]):
                            bits += 1
                    if depth <= 0 and j > i:
                        break
                    j += 1
                defs.setdefault(name, (path, start + 1, fields, bits, kind == 'union'))
                i = j
            i += 1
    return defs


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    paths = args or ['game/src/game', 'game/include/game']
    files = {}
    for p in paths:
        p = os.path.join(ROOT, p)
        for dirpath, _, names in os.walk(p):
            for n in names:
                if n.endswith(('.cpp', '.h', '.c', '.inc')):
                    f = os.path.join(dirpath, n)
                    files[os.path.relpath(f, ROOT)] = open(f, errors='replace').read().split('\n')
    hits = []
    defs = struct_defs(files)
    overlay_types = {}
    for path, lines in files.items():
        active = port_branches(lines)
        for i, line in enumerate(lines):
            if not active[i] or line.strip().startswith('//'):
                continue
            if RAW_CAST.search(line) and '"' not in line.split('*')[0]:
                hits.append(dict(kind='raw-cast', file=path, line=i + 1, text=line.strip()))
            for m in STRUCT_CAST.finditer(line):
                t, rest = m.group(1), m.group(2)
                if t in SKIP_TYPES or t.startswith('J3D') or t.startswith('J2D'):
                    continue
                if RES_EXPR.search(rest):
                    overlay_types.setdefault(t, []).append(f'{path}:{i + 1}')
    for t, uses in sorted(overlay_types.items()):
        d = defs.get(t)
        if not d:
            continue
        plain = [f'{ty} {fn}' for ty, fn, _ in d[2] if not ty.startswith('BE')]
        if plain or d[3]:
            hits.append(dict(kind='overlay', file=d[0], line=d[1], text=f'{t}: plain {", ".join(plain[:8])}'
                             + (f'; {d[3]} bit fields' if d[3] else ''), uses=uses[:5]))
    for name, (path, line, fields, bits, is_union) in sorted(defs.items()):
        if bits:
            hits.append(dict(kind='bitfield', file=path, line=line, text=f'{name}: {bits} bit fields'))
        elif is_union and fields:
            hits.append(dict(kind='union', file=path, line=line,
                             text=f'{name}: ' + ', '.join(f'{t} {n}' for t, n, _ in fields[:6])))
    if '--json' in sys.argv:
        for h in hits:
            print(json.dumps(h))
    else:
        for h in hits:
            print(f"{h['kind']:<9} {h['file']}:{h['line']}  {h['text'][:150]}"
                  + (f"  <- {', '.join(h['uses'])}" if h.get('uses') else ''))


if __name__ == '__main__':
    main()
