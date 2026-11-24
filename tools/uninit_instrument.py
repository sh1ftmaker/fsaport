#!/usr/bin/env python3
"""Instrument fsa.wasm for the uninitialised-read diagnostics (FSA_DIAG_UNINIT).

  tools/uninit_instrument.py build/web-uninit/fsa.wasm build/web-uninit/fsa.map \
      [--symbols ../fsadecomp/fsa-decomp/config/G4SE01/symbols.txt]

Rewrites fsa.wasm in place (keeps the original as fsa.wasm.orig) and writes
fsa.uninit.json next to it. Only functions linked from the game's unit
libraries (libfsa_units_*.a in the linker map) are touched:

- every i32/i64/f32 load and i32.load16 gets an inline check of the loaded
  value against the poison pattern (0xA5 bytes, see platform/diag_uninit.cpp);
  a hit calls fsa_uninit_hit(addr, offset, site, size) with site = the index
  of the function doing the read;
- every constructor (Itanium C1/C2) gets a wrapper that calls
  fsa_uninit_ctor_enter/exit(this, ctor index) around
  it, and all direct calls to it go through the wrapper;
- every call to operator new / JKRHeap::alloc first stores the caller's
  function index at fsa_uninit_site_ptr(), so the runtime knows who allocated.

fsa.uninit.json maps function indices to names for the page (fsa_uninit.js):
{"funcs": {"<index>": [itanium, demangled, mwcc|null, orig_addr|null]}}.
"""
import argparse, json, os, re, shutil, struct, sys

POISON32 = 0xA5A5A5A5
POISON16U = 0xA5A5
POISON16S = 0xFFFFA5A5 - (1 << 32)

# ---------------------------------------------------------------- LEB helpers
def uleb(d, i):
    r = s = 0
    while True:
        b = d[i]; i += 1
        r |= (b & 0x7F) << s; s += 7
        if b < 0x80:
            return r, i

def sleb(d, i):
    r = s = 0
    while True:
        b = d[i]; i += 1
        r |= (b & 0x7F) << s; s += 7
        if b < 0x80:
            if b & 0x40:
                r -= 1 << s
            return r, i

def skip_leb(d, i):
    while d[i] & 0x80:
        i += 1
    return i + 1

def enc_u(v):
    out = bytearray()
    while True:
        b = v & 0x7F; v >>= 7
        if v:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)

def enc_s(v):
    out = bytearray()
    while True:
        b = v & 0x7F; v >>= 7
        if (v == 0 and not b & 0x40) or (v == -1 and b & 0x40):
            out.append(b)
            return bytes(out)
        out.append(b | 0x80)

def enc_name(s):
    b = s.encode()
    return enc_u(len(b)) + b

# ---------------------------------------------------------------- module
def read_sections(d):
    assert d[:4] == b'\0asm'
    secs = []
    i = 8
    while i < len(d):
        sid = d[i]
        n, j = uleb(d, i + 1)
        secs.append([sid, d[j:j + n]])
        i = j + n
    return secs

def custom_name(body):
    n, i = uleb(body, 0)
    return body[i:i + n].decode(), i + n

def parse_types(body):
    n, i = uleb(body, 0)
    types = []
    for _ in range(n):
        assert body[i] == 0x60, 'only func types'
        i += 1
        np_, i = uleb(body, i)
        params = list(body[i:i + np_]); i += np_
        nr, i = uleb(body, i)
        res = list(body[i:i + nr]); i += nr
        types.append((params, res))
    return types

def parse_imports(body):
    n, i = uleb(body, 0)
    nfunc = 0
    for _ in range(n):
        l, i = uleb(body, i); i += l
        l, i = uleb(body, i); i += l
        kind = body[i]; i += 1
        if kind == 0:
            _, i = uleb(body, i); nfunc += 1
        elif kind == 1:
            i += 1
            fl = body[i]; i += 1
            _, i = uleb(body, i)
            if fl & 1:
                _, i = uleb(body, i)
        elif kind == 2:
            fl = body[i]; i += 1
            _, i = uleb(body, i)
            if fl & 1:
                _, i = uleb(body, i)
        elif kind == 3:
            i += 2
        elif kind == 4:
            i += 1
            _, i = uleb(body, i)
        else:
            raise SystemExit(f'import kind {kind}')
    return nfunc

