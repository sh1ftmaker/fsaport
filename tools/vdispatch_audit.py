#!/usr/bin/env python3
"""Virtual dispatch audit (docs/DIAGNOSTICS.md, "Virtual dispatch audit"):
compare the virtual calls the port made with the original's vtables.

  tools/vdispatch_audit.py logs/diag/<run> [more runs] --build build/diag \\
      [--decomp ../fsadecomp/fsa-decomp] [--static] [--all] [--json out.json]

Inputs:
  runs        vdispatch.jsonl from an FSA_DIAG build (cumulative: the last
              record per edge is its total)
  --build     the build the runs came from: fsa.diag.tsv (table index -> decomp
              symbol) and fsa.diag.vt.tsv (the port's vtables, with their words)
  --decomp    the decomp checkout: config/G4SE01/symbols.txt (__vt__ objects,
              function addresses) and orig/sys/main.dol (the original's vtable
              contents: each slot is the address of the function it calls)

For every virtual call (the receiver's vtable is known and holds the target),
the port slot is mapped to the original's: both vtables are split into the
primary and secondary tables at their 2-word headers, clang's destructor pair
(complete + deleting) is counted as CodeWarrior's single __dt__ slot, and slots
are then matched by position. Per (class, slot) the original's function is
compared with the one the port called, by address in symbols.txt:

  MISSING-OVERRIDE  the original's slot holds the receiver class's own function,
                    the port called someone else's (a base implementation):
                    the decomp declares the override without `virtual`, or
                    not at all
  PORT-OVERRIDE     the reverse: the port called the class's own function where
                    the original keeps a base's
  DIFFERENT         another function than the original's (both not the class's)
  UNMAPPED          the port's function has no decomp symbol (port-written)
  LAYOUT            the vtables do not line up (slot counts differ); the slot
                    match is by position and may be off
  NO-ORIGINAL       the original has no __vt__ for this class (port-only class)

--static compares every port vtable that has an original counterpart, slot by
slot, whether or not the runs reached it, and marks the slots the runs called.
Exit status 1 when a run-reached slot is MISSING-OVERRIDE, PORT-OVERRIDE or
DIFFERENT.
"""
import argparse, collections, glob, json, os, re, struct, sys

DEFAULT_DECOMP = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'fsadecomp', 'fsa-decomp')
BAD = ('MISSING-OVERRIDE', 'PORT-OVERRIDE', 'DIFFERENT')


def unesc(s):
    return json.loads('"' + s + '"') if '\\' in s else s


def load_names(path):
    out = {}
    for line in open(path, encoding='utf-8', errors='replace'):
        f = line.rstrip('\n').split('\t')
        if len(f) >= 3 and f[0].isdigit():
            out[int(f[0])] = (unesc(f[1]), unesc(f[2]))
    return out


def load_vts(path):
    out = {}
    for line in open(path, encoding='utf-8', errors='replace'):
        f = line.rstrip('\n').split('\t')
        if len(f) >= 6:
            out[int(f[0])] = dict(addr=int(f[0]), size=int(f[1]), cls=unesc(f[2]), mw=f[3], sym=f[4],
                                  words=[int(w) for w in f[5].split(',') if w])
    return out


class Original:
    def __init__(self, decomp):
        self.addr, self.size, self.byaddr = {}, {}, collections.defaultdict(list)
        for line in open(os.path.join(decomp, 'config/G4SE01/symbols.txt')):
            m = re.match(r'(\S+) = \.(\w+):0x([0-9A-Fa-f]+);(.*)', line)
            if not m:
                continue
            a = int(m.group(3), 16)
            self.addr[m.group(1)] = a
            sz = re.search(r'size:0x([0-9A-Fa-f]+)', m.group(4))
            self.size[m.group(1)] = int(sz.group(1), 16) if sz else 0
            if 'type:function' in m.group(4):
                self.byaddr[a].append(m.group(1))
        dol = open(os.path.join(decomp, 'orig/sys/main.dol'), 'rb').read()
        self.secs = [(o, a, s) for o, a, s in zip(struct.unpack('>18I', dol[0:72]), struct.unpack('>18I', dol[72:144]),
                                                  struct.unpack('>18I', dol[144:216])) if s]
        self.dol = dol

    def word(self, a):
        for o, base, s in self.secs:
            if base <= a < base + s:
                return struct.unpack('>I', self.dol[o + a - base:o + a - base + 4])[0]
        return None

    def vtable(self, mw):
        """Words of __vt__X, or None. A class named after its vtable's address
        (cls_804AF860) whose vtable symbols.txt still calls lbl_804AF860 is
        found by that address."""
        if mw not in self.addr:
            m = re.search(r'_(80[0-9A-F]{6})$', mw)
            if not m or 'lbl_' + m.group(1) not in self.addr:
                return None
            mw = 'lbl_' + m.group(1)
        a, sz = self.addr[mw], self.size[mw]
        return [self.word(a + k) or 0 for k in range(0, sz, 4)]

    def fn_name(self, a):
        n = self.byaddr.get(a)
        return n[0] if n else (f'0x{a:08X}' if a else 'null')


