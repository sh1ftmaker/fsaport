#!/usr/bin/env python3
"""Readable per-function summary of a function-trace run (docs/DIAGNOSTICS.md,
"Function traces (fntrace)").

  tools/fntrace_summary.py logs/diag/<run> [more runs] [--build build/diag]
      [--out FILE.md] [--split DIR] [--symbols symbols.txt] [--notes NOTES.json]

Reads <run>/fntrace_config.jsonl, fntrace_agg.jsonl (cumulative aggregates over
every traced call; the last record per function id wins) and fntrace.jsonl (the
sampled per-call records). Writes FILE.md (default <first run>/fntrace_summary.md);
with --split one DIR/<name>.md per function plus the matching JSONL lines in
DIR/<name>.jsonl. --build names the build directory whose fsa.diag.vt.tsv and
fsa.diag.data.tsv identify vtable and static-data pointers (default build/diag).

<name> is the decomp's full mangled name from symbols.txt with < > / as _ (the
key fsa-decomp's tools/grind.py looks answers up by), also when the request
gave a shortened name or the port's signature differs (DecompSyms.resolve).
Functions without a decomp symbol (port-only helpers, inlined in the original)
keep the request's name with every character outside [A-Za-z0-9_.-] as _.
--symbols is the decomp's symbols.txt (default: FSA_DECOMP_SYMBOLS of the
build's CMakeCache.txt). Every per-function .md starts with a one-line summary
(calls, callers, the arguments' values, branches); --notes JSON {name: text}
appends text to its "branches:" part (the branches seen / not seen, which an
entry/exit trace cannot know by itself); the key is the decomp name or the
request's sym, "*" applies to every function.
"""
import argparse, collections, json, math, os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fntrace_layout  # noqa: E402

NOTE = ('Memory is the port\'s: the game\'s own structs are in host (little-endian) byte order and '
        'clang (wasm32) layout, which matches the decomp\'s offsets wherever the class layout agrees '
        '(pointers are 4 bytes; a vtable pointer sits where clang puts it). Snapshot hex is memory order, '
        'so a u32 field 0x00000102 reads "02010000". Data that the game keeps in file (big-endian) form '
        'is still big-endian in memory.')


def s32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v


def f32(v):
    return struct.unpack('<f', struct.pack('<I', v & 0xFFFFFFFF))[0]


def fmt_f(x):
    if x is None or (isinstance(x, float) and (math.isnan(x) or math.isinf(x))):
        return str(x)
    return f'{x:.6g}'


class Tables:
    def __init__(self, build):
        self.vt, self.data = [], []
        p = os.path.join(build, 'fsa.diag.vt.tsv') if build else ''
        if p and os.path.exists(p):
            for line in open(p):
                f = line.rstrip('\n').split('\t')
                if len(f) >= 4:
                    self.vt.append((int(f[0]), int(f[1]), f[2]))
        p = os.path.join(build, 'fsa.diag.data.tsv') if build else ''
        if p and os.path.exists(p):
            for line in open(p):
                f = line.rstrip('\n').split('\t')
                if len(f) >= 3:
                    self.data.append((int(f[0]), int(f[1]), f[2]))
        self.vt.sort()
        self.data.sort()

    @staticmethod
    def _find(tab, a):
        lo, hi = 0, len(tab)
        while lo < hi:
            m = (lo + hi) // 2
            if tab[m][0] <= a:
                lo = m + 1
            else:
                hi = m
        if lo and a < tab[lo - 1][0] + max(tab[lo - 1][1], 1):
            return tab[lo - 1]
        return None

    def vtable(self, a):
        r = self._find(self.vt, a)
        return r[2] if r else None

    def datasym(self, a):
        r = self._find(self.data, a)
        return (r[2] + (f'+0x{a - r[0]:x}' if a != r[0] else '')) if r else None