def parse_funcs(body):
    n, i = uleb(body, 0)
    out = []
    for _ in range(n):
        t, i = uleb(body, i)
        out.append(t)
    return out

def parse_exports(body):
    n, i = uleb(body, 0)
    out = {}
    for _ in range(n):
        l, i = uleb(body, i)
        name = body[i:i + l].decode(); i += l
        kind = body[i]; i += 1
        idx, i = uleb(body, i)
        if kind == 0:
            out[name] = idx
    return out

def parse_names(body):
    _, i = custom_name(body)
    names = {}
    while i < len(body):
        sub = body[i]
        n, j = uleb(body, i + 1)
        if sub == 1:
            cnt, k = uleb(body, j)
            for _ in range(cnt):
                idx, k = uleb(body, k)
                l, k = uleb(body, k)
                names[idx] = body[k:k + l].decode('utf-8', 'replace'); k += l
        i = j + n
    return names

def rebuild_names(body, extra):
    """Append (index, name) pairs to the function-names subsection."""
    _, i = custom_name(body)
    out = bytearray(body[:i])
    while i < len(body):
        sub = body[i]
        n, j = uleb(body, i + 1)
        sb = body[j:j + n]
        if sub == 1 and extra:
            cnt, k = uleb(sb, 0)
            nb = enc_u(cnt + len(extra)) + sb[k:]
            for idx, nm in extra:
                nb += enc_u(idx) + enc_name(nm)
            sb = nb
        out += bytes([sub]) + enc_u(len(sb)) + sb
        i = j + n
    return bytes(out)

# ---------------------------------------------------------------- instructions
SINGLE_BT = {0x40, 0x7F, 0x7E, 0x7D, 0x7C, 0x7B, 0x70, 0x6F}

def skip_blocktype(d, i):
    if d[i] in SINGLE_BT:
        return i + 1
    if d[i] in (0x63, 0x64):  # (ref null ht)
        return skip_leb(d, i + 1)
    return skip_leb(d, i)

def skip_memarg(d, i):
    a, i = uleb(d, i)
    if a & 0x40:
        i = skip_leb(d, i)
    return skip_leb(d, i)

LOADS = {0x28: 'i32', 0x29: 'i64', 0x2A: 'f32', 0x2E: 's16', 0x2F: 'u16'}