def owner(mwsym):
    """The class qualifier of a CodeWarrior member symbol (as encoded), or ''."""
    m = re.match(r'^(.+?)__((?:Q\d)?\d+)', mwsym or '')
    if not m:
        return ''
    s = mwsym[len(m.group(1)) + 2:]
    if s.startswith('Q'):
        n, i = int(s[1]), 2
        for _ in range(n):
            d = re.match(r'\d+', s[i:])
            if not d:
                return ''
            ln = int(d.group())
            i += d.end() + ln
        return s[:i]
    d = re.match(r'\d+', s)
    return s[:d.end() + int(d.group())]


def segments(words, port):
    """Split a vtable group into [(start word index of the functions, [words])]."""
    segs, cur, start = [], [], 2
    i = 2
    while i < len(words):
        hdr = (i + 1 < len(words) and words[i + 1] == 0 and
               (words[i] >= 0x80000000 if port else words[i] == 0))
        if hdr:
            segs.append((start, cur))
            cur, start = [], i + 2
            i += 2
            continue
        cur.append(words[i])
        i += 1
    segs.append((start, cur))
    return segs


def is_dtor(m):
    return bool(re.search(r'D[012]Ev$', m or ''))


def collapse_port(seg, mangled, n_orig):
    """Port entries -> (list of entries per CodeWarrior slot, {port index: slot}).
    clang's destructor pair (D1 complete, D0 deleting) is one CodeWarrior slot;
    a D1 the port never defines is a 0 word next to the D0. A pure virtual
    destructor is two __cxa_pure_virtual words: those are merged only when that
    makes the slot count match the original's."""
    def run(merge_pure):
        out, idx, k = [], {}, 0
        while k < len(seg):
            a, b = seg[k], seg[k + 1] if k + 1 < len(seg) else None
            ma, mb = mangled.get(a, ''), mangled.get(b, '') if b is not None else ''
            pair = b is not None and (
                (is_dtor(ma) or a == 0) and (is_dtor(mb) or b == 0) and (a or b) or
                (merge_pure and ma == mb == '__cxa_pure_virtual' and not out))
            idx[k] = len(out)
            if pair:
                idx[k + 1] = len(out)
                out.append((a, b))
                k += 2
            else:
                out.append((a,))
                k += 1
        return out, idx
    out, idx = run(False)
    if n_orig is not None and len(out) != n_orig:
        o2, i2 = run(True)
        if len(o2) == n_orig:
            return o2, i2
    return out, idx