def guess(vals, width, tables):
    """A type guess for a field from its values (ints of `width` bytes)."""
    if not vals:
        return ''
    sv = [v - (1 << (8 * width)) if v >> (8 * width - 1) else v for v in vals]
    if set(vals) <= {0, 1}:
        return 'bool / flag (0, 1)'
    if width == 4:
        nz = [v for v in vals if v]
        vt = [tables.vtable(v) for v in nz]
        if nz and all(vt):
            return 'vtable pointer (' + ', '.join(sorted(set(vt))) + ')'
        ds = [tables.datasym(v) for v in nz]
        if nz and all(ds) and all(v & 3 == 0 for v in nz):
            return 'pointer to static data (' + ', '.join(sorted(set(d.split('+')[0] for d in ds))[:4]) + ')'
        if nz and all(0x100000 <= v < 0x20000000 and v & 3 == 0 for v in nz):
            return 'pointer (port heap address)' + (' or null' if len(nz) < len(vals) else '')
        fl = [f32(v) for v in vals]
        if all(math.isfinite(x) and (x == 0 or 1e-5 <= abs(x) <= 1e7) for x in fl) and \
                any(abs(v) > 0xFFFFFF for v in sv):
            return f'f32 [{fmt_f(min(fl))}, {fmt_f(max(fl))}]'
    lo, hi = min(sv), max(sv)
    if width == 4:
        if -0x8000 <= lo and hi <= 0x7FFF:
            t = 'int, fits s16' + (' (and u8)' if 0 <= lo and hi <= 0xFF else '')
        elif 0 <= lo and hi <= 0xFFFF:
            t = 'int, fits u16 (not s16)'
        else:
            t = 'int, needs 32 bits'
    elif width == 2:
        t = 's16/u16' + (': negative as s16' if lo < 0 else '') + (', fits u8' if 0 <= lo and hi <= 0xFF else '')
    else:
        t = 'u8/s8'
    if len(set(vals)) <= 8 and hi - lo < 64:
        t += ', few values (enum/state?)'
    return t + f' [{lo}, {hi}]'


def width_of(bytes_changed):
    """Field width and offset within the word from per-byte change counts."""
    ch = [i for i in range(4) if bytes_changed[i]]
    if not ch:
        return []
    if ch[-1] >= 2 and ch[0] <= 1:
        return [(0, 4)]
    out = []
    lo = [i for i in ch if i < 2]
    hi = [i for i in ch if i >= 2]
    for part, base in ((lo, 0), (hi, 2)):
        if not part:
            continue
        if len(part) == 2 or part == [base + 1]:
            # a change only in the high byte of a halfword is still a 16-bit candidate; report both
            out.append((base, 2) if len(part) == 2 else (base + 1, 1))
        else:
            out.append((base, 1))
    return out


def sub(v, off, width):
    return (v >> (8 * off)) & ((1 << (8 * width)) - 1)


def symbols_path(build, given):
    if given:
        return given
    p = os.path.join(build or '', 'CMakeCache.txt')
    if os.path.exists(p):
        for line in open(p):
            if line.startswith('FSA_DECOMP_SYMBOLS:'):
                return line.split('=', 1)[1].strip()
    p = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                     '../fsadecomp/fsa-decomp/config/G4SE01/symbols.txt')
    return p if os.path.exists(p) else None


class DecompSyms:
    """The decomp's symbols.txt functions (names, by address), and the port's
    name table (fsa.diag.tsv: port demangled name -> decomp symbol)."""

    def __init__(self, path, build=None):
        self.names, self.by_addr, self.order = set(), collections.defaultdict(list), []
        if path and os.path.exists(path):
            for line in open(path, errors='replace'):
                m = re.match(r'(\S+) = \.text:0x([0-9A-Fa-f]+);.*type:function', line)
                if m:
                    self.names.add(m.group(1))
                    self.order.append(m.group(1))
                    self.by_addr[int(m.group(2), 16)].append(m.group(1))
        self.port = collections.defaultdict(set)
        p = os.path.join(build or '', 'fsa.diag.tsv')
        if os.path.exists(p):
            for line in open(p, errors='replace'):
                f = line.rstrip('\n').split('\t')
                if len(f) >= 3 and f[1]:
                    self.port[f[2]].add(f[1])

    @staticmethod
    def split(sym):
        """CodeWarrior name -> (function name, mangled qualifier or '') or None."""
        i = sym.find('__', 2 if sym.startswith('__') else 1)
        if i < 0:
            return None
        base, rest = sym[:i], sym[i + 2:]
        q = ''
        m = re.match(r'(\d+)', rest)
        if m:
            q = rest[:len(m.group(1)) + int(m.group(1))]
        elif len(rest) > 1 and rest[0] == 'Q' and rest[1].isdigit():
            j = 2
            for _ in range(int(rest[1])):
                m = re.match(r'(\d+)', rest[j:])
                if not m:
                    return None
                j += len(m.group(1)) + int(m.group(1))
            q = rest[:j]
        return base, q

    def resolve(self, sym, port_sym=None):
        """The decomp's full symbol for a traced function, or None: the request's
        or the port's symbol when symbols.txt has it; the decomp symbol the port's
        name table gives a demangled name; the one symbols.txt function with the
        same name and class (a shortened request name, or a signature that differs);
        the one at the address the name carries (fn_801F172C)."""
        for s in (sym, port_sym):
            if s and s in self.names:
                return s
        for s in (sym, port_sym):
            c = self.port.get(s or '')
            if c and len(c) == 1:
                return next(iter(c))
        for s in (sym, port_sym):
            sp = self.split(s) if s else None
            if not sp:
                continue
            base, q = sp
            pre = base + '__' + q
            c = [n for n in self.order if n.startswith(pre) and n[len(pre):len(pre) + 1] in ('F', 'C')]
            if len(c) == 1:
                return c[0]
            m = re.search(r'_(8[0-9A-Fa-f]{7})$', base)
            if m:
                c = [n for n in self.by_addr.get(int(m.group(1), 16), []) if n.startswith(base)]
                if len(c) == 1:
                    return c[0]
        return None


