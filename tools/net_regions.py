#!/usr/bin/env python3
"""The static memory a rollback snapshot covers (docs/NETPLAY.md, "What a snapshot is").

Reads the linker map (fsa.map) and writes the .data/.bss address ranges that
belong to the simulation: every game and JSystem unit (libfsa_units_*.a), the
platform objects whose statics are game-visible state (virtual clock, harness,
GBA endpoints, DSP mixer, audio DMA, DVD), and Aurora's OS layer (arena, ARAM
allocator, time hook). Everything else (Aurora's GX/WebGPU state, libc,
libc++, SDL, the GBA view, the netplay layer itself, the thread layer, whose
state platform/os_thread.cpp saves field by field) is left out: it is either
the host's (restoring it would desynchronise it from the GPU or the browser)
or derived from the simulation each frame.

Globals that own heap memory (std containers) must not be restored by bytes:
their headers would point at heap blocks that moved. They are excluded by
symbol (EXCLUDE_SYMBOLS); the ones that matter are empty at every pad read
(the DVD request queue, os_misc's deferred callbacks) and the rest never
change after boot (the FST).

    tools/net_regions.py build/web/fsa.map build/web/fsa.netregions.json
"""
import json
import re
import sys

INCLUDE_LIBS = re.compile(r'^libfsa_units_\d+\.a$')
INCLUDE_OBJS = {
    'libfsa_platform.a': {'harness.cpp.o', 'gba_link.cpp.o', 'audio.cpp.o', 'audio_dsp.cpp.o', 'dvd.cpp.o',
                          'vi.cpp.o', 'os_misc.cpp.o', 'hw_stubs.cpp.o', 'mtx_ps.cpp.o'},
    'libaurora_os.a': None,  # all of it
    'libaurora_si.a': None,
    'libaurora_dvd.a': None,
}
# Single symbols of other objects: Aurora's texture/TLUT object id counters
# (GXTexture.cpp; ids are handed out again after a rollback, see there).
INCLUDE_SYMBOLS = [('GXTexture.cpp.o', 'sNextTexObjId'), ('GXTexture.cpp.o', 'sNextTlutObjId')]
# (object, symbol) pairs never restored by bytes: heap-owning containers;
# harness.cpp's 256 KiB JSON scratch buffer (g_state; not state); host state
# that only looks like the game's (the host-allocation depth, Aurora's
# in-frame flag); wall-clock timings and log counters.
EXCLUDE_SYMBOLS = [('os_misc.cpp.o', 'g_pending'), ('os_misc.cpp.o', 'fsa_host_alloc_depth'),
                   ('dvd.cpp.o', 'g_fst'), ('dvd.cpp.o', 'g_names'), ('dvd.cpp.o', 'g_requests'),
                   ('harness.cpp.o', 'g_state'),
                   ('vi.cpp.o', 'g_inFrame'), ('vi.cpp.o', 'g_frameStart'), ('vi.cpp.o', 'g_frameMsSum'),
                   ('vi.cpp.o', 'g_endMsSum'), ('vi.cpp.o', 'g_beginMsSum'), ('vi.cpp.o', 'g_framesBegun'),
                   ('vi.cpp.o', 'g_framesDrawn'), ('vi.cpp.o', 'lastDrawn'),
                   ('audio_dsp.cpp.o', 'g_renderMs'), ('audio_dsp.cpp.o', 'g_logStarts'),
                   ('audio_dsp.cpp.o', 'g_framesRendered'), ('audio_dsp.cpp.o', 'g_voicesPlayed')]


def main():
    src, out = sys.argv[1], sys.argv[2]
    sec = None
    ranges = []
    holes = []  # excluded sections: never merged over
    objs = {}
    for line in open(src):
        p = line.split()
        if len(p) == 4 and re.fullmatch(r'\.[a-z_]+', p[3]):  # an output section (not a .L symbol)
            sec = p[3]
            continue
        if sec not in ('.data', '.bss') or len(p) < 4:
            continue
        m = re.match(r'(.*?):\((\.[^)]*)\)', p[3])
        if not m:
            continue
        path, section = m.group(1), m.group(2)
        base = path.split('/')[-1]
        mm = re.match(r'(.*?)\((.*)\)$', base)
        lib, obj = (mm.group(1), mm.group(2)) if mm else (base, base)
        ok = False
        if INCLUDE_LIBS.match(lib):
            ok = True
        elif lib in INCLUDE_OBJS:
            ok = INCLUDE_OBJS[lib] is None or obj in INCLUDE_OBJS[lib]
        if not ok and any(o == obj and re.search(r'(\.|\d)' + sym + r'(E|$)', section) for o, sym in INCLUDE_SYMBOLS):
            ok = True
        if ok and any(o == obj and re.search(r'(\.|\d)' + sym + r'(E|$)', section) for o, sym in EXCLUDE_SYMBOLS):
            ok = False
        addr, size = int(p[0], 16), int(p[2], 16)
        if not ok:
            holes.append((addr, addr + size))
            continue
        if size == 0:
            continue
        ranges.append((addr, addr + size))
        key = lib if INCLUDE_LIBS.match(lib) else lib + '(' + obj + ')'
        objs[key] = objs.get(key, 0) + size
    ranges.sort()
    merged = []
    for a, b in ranges:
        # Padding between neighbouring sections (alignment, < 64 bytes) is merged in.
        if merged and a <= merged[-1][1] + 64 and not any(h0 < a and h1 > merged[-1][1] for h0, h1 in holes):
            merged[-1][1] = max(merged[-1][1], b)
        else:
            merged.append([a, b])
    total = sum(b - a for a, b in merged)
    with open(out, 'w') as f:
        json.dump({'version': 1, 'bytes': total, 'ranges': merged, 'objects': objs}, f, indent=0)
    print(f'net_regions: {len(merged)} ranges, {total} bytes -> {out}')


if __name__ == '__main__':
    main()
