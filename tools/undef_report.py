#!/usr/bin/env python3
"""What the compiled units need that none of them define: the work list for linking.
    python3 tools/undef_report.py [build/obj] > build/undefined.txt"""
import collections, os, re, subprocess, sys
NM = os.path.join(os.environ.get('EMSDK', '/bigdata/agent-tom/projects/fsadecomp/tools/emsdk'), 'upstream/bin/llvm-nm')
root = sys.argv[1] if len(sys.argv) > 1 else 'build/obj'
objs = [os.path.join(d, f) for d, _, fs in os.walk(root) for f in fs if f.endswith('.o')]
defined, strong, undef = set(), collections.Counter(), collections.defaultdict(set)
for o in objs:
    for line in subprocess.run([NM, o], capture_output=True, text=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] == 'U':
            undef[parts[1]].add(o)
        elif len(parts) == 3:
            defined.add(parts[2])
            if parts[1] in 'TDBR':
                strong[parts[2]] += 1
missing = {s: u for s, u in undef.items() if s not in defined}
dem = dict(zip(missing, subprocess.run([NM.replace('llvm-nm', 'llvm-cxxfilt')], input='\n'.join(missing), capture_output=True, text=True).stdout.splitlines()))
def cat(s, d):
    n = d.split('(')[0]
    if re.match(r'^(OS|__OS|GX|__GX|DVD|PAD|VI|SI|AR|ARQ|AI|DSP|CARD|EXI|GBA|PPC|C_MTX|PSMTX|PSVEC|PSQUAT|C_VEC|MTX|VEC|GD|THP|DB|LC|__DSP|__VI|__PAD|__CARD|__AR|__DVD)', n): return 'Dolphin SDK'
    if re.match(r'^(mem|str|sprintf|snprintf|vsnprintf|vsprintf|printf|puts|abs|atoi|strtol|strtoul|rand|srand|sin|cos|tan|atan|atan2|sqrt|pow|fmod|floor|ceil|exp|log|acos|asin|fabs|modf|frexp|ldexp|toupper|tolower|isdigit|isalpha|isspace|isupper|exit|abort|qsort|bsearch|wcs|__float_|__double_|__va_arg|__builtin)', n): return 'C library'
    if n.startswith(('__cxa', '__dso', 'operator ', '__cxx', '__construct', '__destroy', '__ptmf', '__dt', '__register_global')) or s.startswith(('_Znw', '_Zna', '_Zdl', '_Zda')): return 'C++ runtime'
    if re.search(r'(lbl|fn|func)_8[0-9A-F]{7}', n): return 'game: unnamed symbols with no definition'
    if n.startswith(('J', 'JAS', 'JAI', 'JAU', 'JKR', 'JUT', 'JSU', 'J2D', 'J3D', 'JPA', 'JMA', 'JMessage', 'JGadget', 'JMath', 'JGeometry', 'JFW', 'JStudio')): return 'JSystem'
    return 'other'
groups = collections.defaultdict(list)
for s, users in missing.items():
    groups[cat(s, dem[s])].append((len(users), dem[s]))
print(f'{len(objs)} objects, {len(defined)} symbols defined, {len(missing)} undefined')
for g, items in sorted(groups.items(), key=lambda kv: -len(kv[1])):
    print(f'\n## {g}: {len(items)}')
    for n, d in sorted(items, reverse=True):
        print(f'  {n:4} {d}')