def answer_key(sym, dname):
    """File name (no extension) of a function's answer: grind.py's key for the
    decomp name (< > / as _), else the request's name made file-safe."""
    if dname:
        return re.sub(r'[<>/]', '_', dname)
    return re.sub(r'[^A-Za-z0-9_.-]', '_', sym)[:150]


def _vals(a, decl):
    """Short text for one argument's (or the result's) aggregate."""
    if not a or not a.get('n'):
        return 'no calls'
    t = a['t']
    d = a.get('distinct', [])
    more = '+' if a.get('more') else ''
    if decl.startswith(('P', 'R')):
        nn = sum(n for v, n in d if int(v, 16) == 0)
        return f'pointer ({len(d)}{more} distinct' + (f', null ×{nn}' if nn else '') + ')'

    def one(v):
        iv = int(v, 16)
        if t == 'f32':
            return fmt_f(f32(iv))
        if t == 'f64':
            return fmt_f(struct.unpack('<d', struct.pack('<Q', iv))[0])
        return str(s32(iv) if t == 'i32' else iv)
    if d and not more and len(d) <= 8:
        if t in ('f32', 'f64'):
            d = sorted(d, key=lambda x: float(one(x[0])) if one(x[0]) not in ('nan', 'inf', '-inf') else 0)
        else:
            d = sorted(d, key=lambda x: s32(int(x[0], 16)) if t == 'i32' else int(x[0], 16))
        return '{' + ', '.join(f'{one(v)}×{n}' for v, n in d) + '}'
    lo, hi = (fmt_f(a['min']), fmt_f(a['max'])) if t in ('f32', 'f64') else (a['min'], a['max'])
    return f'[{lo}, {hi}] ({len(d)}{more} distinct)'


def merge_stats(sts):
    """One argument's aggregates from several runs -> one."""
    sts = [s for s in sts if s and s.get('n')]
    if not sts:
        return None
    m = dict(sts[0])
    if len(sts) == 1:
        return m
    d = collections.Counter()
    for s in sts:
        for v, n in s.get('distinct', []):
            d[v] += n
    m.update(n=sum(s['n'] for s in sts), min=min(s['min'] for s in sts), max=max(s['max'] for s in sts),
             distinct=[[v, n] for v, n in d.most_common()][:16],
             more=int(any(s.get('more') for s in sts) or len(d) > 16))
    return m


def merged_agg(aggs):
    """The args/ret aggregates of several runs (one agg per run) merged."""
    if not aggs:
        return {}
    a0 = dict(aggs[0])
    if len(aggs) > 1 and a0.get('args') is not None:
        a0['args'] = [merge_stats([a['args'][i] for a in aggs if a.get('args') and i < len(a['args'])])
                      or a0['args'][i] for i in range(len(a0['args']))]
        if a0.get('ret'):
            a0['ret'] = merge_stats([a.get('ret') for a in aggs]) or a0['ret']
    return a0


def one_line(dname, sym, cfgs, aggs, note):
    """The one-line summary at the top of a function's answer file."""
    c0 = cfgs[0] if cfgs else {}
    a0 = merged_agg(aggs)
    calls = sum(a['calls'] for a in aggs)
    entries = sum(a.get('entries', 0) for a in aggs)
    parts = [f'calls {calls}' + (f' (+{entries - calls} inlined entries not seen)'
                                 if c0.get('kind') == 'primary' and entries > calls else '')]
    if c0.get('kind') == 'nested':
        parts[0] += ' (nested: only while a primary function of the request ran)'
    cl = collections.Counter()
    for a in aggs:
        for c in a.get('callers', []):
            nm = c.get('id_sym') or c.get('id_name') or str(c['id'])
            i = nm.find('__', 2 if nm.startswith('__') else 1)
            cl[nm[:i] if i > 0 else nm] += c['n']
    if cl:
        parts.append('from ' + ', '.join(f'{k} ×{n}' for k, n in cl.most_common(3)) + (', …' if len(cl) > 3 else ''))
    if calls and a0.get('args') is not None:
        decl = (c0.get('declared') or '').split(',') if c0.get('declared') else []
        ti = c0.get('this_arg', -1)
        av, k = [], 0
        for i, a in enumerate(a0['args']):
            if i == ti or (ti == 1 and i == 0):
                continue
            d = decl[k] if k < len(decl) else ''
            k += 1
            av.append(f'arg{i}{" " + d if d else ""} = {_vals(a, d)}')
        if av:
            parts.append('args: ' + '; '.join(av))
        if a0.get('ret'):
            parts.append('ret = ' + _vals(a0['ret'], ''))
    parts.append('branches: ' + (note or 'not instrumented (entry/exit trace)'))
    return f'**Summary** `{dname or sym}`: ' + '. '.join(parts) + ('' if parts[-1].endswith('.') else '.')