def scan_body(d, i, end, on_load, on_call, on_indirect=None):
    """Walk one function body's instructions. on_load(pos, op, memarg_end,
    align, offset); on_call(pos, op, target, after); on_indirect(pos, op,
    after) for call_indirect / return_call_indirect (also used by
    tools/vdispatch_instrument.py)."""
    while i < end:
        op = d[i]; p = i; i += 1
        if op <= 0x01 or op in (0x05, 0x0B, 0x0F, 0x19, 0x0A, 0xD1, 0xD3, 0xD4, 0x1A, 0x1B) or 0x45 <= op <= 0xC4:
            continue
        if op in (0x02, 0x03, 0x04, 0x06):
            i = skip_blocktype(d, i)
        elif op in (0x0C, 0x0D, 0x07, 0x08, 0x09, 0x18, 0xD5, 0xD6, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0xD2, 0x14, 0x15):
            i = skip_leb(d, i)
        elif op == 0x0E:
            n, i = uleb(d, i)
            for _ in range(n + 1):
                i = skip_leb(d, i)
        elif op in (0x10, 0x12):
            t, j = uleb(d, i)
            on_call(p, op, t, j)
            i = j
        elif op in (0x11, 0x13):
            i = skip_leb(d, i); i = skip_leb(d, i)
            if on_indirect:
                on_indirect(p, op, i)
        elif op == 0x1C:
            n, i = uleb(d, i)
            i += n
        elif op == 0x1F:  # try_table
            i = skip_blocktype(d, i)
            n, i = uleb(d, i)
            for _ in range(n):
                k = d[i]; i += 1
                if k in (0, 1):
                    i = skip_leb(d, i)
                i = skip_leb(d, i)
        elif 0x28 <= op <= 0x3E:
            if op in LOADS:
                a, j = uleb(d, i)
                if a & 0x40:
                    j = skip_leb(d, j)
                off, j = uleb(d, j)
                on_load(p, op, j, a, off)
                i = j
            else:
                i = skip_memarg(d, i)
        elif op in (0x3F, 0x40):
            i = skip_leb(d, i)
        elif op == 0x41:
            i = skip_leb(d, i)
        elif op == 0x42:
            i = skip_leb(d, i)
        elif op == 0x43:
            i += 4
        elif op == 0x44:
            i += 8
        elif op == 0xD0:
            i = skip_blocktype(d, i)
        elif op == 0xFC:
            sub, i = uleb(d, i)
            if sub <= 7:
                pass
            elif sub == 8:
                i = skip_leb(d, i); i = skip_leb(d, i)
            elif sub in (9, 13, 15, 16, 17):
                i = skip_leb(d, i)
            elif sub in (10, 12, 14):
                i = skip_leb(d, i); i = skip_leb(d, i)
            elif sub == 11:
                i = skip_leb(d, i)
            else:
                raise SystemExit(f'0xFC {sub} at {p:#x}')
        elif op == 0xFD:
            sub, i = uleb(d, i)
            if sub <= 11 or sub in (92, 93):
                i = skip_memarg(d, i)
            elif 84 <= sub <= 91:
                i = skip_memarg(d, i); i += 1
            elif sub in (12, 13):
                i += 16
            elif 21 <= sub <= 34:
                i += 1
        elif op == 0xFE:
            sub, i = uleb(d, i)
            if sub == 3:
                i += 1
            else:
                i = skip_memarg(d, i)
        else:
            raise SystemExit(f'unknown opcode {op:#x} at {p:#x}')
    return i

# ---------------------------------------------------------------- names
def parse_map(path):
    """name -> (object, mangled, demangled) for the functions in the map; keyed
    by both names (the name section has the demangled ones)."""
    out = {}
    pat = re.compile(r'^\s+-\s+[0-9a-f]+\s+[0-9a-f]+\s+(\S.*?):\((.+)\)$')
    with open(path) as f:
        lines = f.read().split('\n')
    for k, line in enumerate(lines):
        m = pat.match(line)
        if not m:
            continue
        obj, sym = m.group(1), m.group(2)
        dem = sym
        if k + 1 < len(lines):
            nxt = lines[k + 1].split(None, 3)
            if len(nxt) == 4 and nxt[0] == '-':
                dem = nxt[3]
        out.setdefault(sym, (obj, sym, dem))
        out.setdefault(dem, (obj, sym, dem))
    return out

def split_qual(s):
    """'A::B<x::y>::f' -> ['A', 'B<x::y>', 'f'] at top-level '::'."""
    parts, depth, cur, i = [], 0, '', 0
    while i < len(s):
        c = s[i]
        if c in '<(':
            depth += 1
        elif c in '>)':
            depth -= 1
        if depth == 0 and s.startswith('::', i):
            parts.append(cur); cur = ''; i += 2; continue
        cur += c; i += 1
    parts.append(cur)
    return parts

def fn_head(dem):
    """Qualified name of a demangled function (without the argument list)."""
    depth = 0
    for i, c in enumerate(dem):
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif c == '(' and depth == 0 and not dem.startswith('operator', max(0, i - 8)):
            return dem[:i], dem[i:]
    return dem, ''

def ctor_class(dem):
    head, _ = fn_head(dem)
    parts = split_qual(head)
    if len(parts) < 2:
        return None
    base = lambda s: s.split('<')[0]
    if base(parts[-1]) == base(parts[-2]):
        return '::'.join(parts[:-1])
    return None

# MWCC (CodeWarrior) mangling, for joining with the decomp's symbols.txt.
MW_BASIC = {'void': 'v', 'bool': 'b', 'char': 'c', 'signed char': 'Sc', 'unsigned char': 'Uc', 'short': 's',
            'unsigned short': 'Us', 'int': 'i', 'unsigned int': 'Ui', 'long': 'l', 'unsigned long': 'Ul',
            'long long': 'x', 'unsigned long long': 'Ux', 'float': 'f', 'double': 'd', 'wchar_t': 'w', '...': 'e'}

