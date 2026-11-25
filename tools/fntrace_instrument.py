#!/usr/bin/env python3
"""Rewrite fsa.wasm for function traces (FSA_DIAG builds; docs/DIAGNOSTICS.md,
"Function traces (fntrace)").

  tools/fntrace_instrument.py build/diag/fsa.wasm build/diag/fsa.map

Runs after tools/vdispatch_instrument.py (POST_BUILD). Rewrites fsa.wasm in
place (keeps its input as fsa.wasm.noft) and writes next to it:

  fsa.diag.ft.tsv    id \\t this \\t params \\t result \\t declared  (one line per
                     rewritten function; `this` is the wasm parameter index of
                     `this` or -1, params/result are wasm types (i,I,f,d = i32,
                     i64, f32, f64), declared the Itanium parameter types)
  fsa.diag.data.tsv  addr \\t size \\t name   (data symbols from fsa.map)

Every instrumented function (one whose body calls __cyg_profile_func_enter with
its own table index `id`) becomes

    if (i32.load8_u (SEL + id))          ; one byte per id, 0 = not selected
      store each parameter to ARGS + 8*k
      tok = fsa_ft_enter(id, sig)        ; sig: packed signature (below)
    block (result T)
      <original body, every `return` turned into a `br` to this block>
    end
    [ret = ...]
    if (tok)
      [store ret to RET]
      fsa_ft_exit(tok)
    [ret]

so with nothing selected each call costs one byte load and branch on entry and
a local test on exit. platform/diag_fntrace.cpp does the rest.

sig (i64): bits 0-2 result kind (0 none, 1 i32, 2 i64, 3 f32, 4 f64); bits
3-4 `this` (0 none, 1 parameter 0, 2 parameter 1 after an sret pointer); bits
5-9 the number of parameters stored (at most 27); then 2 bits per parameter
(0 i32, 1 i64, 2 f32, 3 f64).
"""
import argparse, os, re, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import uninit_instrument as ui  # noqa: E402
import diag_symtab as ds  # noqa: E402
from vdispatch_instrument import const_body, parse_elem  # noqa: E402

MARKER = 'fsa_fntrace_instrumented'
VT = {0x7F: 0, 0x7E: 1, 0x7D: 2, 0x7C: 3}
VCH = {0x7F: 'i', 0x7E: 'I', 0x7D: 'f', 0x7C: 'd'}
STORE = {0x7F: (0x36, 2), 0x7E: (0x37, 3), 0x7D: (0x38, 2), 0x7C: (0x39, 3)}
MAX_PARAMS = 27


