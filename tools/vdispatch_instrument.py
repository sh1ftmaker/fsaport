#!/usr/bin/env python3
"""Rewrite fsa.wasm for the virtual dispatch audit (FSA_DIAG builds;
docs/DIAGNOSTICS.md, "Virtual dispatch audit").

  tools/vdispatch_instrument.py build/diag/fsa.wasm build/diag/fsa.map [--llvm-bin DIR]

Rewrites fsa.wasm in place (keeps the original as fsa.wasm.novd) and writes
fsa.diag.vt.tsv next to it. Only instrumented functions are touched: those whose
body calls __cyg_profile_func_enter with the function's own table index (the
game, JSystem and the platform layer; platform/diag_*.cpp are NOINST).

- Before each `call_indirect`: `call $fsa_vd_mark` (takes the table index on top
  of the stack and returns it), after it a store of 0 to the pending word
  (fsa_vd_pending_ptr()). A `return_call_indirect` gets the mark only.
- When the function's first parameter is an i32: just before its own enter call,
  a store of that parameter to the `this` word (fsa_vd_this_ptr()).

platform/diag_vdispatch.cpp pairs the two in its enter hook.

fsa.diag.vt.tsv, one line per vtable group (`_ZTV*` in fsa.map), sorted by address:
  addr \\t size \\t class (demangled) \\t mwcc __vt__ symbol \\t _ZTV symbol \\t words
where words are the group's 32-bit words, comma-separated (function table indices
for the function slots; the runtime only reads the first four fields).
"""
import argparse, os, re, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import uninit_instrument as ui  # noqa: E402  (LEB helpers, section parsers, instruction scanner)
import diag_symtab as ds  # noqa: E402  (Itanium -> CodeWarrior mangling)

MARKER = 'fsa_vdispatch_instrumented'


def parse_elem(body):
    """table index -> function index (active segments, function-index form)."""
    table = {}
    n, j = ui.uleb(body, 0)
    for _ in range(n):
        fl, j = ui.uleb(body, j)
        if fl not in (0, 2):
            raise SystemExit(f'element segment flags {fl}')
        if fl == 2:
            _, j = ui.uleb(body, j)
        assert body[j] == 0x41
        off, j = ui.sleb(body, j + 1)
        assert body[j] == 0x0B
        j += 1
        if fl == 2:
            j += 1
        cnt, j = ui.uleb(body, j)
        for k in range(cnt):
            f, j = ui.uleb(body, j)
            table[off + k] = f
    return table


def parse_data(body):
    """[(address, bytes)] of the active data segments."""
    segs = []
    n, j = ui.uleb(body, 0)
    for _ in range(n):
        fl, j = ui.uleb(body, j)
        if fl == 1:  # passive
            l, j = ui.uleb(body, j)
            j += l
            continue
        if fl == 2:
            _, j = ui.uleb(body, j)
        assert body[j] == 0x41, 'data offset is not a constant'
        off, j = ui.sleb(body, j + 1)
        assert body[j] == 0x0B
        j += 1
        l, j = ui.uleb(body, j)
        segs.append((off & 0xFFFFFFFF, body[j:j + l]))
        j += l
    return sorted(segs)


def read_words(segs, addr, size):
    """Little-endian words from the data segments, byte by byte: wasm-opt's
    memory packing drops zero runs, so a segment can start inside a word."""
    def byte(a):
        for base, b in segs:
            if base <= a < base + len(b):
                return b[a - base]
        return 0
    return [sum(byte(a + k) << (8 * k) for k in range(4)) for a in range(addr, addr + size, 4)]


def const_body(body, s0):
    """The constant an `i32.const N; end` function returns."""
    nloc, k = ui.uleb(body, s0)
    for _ in range(nloc):
        _, k = ui.uleb(body, k)
        k += 1
    if body[k] != 0x41:
        raise SystemExit('pointer accessor is not a constant')
    v, _ = ui.sleb(body, k + 1)
    return v & 0xFFFFFFFF