def load(runs):
    cfg, agg, recs = [], {}, []
    for r in runs:
        p = os.path.join(r, 'fntrace_config.jsonl')
        if os.path.exists(p):
            cfg += [json.loads(l) for l in open(p) if l.strip()]
        p = os.path.join(r, 'fntrace_agg.jsonl')
        if os.path.exists(p):
            for l in open(p):
                if l.strip():
                    o = json.loads(l)
                    agg[(r, o['fn'])] = o
        p = os.path.join(r, 'fntrace.jsonl')
        if os.path.exists(p):
            for l in open(p):
                if l.strip():
                    o = json.loads(l)
                    o['_run'] = r
                    recs.append(o)
    return cfg, agg, recs


def arg_line(i, a, decl, is_this):
    name = 'this' if is_this else f'arg{i}'
    t = a['t']
    if not a.get('n'):
        return f'- `{name}` ({t}{", " + decl if decl else ""}): no calls'
    s = f'- `{name}` ({t}{", declared " + decl if decl else ""}): '
    if t in ('i32', 'i64'):
        s += f'signed [{a["min"]}, {a["max"]}], unsigned [{a["umin"]:#x}, {a["umax"]:#x}]'
        if t == 'i32' and not is_this:
            lo, hi = a['min'], a['max']
            if -0x8000 <= lo and hi <= 0x7FFF:
                s += '; every value fits s16'
            elif 0 <= a['umin'] and a['umax'] <= 0xFFFF:
                s += '; values fit u16 but not s16'
            else:
                s += '; needs 32 bits'
    else:
        s += f'[{fmt_f(a["min"])}, {fmt_f(a["max"])}]'
    d = a.get('distinct', [])
    if d and not is_this:
        shown = []
        for v, n in d:
            iv = int(v, 16)
            if t == 'f32':
                shown.append(f'{fmt_f(f32(iv))}×{n}')
            elif t == 'f64':
                shown.append(f'{fmt_f(struct.unpack("<d", struct.pack("<Q", iv))[0])}×{n}')
            else:
                shown.append(f'{s32(iv) if t == "i32" else iv}×{n}')
        s += '; values: ' + ', '.join(shown) + (' (more)' if a.get('more') else ' (all)')
    elif d and is_this:
        s += f'; {len(d)}{"+" if a.get("more") else ""} distinct objects'
    return s


ELEM = {'u8': 1, 's8': 1, 'char': 1, '_Bool': 1, 'bool': 1, 'unsigned char': 1, 'signed char': 1,
        'u16': 2, 's16': 2, 'short': 2, 'unsigned short': 2}


def elem_info(f):
    """(element size or None, [dims]) from a layout field's type: 'u16[8][64]' -> (2, [8, 64])."""
    t = f.get('type') or ''
    dims = [int(x) for x in re.findall(r'\[(\d+)\]', t)]
    base = re.sub(r'\[\d+\]', '', t).strip()
    if '*' in base:
        return 4, dims
    if base in ELEM:
        return ELEM[base], dims
    if base in ('s32', 'u32', 'f32', 'int', 'long', 'unsigned long', 'unsigned int', 'float'):
        return 4, dims
    return None, dims


def pieces(lay, port_off, bytes_):
    """The changed parts of the word at port_off: [(offset in word, width)], using the
    field's element size from the layout when known, else the changed bytes."""
    if lay:
        f = fntrace_layout.field_at(lay, port_off)
        if f and f['off'] <= port_off < f['off'] + max(f['size'], 1):
            es, _ = elem_info(f)
            if es and (port_off - f['off']) % min(es, 4) == 0:
                es = min(es, 4)
                return [(k, es) for k in range(0, 4, es) if any(bytes_[k:k + es])]
    return width_of(bytes_)


def labeler(lay):
    """port offset -> 'decomp offset `field` (type)' text."""
    def lbl(p):
        if not lay:
            return f'0x{p:x}'
        d, f = fntrace_layout.to_decomp(lay, p)
        if not f:
            return f'0x{p:x}'
        if lay.get('size') and p >= lay['size']:
            return f'0x{d:x} (past the port\'s sizeof 0x{lay["size"]:x}: a derived class\'s field?){"" if d == p else f" [port 0x{p:x}]"}'
        inner = p - f['off']
        es, dims = elem_info(f)
        if inner and es and dims and inner % es == 0 and inner < f['size']:
            k, idx = inner // es, []
            for dim in reversed(dims):
                idx.append(k % dim)
                k //= dim
            name = f['name'] + ''.join(f'[{i}]' for i in reversed(idx))
        else:
            name = f['name'] + (f'+0x{inner:x}' if inner else '')
        t = f' ({f["type"]})' if f['type'] and not inner else ''
        port = f' [port 0x{p:x}]' if d != p else ''
        return f'0x{d:x} `{name}`{t}{port}'
    return lbl