def declared(sym):
    """(is_member, number of declared parameters, readable types) or None."""
    try:
        comps, quals, params, _ = ds.Itanium(sym).function()
    except Exception:
        return None
    if params == [('b', 'v')]:
        params = []
    try:
        txt = ','.join(ds.mw_type(p) for p in params)
    except Exception:
        txt = '?'
    return len(comps) > 1, len(params), txt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('wasm')
    ap.add_argument('map')
    a = ap.parse_args()
    orig = a.wasm + '.noft'
    src = open(a.wasm, 'rb').read()
    if MARKER.encode() in src and os.path.exists(orig):
        src = open(orig, 'rb').read()
    else:
        shutil.copyfile(a.wasm, orig)
    secs = ui.read_sections(src)
    types = funcs = exports = names = code = elem = None
    nimp = 0
    for s in secs:
        if s[0] == 1: types = ui.parse_types(s[1])
        elif s[0] == 2: nimp = ui.parse_imports(s[1])
        elif s[0] == 3: funcs = ui.parse_funcs(s[1])
        elif s[0] == 7: exports = ui.parse_exports(s[1])
        elif s[0] == 9: elem = parse_elem(s[1])
        elif s[0] == 10: code = s
        elif s[0] == 0 and ui.custom_name(s[1])[0] == 'name': names = ui.parse_names(s[1])
    if names is None:
        raise SystemExit('no name section')
    for n in ('fsa_ft_enter', 'fsa_ft_exit', 'fsa_ft_sel_ptr', 'fsa_ft_args_ptr', 'fsa_ft_ret_ptr', 'fsa_ft_sel_size'):
        if n not in exports:
            raise SystemExit(f'{n} not exported (platform/diag_fntrace.cpp not linked?)')
    f_enter_ft, f_exit_ft = exports['fsa_ft_enter'], exports['fsa_ft_exit']
    if types[funcs[f_enter_ft - nimp]] != ([0x7F, 0x7E], [0x7F]):
        raise SystemExit(f'fsa_ft_enter has type {types[funcs[f_enter_ft - nimp]]}, expected (i32, i64) -> i32')
    if types[funcs[f_exit_ft - nimp]] != ([0x7F], []):
        raise SystemExit('fsa_ft_exit must be (i32) -> void')
    f_cyg = next((i for i, n in names.items() if n == '__cyg_profile_func_enter'), None)
    if f_cyg is None:
        raise SystemExit('__cyg_profile_func_enter not found')

    body = code[1]
    cnt, i = ui.uleb(body, 0)
    bodies = []
    for _ in range(cnt):
        n, j = ui.uleb(body, i)
        bodies.append((j, j + n))
        i = j + n
    cst = lambda name: const_body(body, bodies[exports[name] - nimp][0])
    sel, args, ret, sel_size = cst('fsa_ft_sel_ptr'), cst('fsa_ft_args_ptr'), cst('fsa_ft_ret_ptr'), cst('fsa_ft_sel_size')
    i32c = lambda v: b'\x41' + ui.enc_s(v - (1 << 32) if v >= 1 << 31 else v)

    tidx_of = {}
    for t, f in elem.items():
        tidx_of.setdefault(f, set()).add(t)

    st = dict(funcs=0, this=0, skipped_types=0, returns=0, too_many_params=0, big_id=0)
    out_bodies, ft_lines = [], []
    for fi, (b0, b1) in enumerate(bodies):
        idx = nimp + fi
        selfs = tidx_of.get(idx, ())
        nloc, k = ui.uleb(body, b0)
        locals_ = []
        for _ in range(nloc):
            c, k = ui.uleb(body, k)
            locals_.append((c, body[k]))
            k += 1
        code0 = k
        enters, ctrl = [], []

        def on_call(p, op, t, j):
            if t == f_cyg and op == 0x10:
                enters.append(p)
        end = ui.scan_body(body, code0, b1, lambda *x: None, on_call, None, lambda p, op: ctrl.append((p, op)))
        assert end == b1
        own = None
        if enters:
            p = enters[0]
            for t in selfs:
                c = b'\x41' + ui.enc_s(t)
                if body[p - len(c):p] == c:
                    own = t
                    break
        params, results = types[funcs[fi]]
        if own is None:
            out_bodies.append(body[b0:b1])
            continue
        if any(t not in VT for t in params) or len(results) > 1 or any(t not in VT for t in results) \
                or any(op in (0x12, 0x13) for _, op in ctrl):
            st['skipped_types'] += 1
            out_bodies.append(body[b0:b1])
            continue
        if own >= sel_size:
            st['big_id'] += 1
            out_bodies.append(body[b0:b1])
            continue
        st['funcs'] += 1
        # `this`: a member function whose wasm parameters are its declared ones
        # plus one (or plus two: sret first, then this).
        dec = declared(names.get(idx, ''))
        this = -1
        if dec and dec[0] and params and params[0] == 0x7F:
            if len(params) == dec[1] + 1:
                this = 0
            elif len(params) == dec[1] + 2 and len(params) > 1 and params[1] == 0x7F:
                this = 1
        if this >= 0:
            st['this'] += 1
        np_ = min(len(params), MAX_PARAMS)
        st['too_many_params'] += len(params) > MAX_PARAMS
        rk = {None: 0, 0x7F: 1, 0x7E: 2, 0x7D: 3, 0x7C: 4}[results[0] if results else None]
        sig = rk | ((this + 1) << 3) | (np_ << 5)
        for k2 in range(np_):
            sig |= VT[params[k2]] << (10 + 2 * k2)
        if sig >= 1 << 63:
            sig -= 1 << 64
        ft_lines.append(f'{own}\t{this}\t{"".join(VCH[t] for t in params)}\t{"".join(VCH[t] for t in results)}\t'
                        f'{dec[2] if dec else ""}\n')

        nl = len(params) + sum(c for c, _ in locals_)
        tok = nl
        retl = nl + 1
        new_locals = locals_ + [(1, 0x7F)] + ([(1, results[0])] if results else [])

        pro = bytearray()
        pro += b'\x41\x00\x2D\x00' + ui.enc_u(sel + own)  # i32.load8_u offset=sel+id
        pro += b'\x04\x40'
        for k2 in range(np_):
            op, al = STORE[params[k2]]
            pro += i32c(args) + b'\x20' + ui.enc_u(k2) + bytes([op, al]) + ui.enc_u(8 * k2)
        pro += i32c(own) + b'\x42' + ui.enc_s(sig) + b'\x10' + ui.enc_u(f_enter_ft) + b'\x21' + ui.enc_u(tok)
        pro += b'\x0B'
        pro += b'\x02' + (bytes([results[0]]) if results else b'\x40')

        # The body: returns -> br <depth>; drop the final `end`.
        nb = bytearray()
        pos, depth = code0, 0
        last_end = None
        for p, op in ctrl:
            if op in (0x02, 0x03, 0x04, 0x06, 0x1F):
                depth += 1
            elif op in (0x0B, 0x18):
                if depth == 0:
                    last_end = p
                else:
                    depth -= 1
            elif op == 0x0F:
                nb += body[pos:p] + b'\x0C' + ui.enc_u(depth)
                pos = p + 1
                st['returns'] += 1
        assert last_end == b1 - 1, (names.get(idx), last_end, b1)
        nb += body[pos:b1 - 1]

        epi = bytearray(b'\x0B')  # end of the wrapper block
        if results:
            epi += b'\x21' + ui.enc_u(retl)
        epi += b'\x20' + ui.enc_u(tok) + b'\x04\x40'
        if results:
            op, al = STORE[results[0]]
            epi += i32c(ret) + b'\x20' + ui.enc_u(retl) + bytes([op, al]) + b'\x00'
        epi += b'\x20' + ui.enc_u(tok) + b'\x10' + ui.enc_u(f_exit_ft) + b'\x0B'
        if results:
            epi += b'\x20' + ui.enc_u(retl)
        epi += b'\x0B'

        hdr = ui.enc_u(len(new_locals)) + b''.join(ui.enc_u(c) + bytes([t]) for c, t in new_locals)
        out_bodies.append(bytes(hdr + pro + nb + epi))

    out = bytearray(src[:8])
    for sid, sb in secs:
        if sid == 10:
            sb = ui.enc_u(len(out_bodies)) + b''.join(ui.enc_u(len(x)) + x for x in out_bodies)
        out += bytes([sid]) + ui.enc_u(len(sb)) + sb
    cs = ui.enc_name(MARKER) + b'1'
    out += b'\x00' + ui.enc_u(len(cs)) + cs
    open(a.wasm, 'wb').write(out)

    d = os.path.dirname(a.wasm)
    with open(os.path.join(d, 'fsa.diag.ft.tsv'), 'w') as f:
        f.writelines(sorted(ft_lines, key=lambda l: int(l.split('\t')[0])))
    # Data symbols: "addr off size <spaces> name" lines (functions have "-" as addr).
    pat = re.compile(r'^\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+(\S+)$')
    data = {}
    for line in open(a.map):
        m = pat.match(line)
        if m and not m.group(3).startswith(('<', '.')) and ':(' not in m.group(3):
            data.setdefault(m.group(3), (int(m.group(1), 16), int(m.group(2), 16)))
    with open(os.path.join(d, 'fsa.diag.data.tsv'), 'w') as f:
        for name, (addr, size) in sorted(data.items(), key=lambda x: x[1]):
            f.write(f'{addr}\t{size}\t{name}\n')
    print(f'fntrace_instrument: {st["funcs"]} functions ({st["this"]} with `this`, {st["returns"]} returns rewritten, '
          f'{st["skipped_types"]} skipped for their types, {st["too_many_params"]} with >{MAX_PARAMS} params, '
          f'{st["big_id"]} ids >= {sel_size}); {len(data)} data symbols; sel {sel:#x} args {args:#x} ret {ret:#x}')


if __name__ == '__main__':
    main()
