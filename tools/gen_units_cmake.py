#!/usr/bin/env python3
"""game/units.txt -> build-time CMake: one static library per distinct flag set.
    python3 tools/gen_units_cmake.py game/units.txt OUT.cmake"""
import collections, sys
groups = collections.OrderedDict()
for line in open(sys.argv[1]):
    path, flags = line.rstrip('\n').split('\t')
    groups.setdefault(flags, []).append(path)
out = []
for i, (flags, paths) in enumerate(groups.items()):
    f = flags.split()
    incs = [x[2:] for x in f if x.startswith('-I')]
    defs = [x[2:] for x in f if x.startswith('-D')]
    lib = f'fsa_units_{i}'
    out.append(f'add_library({lib} STATIC')
    out += [f'  ${{FSA_GAME}}/{p}' for p in paths]
    out.append(')')
    inc = ' '.join('${FSA_ASSETS}' if x == '@ASSETS@' else f'${{FSA_GAME}}/{x}' for x in incs)
    out.append(f'target_include_directories({lib} PRIVATE {inc})')
    if defs:
        out.append(f'target_compile_definitions({lib} PRIVATE {" ".join(defs)})')
    out.append(f'target_link_libraries({lib} PRIVATE fsa_game_flags)')
    if '--language=c++' in f:
        cs = [p for p in paths if p.endswith('.c')]
        if cs:
            out.append('set_source_files_properties(' + ' '.join(f'${{FSA_GAME}}/{p}' for p in cs) + ' PROPERTIES LANGUAGE CXX)')
    out.append(f'list(APPEND FSA_UNIT_LIBS {lib})')
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
print(f'{len(groups)} groups')