class Auditor:
    def __init__(self, names, vts, orig, mangled):
        self.names, self.vts, self.orig, self.mangled = names, vts, orig, mangled
        # clang symbol -> decomp symbol, for thunk targets
        self.mw_of = {mangled[t]: names[t][0] for t in names if t in mangled and names[t][0]}
        import diag_symtab
        self.mwcc = diag_symtab.mwcc_mangle
        self.cache = {}

    def sym(self, tid):
        return self.names.get(tid, ('', ''))[0]

    def name(self, tid):
        return self.names.get(tid, ('', ''))[1] or self.mangled.get(tid) or f'#{tid}'

    def port_syms(self, tid):
        """Decomp symbols a port vtable word stands for (a thunk: its target)."""
        s = self.sym(tid)
        if s:
            return [s]
        mg = self.mangled.get(tid, '')
        m = re.match(r'^_ZThn\d+_(.*)$', mg)
        if m and self.mw_of.get('_Z' + m.group(1)):
            return ['@thunk@' + self.mw_of['_Z' + m.group(1)]]
        if mg.startswith('_Z') and not m:
            # not instrumented (e.g. the deleting destructor of an abstract
            # class, which clang emits as a trap): mangle it ourselves
            try:
                mw = self.mwcc(mg)[0]
                if mw in self.orig.addr:
                    return [mw]
            except Exception:
                pass
        return []

    def layout(self, vt_addr):
        """Per port word index: (orig slot offset, orig function addr, layout ok,
        all port words of that slot) or None."""
        if vt_addr in self.cache:
            return self.cache[vt_addr]
        v = self.vts[vt_addr]
        ow = self.orig.vtable(v['mw'])
        res = None
        if ow is not None:
            res = {}
            ps = segments(v['words'], True)
            # The original's secondary-table headers are two null words, like
            # its pure virtual slots, so it is split by the port's slot counts
            # first; if its headers are not where those put them, by null pairs.
            coll = [collapse_port(pseg, self.mangled, None) for _, pseg in ps]
            os_, pos, fit = [], 2, True
            for si, (slots, _) in enumerate(coll):
                if si:
                    fit = fit and ow[pos:pos + 2] == [0, 0]
                    pos += 2
                os_.append((pos, ow[pos:pos + len(slots)]))
                pos += len(slots)
            fit = fit and pos <= len(ow) and not any(ow[pos:])
            if not fit:
                os_ = segments(ow, False)
            for si, (pstart, pseg) in enumerate(ps):
                if si >= len(os_):
                    break
                ostart, oseg = os_[si]
                slots, idx = collapse_port(pseg, self.mangled, len(oseg))
                if si == len(os_) - 1 and len(oseg) > len(slots) and not any(oseg[len(slots):]):
                    oseg = oseg[:len(slots)]  # dtk's size includes the alignment padding
                ok = fit or (len(slots) == len(oseg) and len(ps) == len(os_))
                for k, s in idx.items():
                    if s < len(oseg):
                        res[pstart + k] = ((ostart + s) * 4, oseg[s], ok, slots[s])
        self.cache[vt_addr] = res
        return res

    def by_name(self, v, tid):
        """When the layouts do not line up: look for the port's function, or a
        function with the same name and parameters, anywhere in the original's
        vtable. None if neither is there."""
        ps = [re.sub(r'^@thunk@', '', p) for p in self.port_syms(tid)]
        if not ps:
            return None
        ow = self.orig.vtable(v['mw']) or []
        onames = [(k * 4, re.sub(r'^@\d+@', '', n)) for k, w in enumerate(ow) if w for n in self.orig.byaddr.get(w, [])]
        pdesc = ' / '.join(ps)
        for off, n in onames:
            if n in ps:
                return 'OK-BYNAME', off, n, pdesc
        def key(m):
            o = owner(m)
            return m.replace('__' + o, '__', 1) if o else None
        cls = v['mw'][len('__vt__'):]
        pk = {key(p) for p in ps} - {None}
        for off, n in onames:
            if key(n) in pk:
                po, oo = {owner(p) for p in ps}, owner(n)
                st = 'MISSING-OVERRIDE' if oo == cls and cls not in po else 'PORT-OVERRIDE' if cls in po else 'DIFFERENT'
                return st + '?', off, n, pdesc
        return None

    def judge(self, v, word_index, tid):
        """(status, original slot offset, original function name, port function)."""
        lay = self.layout(v['addr'])
        if lay is None:
            return 'NO-ORIGINAL', None, '', self.sym(tid) or self.name(tid)
        if word_index not in lay or not lay[word_index][2]:
            r = self.by_name(v, tid)
            if r:
                return r
        if word_index not in lay:
            return 'LAYOUT', None, '', self.sym(tid) or self.name(tid)
        off, oaddr, ok, words = lay[word_index]
        onames = self.orig.byaddr.get(oaddr, [])
        oname = self.orig.fn_name(oaddr)
        psyms = [ps for w in words for ps in self.port_syms(w)]
        pdesc = ' / '.join(psyms) or ' / '.join(self.name(w) for w in words)
        for ps in psyms:
            if ps.startswith('@thunk@'):
                if any(re.sub(r'^@\d+@', '', o) == ps[7:] for o in onames):
                    return ('OK' if ok else 'OK-LAYOUT'), off, oname, pdesc
            elif self.orig.addr.get(ps) == oaddr:
                return ('OK' if ok else 'OK-LAYOUT'), off, oname, pdesc
            elif any(re.sub(r'^@\d+@', '', o) == ps for o in onames):  # original thunk, port direct
                return ('OK' if ok else 'OK-LAYOUT'), off, oname, pdesc
        if not psyms:
            pure = all(self.mangled.get(w) == '__cxa_pure_virtual' for w in words)
            if pure and oaddr == 0:  # CodeWarrior leaves pure virtual slots null
                return ('OK' if ok else 'OK-LAYOUT'), off, 'null (pure virtual)', pdesc
            return ('PURE' if pure else 'UNMAPPED'), off, oname, pdesc
        cls = v['mw'][len('__vt__'):]  # the class, encoded as in member symbols
        po = {owner(re.sub(r'^@thunk@', '', p)) for p in psyms}
        oo = owner(re.sub(r'^@\d+@', '', oname))
        if oo == cls and cls not in po:
            st = 'MISSING-OVERRIDE'
        elif cls in po and oo != cls:
            st = 'PORT-OVERRIDE'
        else:
            st = 'DIFFERENT'
        return (st if ok else st + '?'), off, oname, pdesc