def elem_labeler(lay):
    """Offset in a global -> text: the field of its layout (`[i]` element of an
    array of that type when past its size), or +0x... without a layout."""
    if not lay:
        return lambda p: f'+0x{p:x}'
    lbl, size = labeler(lay), lay.get('size') or 0

    def f(p):
        if size and p >= size:
            return f'[{p // size}] ' + lbl(p % size)
        return lbl(p)
    return f


def render_fields(w, words, lay, calls, tables):
    """The written / varying / constant words of a snapshot (`this` or a pointee):
    words = {port offset: [word stats]}."""
    lbl = labeler(lay)
    written = [(off, l) for off, l in sorted(words.items()) if any(x.get('changed') for x in l)]
    if written:
        w('Written by the function (offset, how often, guessed type from the values it stores):\n')
        w('| offset | changed in calls | bytes changed | values at entry | new values | guess |')
        w('|---|---|---|---|---|---|')
        for off, l in written:
            wd = l[0]
            bytes_ = [sum(x.get('bytes', [0] * 4)[i] for x in l) for i in range(4)]
            ch = sum(x.get('changed', 0) for x in l)
            for so, width in pieces(lay, off, bytes_):
                ent = sorted({sub(int(v, 16), so, width) for x in l for v, _ in x['entry']})
                new = sorted({sub(int(v, 16), so, width) for x in l for v, _ in x.get('new', [])})
                more = any(x.get('entry_more') or x.get('new_more') for x in l)

                def show(vs):
                    sv = [v - (1 << (8 * width)) if v >> (8 * width - 1) else v for v in vs]
                    if width == 4 and guess(vs, 4, tables).startswith('f32'):
                        return ', '.join(fmt_f(f32(v)) for v in vs[:8]) + (' …' if len(vs) > 8 else '')
                    if width == 4 and 'pointer' in guess(vs, 4, tables):
                        return ', '.join(f'{v:#x}' for v in vs[:6]) + (' …' if len(vs) > 6 else '')
                    return ', '.join(str(v) for v in sv[:12]) + (' …' if len(vs) > 12 else '')
                w(f'| {lbl(off + so)} | {ch} / {calls} | {width} | {show(ent)}{" (+more)" if more else ""} | '
                  f'{show(new)} | {guess(sorted(set(ent) | set(new)), width, tables)} |')
        w('')
    else:
        w('No snapshot word changed during any traced call.\n')
    # Varying but not written: inputs.
    varying = [(off, l) for off, l in sorted(words.items())
               if not any(x.get('changed') for x in l) and
               (sum(len(x['entry']) for x in l) > 1 or any(x.get('entry_more') for x in l))]
    if varying:
        w('Read-side context: words that differ between calls but the function does not write:\n')
        for off, l in varying[:40]:
            vs = sorted({int(v, 16) for x in l for v, _ in x['entry']})
            w(f'- {lbl(off)}: {len(vs)}{"+" if any(x.get("entry_more") for x in l) else ""} values, '
              f'{guess(vs, 4, tables)}')
        w('')
    const = [(off, l) for off, l in sorted(words.items())
             if sum(len(x['entry']) for x in l) == 1 and not any(x.get('changed') for x in l)]
    if const:
        w(f'Constant over the run ({len(const)} words): ' + ', '.join(
            f'{lbl(off)}={l[0]["entry"][0][0]}' + (f' ({tables.vtable(int(l[0]["entry"][0][0], 16))})'
                                                   if tables.vtable(int(l[0]['entry'][0][0], 16)) else '')
            for off, l in const[:24]) + (' …' if len(const) > 24 else '') + '\n')