def mw_qual(parts):
    if len(parts) == 1:
        return f'{len(parts[0])}{parts[0]}'
    return f'Q{len(parts)}' + ''.join(f'{len(p)}{p}' for p in parts)

def mw_type(t):
    t = t.strip()
    if t in MW_BASIC:
        return MW_BASIC[t]
    for suf, code in (('*', 'P'), ('&', 'R')):
        if t.endswith(suf):
            inner = t[:-1].strip()
            const = ''
            if inner.endswith(' const'):
                inner = inner[:-6].strip(); const = 'C'
            elif inner.startswith('const '):
                inner = inner[6:].strip(); const = 'C'
            r = mw_type(inner)
            return None if r is None else code + const + r
    if '<' in t or '(' in t or '[' in t:
        return None
    return mw_qual(split_qual(t))

def split_args(a):
    a = a.strip()
    assert a.startswith('(')
    close = a.rfind(')')
    inner, tail = a[1:close], a[close + 1:]
    args, depth, cur = [], 0, ''
    for c in inner:
        if c in '<(':
            depth += 1
        elif c in '>)':
            depth -= 1
        if c == ',' and depth == 0:
            args.append(cur); cur = ''
        else:
            cur += c
    if cur.strip():
        args.append(cur)
    return args, tail.strip()

def mwcc_name(dem):
    head, args = fn_head(dem)
    if not args:
        return None
    parts = split_qual(head)
    if any('<' in p for p in parts):
        return None
    name = parts[-1]
    quals = parts[:-1]
    if quals and name == quals[-1]:
        name = '__ct'
    elif quals and name == '~' + quals[-1]:
        name = '__dt'
    elif name.startswith('operator'):
        return None
    try:
        al, tail = split_args(args)
    except AssertionError:
        return None
    codes = [mw_type(x) for x in al]
    if any(c is None for c in codes):
        return None
    sig = ''.join(codes) or 'v'
    cq = 'C' if tail.startswith('const') else ''
    q = mw_qual(quals) if quals else ''
    return f'{name}__{q}{cq}F{sig}'

def load_symbols(path):
    full = {}
    if not path or not os.path.exists(path):
        return full
    pat = re.compile(r'^(\S+) = \.text:(0x[0-9A-Fa-f]+);')
    for line in open(path):
        m = pat.match(line)
        if m:
            full[m.group(1)] = m.group(2)
    return full

# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('wasm')
    ap.add_argument('map')
    ap.add_argument('--symbols')
    ap.add_argument('--json')
    a = ap.parse_args()

    orig = a.wasm + '.orig'
    src = open(a.wasm, 'rb').read()
    # A rerun on an already instrumented module starts from the saved original.
    if os.path.exists(orig) and src.startswith(b'\0asm') and b'fsa_uninit_instrumented' in src:
        src = open(orig, 'rb').read()
    else:
        shutil.copyfile(a.wasm, orig)
    d = src
    secs = read_sections(d)
    types = imports = funcs = exports = names = code = None
    for s in secs:
        if s[0] == 1: types = parse_types(s[1])
        elif s[0] == 2: imports = parse_imports(s[1])
        elif s[0] == 3: funcs = parse_funcs(s[1])
        elif s[0] == 7: exports = parse_exports(s[1])
        elif s[0] == 10: code = s
        elif s[0] == 0 and custom_name(s[1])[0] == 'name': names = parse_names(s[1])
    if names is None:
        raise SystemExit('no name section (link with --profiling-funcs)')
    nimp = imports or 0
    need = ['fsa_uninit_hit', 'fsa_uninit_ctor_enter', 'fsa_uninit_ctor_exit', 'fsa_uninit_site_ptr']
    for n in need:
        if n not in exports:
            raise SystemExit(f'{n} not exported (build with -DFSA_DIAG_UNINIT=ON)')
    f_hit, f_enter, f_exit, f_siteptr = (exports[n] for n in need)

    mp = parse_map(a.map)
    full_syms = load_symbols(a.symbols)

    def is_game(idx):
        nm = names.get(idx)
        if nm is None:
            return False
        e = mp.get(nm)
        return e is not None and 'libfsa_units_' in e[0]

    # The address fsa_uninit_site_ptr() returns: its body is `i32.const N; end`.
    body = code[1]
    cnt, i = uleb(body, 0)
    bodies = []
    for _ in range(cnt):
        n, j = uleb(body, i)
        bodies.append((j, j + n))
        i = j + n
    s0, s1 = bodies[f_siteptr - nimp]
    nloc, k = uleb(body, s0)
    for _ in range(nloc):
        _, k = uleb(body, k); k += 1
    if body[k] != 0x41:
        raise SystemExit('fsa_uninit_site_ptr is not a constant')
    site_addr, k = sleb(body, k + 1)
    site_addr &= 0xFFFFFFFF

    skip_names = {'memcpy', 'memmove', 'memset', 'memcmp', 'strlen', 'strcmp', 'strncmp', 'strcpy', 'strncpy'}
    alloc_fns = set()
    ctors = {}
    for idx, nm in names.items():
        if idx < nimp:
            continue
        mangled = mp[nm][1] if nm in mp else nm
        if mangled.startswith(('_Znw', '_Zna', '_ZN7JKRHeap5allocE')) or nm.startswith(('operator new', 'JKRHeap::alloc(')):
            alloc_fns.add(idx)
        if is_game(idx) and re.search(r'C[12]E', mangled):
            cls = ctor_class(mp[nm][2])
            if cls:
                ptypes = types[funcs[idx - nimp]][0]
                if ptypes and ptypes[0] == 0x7F:
                    ctors[idx] = cls
    nfun = len(funcs)
    wrapper_of = {}
    for n, idx in enumerate(sorted(ctors)):
        wrapper_of[idx] = nimp + nfun + n

    I32, I64, F32 = 0x7F, 0x7E, 0x7D
    P32 = enc_s(POISON32 - (1 << 32))
    out_bodies = []
    stats = dict(loads=0, funcs=0, calls=0, allocsites=0)
    for fi, (b0, b1) in enumerate(bodies):
        idx = nimp + fi
        nm = names.get(idx, '')
        game = is_game(idx) and nm not in skip_names and idx not in (f_hit, f_enter, f_exit, f_siteptr)
        if not game and not wrapper_of:
            out_bodies.append(body[b0:b1]); continue
        params = types[funcs[fi]][0]
        nloc, k = uleb(body, b0)
        locs = []
        total = len(params)
        for _ in range(nloc):
            c, k = uleb(body, k)
            t = body[k]; k += 1
            locs.append((c, t)); total += c
        code0 = k
        edits = []  # (pos, delete_len, insert_bytes)
        LA, LV, LF, L64 = total, total + 1, total + 2, total + 3

        def on_load(p, op, j, align, off):
            if not game:
                return
            kind = LOADS[op]
            size = {'i32': 4, 'i64': 8, 'f32': 4, 's16': 2, 'u16': 2}[kind]
            soff = off - (1 << 32) if off >= 1 << 31 else off
            hit = b'\x20' + enc_u(LA) + b'\x41' + enc_s(soff) + b'\x41' + enc_s(idx) + b'\x41' + enc_s(size) + b'\x10' + enc_u(f_hit)
            pre = b'\x22' + enc_u(LA)
            ld = body[p:j]
            if kind in ('i32', 's16', 'u16'):
                pv = P32 if kind == 'i32' else enc_s(POISON16U if kind == 'u16' else POISON16S)
                post = b'\x22' + enc_u(LV) + b'\x41' + pv + b'\x46' + b'\x04\x40' + hit + b'\x0b' + b'\x20' + enc_u(LV)
            elif kind == 'f32':
                post = b'\x22' + enc_u(LF) + b'\xbc' + b'\x41' + P32 + b'\x46' + b'\x04\x40' + hit + b'\x0b' + b'\x20' + enc_u(LF)
            else:
                post = (b'\x22' + enc_u(L64) + b'\xa7' + b'\x41' + P32 + b'\x46' +
                        b'\x20' + enc_u(L64) + b'\x42\x20\x88' + b'\xa7' + b'\x41' + P32 + b'\x46' + b'\x72' +
                        b'\x04\x40' + hit + b'\x0b' + b'\x20' + enc_u(L64))
            edits.append((p, j - p, pre + ld + post))
            stats['loads'] += 1

        def on_call(p, op, t, j):
            ins = b''
            if game and t in alloc_fns and idx not in alloc_fns:
                ins = b'\x41' + enc_s(site_addr) + b'\x41' + enc_s(idx) + b'\x36\x02\x00'
                stats['allocsites'] += 1
            if t in wrapper_of:
                edits.append((p, j - p, ins + bytes([op]) + enc_u(wrapper_of[t])))
                stats['calls'] += 1
            elif ins:
                edits.append((p, 0, ins))

        end = scan_body(body, code0, b1, on_load, on_call)
        assert end == b1, (nm, end, b1)
        if not edits:
            out_bodies.append(body[b0:b1]); continue
        stats['funcs'] += 1
        nb = bytearray()
        if any(e[2][:1] == b'\x22' for e in edits):  # load checks use 4 new locals
            nb += enc_u(nloc + 3) + body[b0 + len(enc_u(nloc)):code0] + b'\x02\x7f' + b'\x01\x7d' + b'\x01\x7e'
        else:
            nb += body[b0:code0]
        pos = code0
        for p, dl, ins in edits:
            nb += body[pos:p]; nb += ins; pos = p + dl
        nb += body[pos:b1]
        out_bodies.append(bytes(nb))

    # Constructor wrappers.
    new_funcs = []
    new_names = []
    for idx in sorted(ctors):
        ti = funcs[idx - nimp]
        params, res = types[ti]
        b = bytearray(b'\x00')
        b += b'\x20\x00\x41' + enc_s(idx) + b'\x10' + enc_u(f_enter)
        for pi in range(len(params)):
            b += b'\x20' + enc_u(pi)
        b += b'\x10' + enc_u(idx)
        b += b'\x20\x00\x41' + enc_s(idx) + b'\x10' + enc_u(f_exit)
        b += b'\x0b'
        new_funcs.append((ti, bytes(b)))
        new_names.append((wrapper_of[idx], 'uninit_wrap$' + names[idx]))

    # Reassemble.
    out = bytearray(d[:8])
    for sid, sb in secs:
        if sid == 3:
            sb = enc_u(nfun + len(new_funcs)) + sb[len(enc_u(nfun)):] + b''.join(enc_u(t) for t, _ in new_funcs)
        elif sid == 10:
            allb = out_bodies + [nb for _, nb in new_funcs]
            sb = enc_u(len(allb)) + b''.join(enc_u(len(x)) + x for x in allb)
        elif sid == 0 and custom_name(sb)[0] == 'name':
            sb = rebuild_names(sb, new_names)
        out += bytes([sid]) + enc_u(len(sb)) + sb
    cs = enc_name('fsa_uninit_instrumented') + b'1'
    out += b'\x00' + enc_u(len(cs)) + cs
    open(a.wasm, 'wb').write(out)

    # Names for the page.
    funcs_json = {}
    for idx, nm in names.items():
        if not is_game(idx):
            continue
        _, mangled, dem = mp[nm]
        mw = mwcc_name(dem)
        addr = full_syms.get(mw) if mw else None
        funcs_json[str(idx)] = [mangled, dem, mw if addr else None, addr]
    for idx, cls in ctors.items():
        funcs_json[str(idx)].append(cls)
    jpath = a.json or os.path.join(os.path.dirname(a.wasm), 'fsa.uninit.json')
    with open(jpath, 'w') as f:
        json.dump({'funcs': funcs_json}, f, separators=(',', ':'))
    print(f'uninit_instrument: {stats["loads"]} loads in {stats["funcs"]} functions, {len(ctors)} constructors '
          f'({stats["calls"]} calls redirected), {stats["allocsites"]} allocation sites, site word at {site_addr:#x}; '
          f'{sum(1 for v in funcs_json.values() if v[2])}/{len(funcs_json)} functions matched to symbols.txt')

if __name__ == '__main__':
    main()