def collapse_runs(runs):
    recs = {}
    for r in runs:
        for line in open(os.path.join(r, 'vdispatch.jsonl')):
            d = json.loads(line)
            d['run'] = os.path.basename(r.rstrip('/'))
            key = (d['run'], d['caller'], d['target'], d.get('entered'), d.get('vt_addr'), d.get('vptr_off'))
            recs[key] = d
    return list(recs.values())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('runs', nargs='*')
    ap.add_argument('--build', required=True)
    ap.add_argument('--decomp', default=DEFAULT_DECOMP)
    ap.add_argument('--static', action='store_true', help='also compare every port vtable, reached or not')
    ap.add_argument('--all', action='store_true', help='list OK slots too')
    ap.add_argument('--json', help='write all (class, slot) results here')
    a = ap.parse_args()

    names = load_names(os.path.join(a.build, 'fsa.diag.tsv'))
    vts = load_vts(os.path.join(a.build, 'fsa.diag.vt.tsv'))
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import diag_symtab
    table, wnames, _, _ = diag_symtab.parse_wasm(os.path.join(a.build, 'fsa.wasm'))
    mangled = {t: wnames.get(f, '') for t, f in table.items()}
    au = Auditor(names, vts, Original(a.decomp), mangled)
    recs = collapse_runs(a.runs)

    kinds = collections.Counter(r['kind'] for r in recs)
    calls = collections.Counter()
    for r in recs:
        calls[r['kind']] += r['calls']

    # (class vtable, port word index) -> aggregate
    slots = {}
    for r in recs:
        if r['kind'] != 'virtual':
            continue
        v = vts.get(r['vt_addr'])
        if v is None:
            continue
        wi = (r['vptr_off'] + r['slot']) // 4
        k = (r['vt_addr'], wi, r['target'])
        s = slots.get(k)
        if s is None:
            st, off, oname, pdesc = au.judge(v, wi, r['target'])
            s = slots[k] = dict(cls=v['cls'], vt=v['mw'], port_off=wi * 4, orig_off=off, status=st,
                                port=pdesc, port_name=au.name(r['target']),
                                orig=oname, calls=0, callers=collections.Counter(), first_scene=r['first_scene'],
                                first_frame=r['first_frame'], runs=set())
        s['calls'] += r['calls']
        s['callers'][r['caller_name'] or f"#{r['caller']}"] += r['calls']
        s['runs'].add(r['run'])
        if r['first_frame'] < s['first_frame'] and r['run'] in s['runs']:
            s['first_scene'], s['first_frame'] = r['first_scene'], r['first_frame']

    st_count = collections.Counter(s['status'] for s in slots.values())
    print(f"{len(recs)} edges in {len(a.runs)} run(s): " + ', '.join(f'{k} {kinds[k]} ({calls[k]} calls)' for k in sorted(kinds)))
    print(f"{len(slots)} (class, slot, target) reached by virtual calls: " +
          ', '.join(f'{k} {v}' for k, v in sorted(st_count.items())))

    def show(s, reached=True):
        sc = s.get('first_scene') or {}
        where = f"mode {sc.get('mode')} stage {sc.get('stage')} level {sc.get('level')}" if sc else ''
        off = f"+0x{s['orig_off']:X}" if s['orig_off'] is not None else '?'
        print(f"  {s['status']:<17} {s['vt']} {off} (port +0x{s['port_off']:X})"
              + (f"  calls {s['calls']}  first: {where}" if reached else '  (not reached)'))
        print(f"      original {s['orig']}")
        print(f"      port     {s['port']}")
        if reached:
            top = ', '.join(f'{c} x{n}' for c, n in s['callers'].most_common(2))
            print(f"      via      {top}")

    bad = [s for s in slots.values() if s['status'].rstrip('?') in BAD]
    order = lambda s: (s['status'], s['vt'], s['port_off'])
    print('\nReached slots where the port dispatches differently from the original:')
    for s in sorted(bad, key=order):
        show(s)
    if not bad:
        print('  none')
    other = [s for s in slots.values() if s['status'] not in ('OK', 'OK-LAYOUT', 'OK-BYNAME') and s not in bad]
    if other:
        print('\nReached slots that could not be checked:')
        for s in sorted(other, key=order):
            if a.all or s['status'] != 'NO-ORIGINAL':
                show(s)
        n_no = sum(1 for s in other if s['status'] == 'NO-ORIGINAL')
        if n_no and not a.all:
            cls = sorted({s['cls'] for s in other if s['status'] == 'NO-ORIGINAL'})
            print(f"  NO-ORIGINAL: {n_no} slots in {len(cls)} port-only classes ({', '.join(cls[:8])}{', ...' if len(cls) > 8 else ''})")
    if a.all:
        print('\nOK:')
        for s in sorted((s for s in slots.values() if s['status'].startswith('OK')), key=order):
            show(s)

    static = []
    if a.static:
        reached = {(k[0], k[1]) for k in slots}
        n_vt = n_cmp = 0
        for addr, v in sorted(vts.items()):
            lay = au.layout(addr)
            if lay is None:
                continue
            n_vt += 1
            seen = set()
            for wi in sorted(lay):
                if lay[wi][0] in seen:  # the second word of a destructor pair
                    continue
                seen.add(lay[wi][0])
                tid = v['words'][wi]
                n_cmp += 1
                st, off, oname, pdesc = au.judge(v, wi, tid)
                if st in ('OK', 'OK-LAYOUT', 'OK-BYNAME', 'PURE') and not a.all:
                    continue
                static.append(dict(cls=v['cls'], vt=v['mw'], port_off=wi * 4, orig_off=off, status=st,
                                   port=pdesc, port_name=au.name(tid), orig=oname,
                                   reached=any((addr, w) in reached for w in lay if lay[w][0] == off)))
        sc = collections.Counter(s['status'] for s in static)
        print(f'\nStatic: {n_vt} port vtables with an original, {n_cmp} slots compared: ' +
              (', '.join(f'{k} {v}' for k, v in sorted(sc.items())) or 'all match'))
        for s in sorted(static, key=order):
            if s['status'].rstrip('?') in BAD or a.all:
                print(f"  {s['status']:<17} {s['vt']} +0x{s['orig_off']:X}{'  [reached]' if s['reached'] else ''}")
                print(f"      original {s['orig']}\n      port     {s['port']}")

    if a.json:
        def clean(s):
            s = dict(s)
            if 'callers' in s:
                s['callers'] = dict(s['callers'].most_common(5))
                s['runs'] = sorted(s['runs'])
            return s
        with open(a.json, 'w') as f:
            json.dump({'reached': [clean(s) for s in sorted(slots.values(), key=order)],
                       'static': static, 'edges': dict(kinds)}, f, indent=1)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
