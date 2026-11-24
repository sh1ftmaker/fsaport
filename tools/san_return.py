#!/usr/bin/env python3
"""Missing returns, from the -Wreturn-type warnings of the FSA_SANITIZE=undefined build.

    ninja -C build/ubsan fsa 2>&1 | tee build/ubsan-build.log     (a full build)
    python3 tools/san_return.py build/ubsan-build.log > OUT.jsonl

The run-time check (-fsanitize=return) cannot be used: clang follows its
handler with `unreachable`, which traps, and the game does fall off the end of
value-returning functions (MWCC let it; the port builds with
-fno-strict-return). So these are reported statically, one JSON line per site
in the sanitizer record format (kind "return-static", count 0, the enclosing
function taken from the source)."""
import json, os, re, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
pat = re.compile(r'^(\S+?):(\d+):(\d+): warning: (non-void function does not return a value[^\[]*|'
                 r'non-void lambda does not return a value[^\[]*)\[-Wreturn-type\]')
seen = set()
for line in open(sys.argv[1], errors='replace'):
    m = pat.match(line.strip())
    if not m:
        continue
    path, ln, col, msg = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4).strip()
    i = path.find('/game/')
    rel = path[i + 1:] if i >= 0 else path
    if (rel, ln, col) in seen:
        continue
    seen.add((rel, ln, col))
    func = ''
    try:
        src = open(os.path.join(root, rel), errors='replace').read().split('\n')
        # The function header: the nearest line above at column 0 that opens a call.
        # (Units with #line directives give lines past the end: no name then.)
        for k in range(min(ln, len(src)) - 1 if ln <= len(src) else -1, -1, -1):
            t = src[k]
            if k == ln - 1 and col > 1 and '(' in t:  # a one-line body
                func = t.strip()
                break
            if t and not t[0].isspace() and '(' in t and not t.startswith(('#', '//', '}')):
                func = t.strip()
                break
    except OSError:
        pass
    print(json.dumps({'key': f'return-static|{rel}:{ln}:{col}', 'kind': 'return-static', 'file': rel, 'line': ln,
                      'col': col, 'function': func[:200], 'symbol': '', 'unit': rel, 'detail': msg, 'count': 0,
                      'first_scene': ''}))