def summarize(sym, cfgs, aggs, recs, tables, out, lay=None, ptr_lays=None, glob_lays=None):
    lbl = labeler(lay)
    c0 = cfgs[0] if cfgs else {}
    a0 = aggs[0] if aggs else {}
    name = a0.get('fn_name') or (recs[0]['fn_name'] if recs else '')
    w = out.append
    w(f'## `{sym}`\n')
    w(f'{name}  ')
    w(f'kind: {c0.get("kind", "?")}, request {c0.get("req", "?")}, declared parameters `({c0.get("declared") or ""})`, '
      f'wasm `({c0.get("params", "")}) -> {c0.get("result") or "void"}`, `this` = wasm param {c0.get("this_arg")}\n')
    if c0.get('question'):
        w(f'> question: {c0["question"]}\n')
    calls = sum(a['calls'] for a in aggs)
    logged = sum(a['logged'] for a in aggs)
    entries = sum(a.get('entries', 0) for a in aggs)
    w(f'- calls traced: **{calls}** ({logged} logged in full, {len(recs)} records here)')
    if c0.get('kind') == 'primary' and entries and not calls:
        w(f'- **no traced call** but {entries} instrumented entries: the function is inlined everywhere, or the '
          'page ran a fsa.wasm without the fntrace rewrite (a stale browser cache; touch build/diag/fsa.wasm)')
    elif c0.get('kind') == 'primary' and entries > calls:
        w(f'- instrumented entries: {entries}: {entries - calls} more than traced: inlined copies '
          '(the wrapper only sees out-of-line calls)')
    eip = sum(a.get('entries_in_parent', 0) for a in aggs)
    if c0.get('kind') == 'nested' and eip > calls:
        w(f'- instrumented entries while the request\'s primary function ran: {eip}: {eip - calls} more than '
          'traced: inlined copies (arguments and results of those are not seen)')
    unt = sum(a.get('untracked', 0) for a in aggs)
    if unt:
        w(f'- {unt} calls not tracked (more than 64 traced calls in flight)')
    nt = sum(a.get('null_this', 0) for a in aggs)
    if nt:
        w(f'- {nt} calls with `this` == 0')
    # scenes
    sc = collections.Counter()
    for a in aggs:
        for s in a.get('scenes', []):
            x = s['scene']
            sc[(x.get('mode'), x.get('stage'), x.get('level'), x.get('loaded'))] += s['n']
    if sc:
        w('- scenes: ' + ', '.join(f'mode {m} stage {s}-{l}{" (loaded)" if ld else ""} ×{n}'
                                     for (m, s, l, ld), n in sc.most_common()))
    cl = collections.Counter()
    for a in aggs:
        for c in a.get('callers', []):
            cl[c.get('id_sym') or c.get('id_name') or str(c['id'])] += c['n']
    if cl:
        w('- callers: ' + ', '.join(f'`{k}` ×{n}' for k, n in cl.most_common(12)) +
          (' (+more)' if any(a.get('callers_more') for a in aggs) else ''))
    if recs:
        ns = [r['n'] for r in recs]
        fr = [r['frame'] for r in recs]
        w(f'- logged calls span pad reads n {min(ns)}..{max(ns)}, frames {min(fr)}..{max(fr)}')
    parents = collections.Counter(r.get('parent') for r in recs if r.get('parent'))
    if parents:
        w(f'- nested: logged under {len(parents)} primary calls (records\' `parent` = their `seq`)')
    w('')
    # args / ret
    decl = (c0.get('declared') or '').split(',') if c0.get('declared') else []
    ti = c0.get('this_arg', -1)
    if a0.get('args') is not None:
        w('### Arguments and result (all traced calls)\n')
        k = 0
        for i, a in enumerate(a0['args']):
            is_this = i == ti
            d = ''
            if not is_this and not (ti == 1 and i == 0):
                d = decl[k] if k < len(decl) else ''
                k += 1
            w(arg_line(i, a, d, is_this))
        if a0.get('ret'):
            w(arg_line('', a0['ret'], '', False).replace('`arg`', '`return`'))
        w('')
    # this
    words = {}
    for a in aggs:
        for wd in a.get('words', []):
            words.setdefault(wd['off'], []).append(wd)
    if words:
        rng = a0.get('ranges', [])
        w(f'### `this` fields (port snapshot {", ".join(f"0x{s:x}-0x{e:x}" for s, e in rng)}; all traced calls)\n')
        if lay:
            w(f'Offsets are the decomp\'s (from the field names of the port\'s `{lay["cls"]}` layout; '
              '[port 0x...] where the port\'s offset differs).\n')
        render_fields(w, words, lay, calls, tables)
    # Pointer arguments (ptr_args): C strings, or pointee snapshots at entry and exit.
    pargs = collections.OrderedDict()
    for a in aggs:
        for p in a.get('ptrs', []):
            pargs.setdefault(p['arg'], []).append(p)
    argno = lambda v: 31 if v == 'ret' else v  # the result's pointee: arg 31 in the records
    pname = lambda v: 'Result pointer' if v == 31 else f'Pointer argument {v}'
    ptypes = {argno(p.get('arg')): p.get('type') for p in (c0.get('ptr_args') or [])}
    for arg, pl in pargs.items():
        nulls = sum(p.get('nulls', 0) for p in pl)
        if any(p.get('str') for p in pl):
            strs = collections.Counter()
            for p in pl:
                for s_, n in p.get('strings', []):
                    strs[s_] += n
            more = any(p.get('more') for p in pl)
            w(f'### {pname(arg)}: C string (at most {pl[0]["bytes"] - 1} characters read; all traced calls)\n')
            w(f'{len(strs)}{"+" if more else ""} distinct strings, {nulls} null:\n')
            for s_, n in strs.most_common():
                w(f'- `{json.dumps(s_)[1:-1]}` ×{n}')
            w('')
            continue
        play = (ptr_lays or {}).get(arg)
        t = ptypes.get(arg)
        what = 'result' if arg == 31 else f'arg{arg}'
        when = 'at exit' if arg == 31 else 'at entry and exit'
        w(f'### {pname(arg)}: `*{what}`{" (" + t + ")" if t else ""}, {pl[0]["bytes"]} bytes {when} '
          f'(all traced calls; {nulls} null)\n')
        if play:
            w(f'Offsets are the decomp\'s (the port\'s `{play["cls"]}` layout).\n')
        pw = {}
        for p in pl:
            for wd in p.get('words', []):
                pw.setdefault(wd['off'], []).append(wd)
        if pw:
            render_fields(w, pw, play, calls, tables)
    # Per-call deltas of the sampled records.
    deltas = collections.defaultdict(collections.Counter)
    for r in recs:
        for d in r.get('diff', []):
            o, n = int(d['old'], 16), int(d['new'], 16)
            if d['mask'] in (1, 3):
                deltas[d['off']][((n & 0xFFFF) - (o & 0xFFFF) + 0x8000) % 0x10000 - 0x8000] += 1
            elif d['mask'] < 16:
                deltas[d['off']][s32(n - o)] += 1
    if deltas:
        w('Per-call change of each written word (new - old, logged calls; low halfword for 16-bit changes):\n')
        for off in sorted(deltas):
            c = deltas[off]
            tag = ''
            if len(c) == 1 and abs(next(iter(c))) == 1:
                tag = ' (counter/timer: steps of %+d)' % next(iter(c))
            w(f'- {lbl(off)}: ' + ', '.join(f'{k:+d}×{n}' for k, n in c.most_common(8)) + (' …' if len(c) > 8 else '') + tag)
        w('')
    # Globals (from the logged calls only)
    gl = collections.OrderedDict()
    for r in recs:
        for g in r.get('globals', []):
            st = gl.setdefault(g['name'], dict(n=0, ch=0, words={}))
            st['n'] += 1
            st['ch'] += g['in'] != g['out']
            bi, bo = bytes.fromhex(g['in']), bytes.fromhex(g['out'])
            for o in range(0, len(bi) - len(bi) % 4 if len(bi) >= 4 else 0, 4):
                a, b = int.from_bytes(bi[o:o + 4], 'little'), int.from_bytes(bo[o:o + 4], 'little')
                ws = st['words'].setdefault(o, dict(entry=collections.Counter(), new=collections.Counter(),
                                                    changed=0, bytes=[0] * 4, deltas=collections.Counter()))
                ws['entry'][a] += 1
                if a != b:
                    ws['changed'] += 1
                    ws['new'][b] += 1
                    for k in range(4):
                        ws['bytes'][k] += bi[o + k] != bo[o + k]
            if len(bi) < 4:  # a 1-3 byte global: one partial word
                a = int.from_bytes(bi, 'little')
                b = int.from_bytes(bo, 'little')
                ws = st['words'].setdefault(0, dict(entry=collections.Counter(), new=collections.Counter(),
                                                    changed=0, bytes=[0] * 4, deltas=collections.Counter()))
                ws['entry'][a] += 1
                if a != b:
                    ws['changed'] += 1
                    ws['new'][b] += 1
                    for k in range(len(bi)):
                        ws['bytes'][k] += bi[k] != bo[k]
    if gl:
        w('### Globals (logged calls)\n')
        for k, st in gl.items():
            glbl = elem_labeler((glob_lays or {}).get(k))
            w(f'`{k}`: changed in {st["ch"]} of {st["n"]} logged calls.\n')
            rows = []
            for o, ws in sorted(st['words'].items()):
                for so, width in width_of(ws['bytes']):
                    ent = sorted({sub(v, so, width) for v in ws['entry']})
                    new = sorted({sub(v, so, width) for v in ws['new']})
                    sv = lambda vs: ', '.join(str(v - (1 << (8 * width)) if v >> (8 * width - 1) else v)
                                              for v in vs[:12]) + (' …' if len(vs) > 12 else '')
                    rows.append(f'| {glbl(o + so)} | {ws["changed"]} / {st["n"]} | {width} | {sv(ent)} | {sv(new)} | '
                                f'{guess(sorted(set(ent) | set(new)), width, tables)} |')
            if rows:
                w('| offset | changed | width | values at entry | new values | guess |')
                w('|---|---|---|---|---|---|')
                for r_ in rows:
                    w(r_)
                w('')
            var = [(o, ws) for o, ws in sorted(st['words'].items()) if not ws['changed'] and len(ws['entry']) > 1]
            if var:
                w('Varying, not written: ' + ', '.join(f'{glbl(o)} ({guess(sorted(ws["entry"]), 4, tables)})'
                                                       for o, ws in var[:16]) + '\n')
    if recs:
        r = recs[0]
        ex = {k: v for k, v in r.items() if k not in ('snap', '_run')}
        w('Example record (snapshot hex omitted):\n')
        w('```json\n' + json.dumps(ex)[:1500] + '\n```\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('runs', nargs='+')
    ap.add_argument('--build', default='build/diag')
    ap.add_argument('--out')
    ap.add_argument('--split')
    ap.add_argument('--no-layout', action='store_true')
    ap.add_argument('--symbols')
    ap.add_argument('--notes')
    a = ap.parse_args()
    tables = Tables(a.build)
    dsyms = DecompSyms(symbols_path(a.build, a.symbols), a.build)
    notes = json.load(open(a.notes)) if a.notes else {}
    cfg, agg, recs = load(a.runs)
    syms = []
    cfg_by = collections.defaultdict(list)
    for c in cfg:
        if 'sym' in c and 'id' in c:
            cfg_by[c['sym']].append(c)
            if c['sym'] not in syms:
                syms.append(c['sym'])
    id2sym = {c['id']: c['sym'] for c in cfg if 'id' in c}
    agg_by = collections.defaultdict(list)
    for (r, fid), o in agg.items():
        agg_by[id2sym.get(fid, o.get('fn_sym'))].append(o)
    rec_by = collections.defaultdict(list)
    for r in recs:
        rec_by[id2sym.get(r['fn'], r.get('fn_sym'))].append(r)
    head = [f'# Function traces: {", ".join(os.path.basename(r.rstrip("/")) for r in a.runs)}\n', NOTE + '\n']
    errs = [c for c in cfg if c.get('error')]
    for e in errs:
        head.append(f'- not traced: `{e.get("sym") or e.get("global")}`: {e["error"]}')
    if errs:
        head.append('')
    allout = list(head)
    for s in syms:
        out = []
        lay = None
        if any(o.get('words') for o in agg_by.get(s, [])) and not a.no_layout:
            lay = fntrace_layout.layout_for_sym(a.build, s)
        ptr_lays = {}
        for p in (cfg_by[s][0].get('ptr_args') or []) if not a.no_layout else []:
            if p.get('type') and not p.get('str'):
                ptr_lays[31 if p.get('arg') == 'ret' else p.get('arg')] = fntrace_layout.layout_for_type(a.build, s, p['type'])
        glob_lays = {}
        req = cfg_by[s][0].get('req')
        for g in cfg:
            if g.get('global') and g.get('type') and g.get('req') == req and not a.no_layout:
                glob_lays[g['global']] = fntrace_layout.layout_for_type(a.build, s, g['type'])
        summarize(s, cfg_by[s], agg_by.get(s, []), rec_by.get(s, []), tables, out, lay, ptr_lays, glob_lays)
        allout += out
        if a.split:
            os.makedirs(a.split, exist_ok=True)
            c0 = cfg_by[s][0]
            dname = dsyms.resolve(s, c0.get('port_sym'))
            safe = answer_key(s, dname)
            note = ' '.join(x for x in (notes.get(dname or '') or notes.get(s), notes.get('*')) if x)
            top = [one_line(dname, s, cfg_by[s], agg_by.get(s, []), note), '']
            if dname and dname != s:
                top.append(f'Decomp symbol `{dname}`; traced as `{s}`'
                           + (f' (port `{c0["port_sym"]}`)' if c0.get('port_sym') and c0['port_sym'] != dname else '')
                           + '.\n')
            elif not dname:
                top.append(f'No decomp symbol in symbols.txt for `{s}` (a port-only or inlined helper).\n')
            with open(os.path.join(a.split, safe + '.md'), 'w') as f:
                f.write('\n'.join(top + head + out) + '\n')
            with open(os.path.join(a.split, safe + '.jsonl'), 'w') as f:
                for r in rec_by.get(s, []):
                    f.write(json.dumps({k: v for k, v in r.items() if k != '_run'}) + '\n')
                for o in agg_by.get(s, []):
                    f.write(json.dumps(dict(o, record='agg')) + '\n')
    out = a.out or os.path.join(a.runs[0], 'fntrace_summary.md')
    with open(out, 'w') as f:
        f.write('\n'.join(allout) + '\n')
    print(f'{out}: {len(syms)} functions, {len(recs)} call records')


if __name__ == '__main__':
    main()
