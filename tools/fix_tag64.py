#!/usr/bin/env python3
"""Rewrite multi-character literals longer than 4 chars ('NIN_Logo') as FSA_TAG64("NIN_Logo").

MWCC gives 5-8 character constants 64-bit values (J2D pane tags); clang keeps only
the last 4 bytes in an int. FSA_TAG64 (game/port/port_prefix.h) builds the u64.
Skips string literals and comments.   python3 tools/fix_tag64.py [--dry] FILES..."""
import re, sys
dry = '--dry' in sys.argv
total = 0
for path in [a for a in sys.argv[1:] if a != '--dry']:
    src = open(path, errors='surrogateescape').read()
    out, i, n, changed = [], 0, len(src), 0
    while i < n:
        c = src[i]
        if src.startswith('//', i):
            j = src.find('\n', i); j = n if j < 0 else j
            out.append(src[i:j]); i = j; continue
        if src.startswith('/*', i):
            j = src.find('*/', i + 2); j = n if j < 0 else j + 2
            out.append(src[i:j]); i = j; continue
        if c == '"':
            j = i + 1
            while j < n and src[j] != '"':
                j += 2 if src[j] == '\\' else 1
            out.append(src[i:j + 1]); i = j + 1; continue
        if c == "'":
            m = re.match(r"'([^'\\\n]{5,8})'", src[i:])
            if m and not (i > 0 and (src[i-1].isalnum() or src[i-1] == '_')):
                out.append(f'FSA_TAG64("{m.group(1)}")'); i += m.end(); changed += 1; continue
            m = re.match(r"'(\\.|[^'\\\n])*'", src[i:])
            if m:
                out.append(m.group(0)); i += m.end(); continue
        out.append(c); i += 1
    if changed:
        total += changed
        print(f'{path}: {changed}')
        if not dry:
            open(path, 'w', errors='surrogateescape').write(''.join(out))
print('total', total)