def mw_vt(itanium_vt):
    """_ZTV<type> -> __vt__<CodeWarrior class name>, or '' if it does not parse."""
    try:
        t = ds.Itanium(itanium_vt[4:]).type()
        if t[0] != 'cls':
            return ''
        return '__vt__' + ds.mw_qual(t[1])
    except Exception:
        return ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('wasm')
    ap.add_argument('map')
    ap.add_argument('--llvm-bin', default='')
    ap.add_argument('--vt-out')
    a = ap.parse_args()
    tool = lambda n: os.path.join(a.llvm_bin, n) if a.llvm_bin and os.path.exists(os.path.join(a.llvm_bin, n)) else n

    orig = a.wasm + '.novd'
    src = open(a.wasm, 'rb').read()
    if MARKER.encode() in src and os.path.exists(orig):
        src = open(orig, 'rb').read()  # rerun on an instrumented module
    else:
        shutil.copyfile(a.wasm, orig)
    secs = ui.read_sections(src)
    types = funcs = exports = names = code = elem = data = None
    nimp = 0
    for s in secs:
        if s[0] == 1: types = ui.parse_types(s[1])
        elif s[0] == 2: nimp = ui.parse_imports(s[1])
        elif s[0] == 3: funcs = ui.parse_funcs(s[1])
        elif s[0] == 7: exports = ui.parse_exports(s[1])
        elif s[0] == 9: elem = parse_elem(s[1])
        elif s[0] == 10: code = s
        elif s[0] == 11: data = parse_data(s[1])
        elif s[0] == 0 and ui.custom_name(s[1])[0] == 'name': names = ui.parse_names(s[1])
    if names is None:
        raise SystemExit('no name section (link with --profiling-funcs)')
    for n in ('fsa_vd_mark', 'fsa_vd_this_ptr', 'fsa_vd_pending_ptr'):
        if n not in exports:
            raise SystemExit(f'{n} not exported (platform/diag_vdispatch.cpp not linked?)')
    f_mark = exports['fsa_vd_mark']
    f_enter = next((i for i, n in names.items() if n == '__cyg_profile_func_enter'), None)
    if f_enter is None:
        raise SystemExit('__cyg_profile_func_enter not found (not an FSA_DIAG build?)')
    mt = types[funcs[f_mark - nimp]]
    if mt != ([0x7F], [0x7F]):
        raise SystemExit(f'fsa_vd_mark has type {mt}, expected (i32) -> i32')

    body = code[1]
    cnt, i = ui.uleb(body, 0)
    bodies = []
    for _ in range(cnt):
        n, j = ui.uleb(body, i)
        bodies.append((j, j + n))
        i = j + n
    this_addr = const_body(body, bodies[exports['fsa_vd_this_ptr'] - nimp][0])
    pend_addr = const_body(body, bodies[exports['fsa_vd_pending_ptr'] - nimp][0])

    tidx_of = {}
    for t, f in elem.items():
        tidx_of.setdefault(f, set()).add(t)

    store_this = b'\x41' + ui.enc_s(this_addr - (1 << 32) if this_addr >= 1 << 31 else this_addr) + b'\x20\x00\x36\x02\x00'
    clear_pend = b'\x41' + ui.enc_s(pend_addr - (1 << 32) if pend_addr >= 1 << 31 else pend_addr) + b'\x41\x00\x36\x02\x00'
    mark = b'\x10' + ui.enc_u(f_mark)
    st = dict(funcs=0, this=0, no_i32_arg=0, indirect=0, return_indirect=0, no_self=0)
    out_bodies = []
    for fi, (b0, b1) in enumerate(bodies):
        idx = nimp + fi
        selfs = tidx_of.get(idx, ())
        nloc, k = ui.uleb(body, b0)
        for _ in range(nloc):
            _, k = ui.uleb(body, k)
            k += 1
        code0 = k
        enters, indirect = [], []

        def on_call(p, op, t, j):
            if t == f_enter and op == 0x10:
                enters.append(p)

        def on_ind(p, op, j):
            indirect.append((p, op, j))

        end = ui.scan_body(body, code0, b1, lambda *x: None, on_call, on_ind)
        assert end == b1, (names.get(idx), end, b1)
        # The function's own entry: the first enter call, with its own table index.
        own = None
        if enters:
            p = enters[0]
            for t in selfs:
                c = b'\x41' + ui.enc_s(t)
                if body[p - len(c):p] == c:
                    own = p - len(c)
                    break
        if own is None:
            if enters and selfs:
                st['no_self'] += 1
            out_bodies.append(body[b0:b1])
            continue
        st['funcs'] += 1
        edits = []  # (pos, delete_len, insert)
        params = types[funcs[fi]][0]
        if params and params[0] == 0x7F:
            edits.append((own, 0, store_this))
            st['this'] += 1
        else:
            st['no_i32_arg'] += 1
        for p, op, j in indirect:
            edits.append((p, 0, mark))
            if op == 0x11:
                edits.append((j, 0, clear_pend))
                st['indirect'] += 1
            else:
                st['return_indirect'] += 1
        edits.sort(key=lambda e: e[0])
        nb = bytearray(body[b0:code0])
        pos = code0
        for p, dl, ins in edits:
            nb += body[pos:p]
            nb += ins
            pos = p + dl
        nb += body[pos:b1]
        out_bodies.append(bytes(nb))

    out = bytearray(src[:8])
    for sid, sb in secs:
        if sid == 10:
            sb = ui.enc_u(len(out_bodies)) + b''.join(ui.enc_u(len(x)) + x for x in out_bodies)
        out += bytes([sid]) + ui.enc_u(len(sb)) + sb
    cs = ui.enc_name(MARKER) + b'1'
    out += b'\x00' + ui.enc_u(len(cs)) + cs
    open(a.wasm, 'wb').write(out)

    # Vtables.
    vts = []
    pat = re.compile(r'^\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+(_ZTV\S+)$')
    for line in open(a.map):
        m = pat.match(line)
        if m:
            vts.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
    vts = sorted(set(vts))
    dem = subprocess.run([tool('llvm-cxxfilt')], input='\n'.join(v[2] for v in vts) + '\n',
                         capture_output=True, text=True).stdout.split('\n')
    vt_out = a.vt_out or os.path.join(os.path.dirname(a.wasm), 'fsa.diag.vt.tsv')
    with open(vt_out, 'w') as f:
        for (addr, size, sym), d in zip(vts, dem):
            cls = d[len('vtable for '):] if d.startswith('vtable for ') else d
            words = read_words(data, addr, size)
            cls = cls.replace('\\', '\\\\').replace('"', '\\"')
            f.write(f'{addr}\t{size}\t{cls}\t{mw_vt(sym)}\t{sym}\t{",".join(map(str, words))}\n')
    print(f'vdispatch_instrument: {st["funcs"]} functions ({st["this"]} pass `this`, {st["no_i32_arg"]} without an i32 '
          f'first parameter), {st["indirect"]} call_indirect + {st["return_indirect"]} return_call_indirect marked, '
          f'{st["no_self"]} enter calls without the own index; {len(vts)} vtables -> {os.path.basename(vt_out)}; '
          f'this word {this_addr:#x}, pending word {pend_addr:#x}')


if __name__ == '__main__':
    main()
