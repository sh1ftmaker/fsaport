#!/usr/bin/env python3
"""Turn the multi-byte scalar fields of named structs into BE<T> fields.

    python3 tools/be_structs.py HEADER STRUCT [STRUCT ...]

Only direct field declarations at the struct's own level are changed
(`u16 mFoo;`, `s32 mBar[4];`, `f32 x, y;`), not nested structs or methods.
Adds #include "port/be.h" if missing. Prints what it changed.
"""
import re
import sys

SCALARS = r'(u16|s16|u32|s32|f32|u64|s64|f64|unsigned short|short|unsigned int|int|unsigned long|long|float)'
FIELD = re.compile(r'^(\s*)' + SCALARS + r'(\s+)([A-Za-z_]\w*(?:\s*\[[^\]]*\])*(?:\s*,\s*[A-Za-z_]\w*(?:\s*\[[^\]]*\])*)*)\s*;')


def struct_body(s, name):
    m = re.search(r'\b(struct|class)\s+' + re.escape(name) + r'\b[^;{]*\{', s)
    if not m:
        raise SystemExit(f'struct {name} not found')
    i = m.end()
    depth = 1
    j = i
    while depth:
        c = s[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        j += 1
    return i, j - 1


def convert(s, name):
    i, j = struct_body(s, name)
    body = s[i:j]
    out = []
    depth = 0
    changed = 0
    for line in body.split('\n'):
        if depth == 0:
            m = FIELD.match(line)
            if m and '(' not in line.split(';')[0]:
                ind, typ, sp, names = m.group(1), m.group(2), m.group(3), m.group(4)
                if len(names.split(',')) > 1:
                    decl = '\n'.join(f'{ind}BE<{typ}> {n.strip()};' for n in names.split(','))
                    line = decl + line[m.end():]
                else:
                    line = f'{ind}BE<{typ}>{sp}{names};' + line[m.end():]
                changed += 1
        depth += line.count('{') - line.count('}')
        out.append(line)
    return s[:i] + '\n'.join(out) + s[j:], changed


def main():
    path, names = sys.argv[1], sys.argv[2:]
    s = open(path).read()
    for n in names:
        s, c = convert(s, n)
        print(f'{path}: {n}: {c} fields')
    if '"port/be.h"' not in s:
        m = re.search(r'#include[^\n]*\n', s)
        s = s[:m.start()] + '#include "port/be.h"\n' + s[m.start():]
    open(path, 'w').write(s)


if __name__ == '__main__':
    main()
