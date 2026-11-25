#!/usr/bin/env python3
"""Name table for the diagnostics build (docs/DIAGNOSTICS.md).

    python3 tools/diag_symtab.py --build build/diag --source . \
        --symbols ../fsadecomp/fsa-decomp/config/G4SE01/symbols.txt \
        --llvm-bin $EMSDK/upstream/bin --out build/diag/fsa.diag.tsv

-finstrument-functions hands platform/diag_cov.cpp each function's wasm
table index. This reads fsa.wasm (element section: table index -> function
index; name section: function index -> clang symbol; code section: body
sizes), the unit objects (llvm-nm: which unit defines each symbol), and the
decomp's symbols.txt, and writes one line per instrumented function:

    id \t sym \t name \t kind \t match \t unit

  sym    the decomp's symbol (symbols.txt spelling, CodeWarrior mangling), or ""
  name   the clang symbol demangled (llvm-cxxfilt)
  kind   "" game code; "stub" (platform/hw_stubs.cpp, or a platform function
         whose body is empty / returns a constant); "missing" (written by the
         port in game/src/port/missing_functions.cpp); "asm-only" (the decomp
         body is MWCC asm with no portable #else: compiled to nothing);
         "asm-partial" (some of the body is such asm); "empty" (a game
         function whose port body compiled to nothing but the original's is
         larger than a return); "platform" (the port's replacement for SDK code)
  match  how sym was found: "exact" (clang symbol re-mangled the CodeWarrior
         way, found verbatim), "c" (unmangled C name), "name" (same qualified
         name, parameters differ: unique in symbols.txt), "addr" (the name
         carries the original's address, e.g. fn_8023FBF0, and symbols.txt has
         a function there), "ambiguous" (several candidates: sym left empty),
         "none"
  unit   the source file that defines the function (strong definition), or
         "inline:<file>" for an inline / template function kept from <file>

Strings are JSON-escaped (the runtime splices them into JSON lines). Also
writes <out>.report.json: mapping statistics and the unmapped functions.
"""
import argparse, bisect, collections, glob, json, os, re, subprocess, sys


# --- wasm ------------------------------------------------------------------------
def leb(b, i):
    r = s = 0
    while True:
        x = b[i]; i += 1
        r |= (x & 0x7F) << s; s += 7
        if x < 0x80:
            return r, i


def sleb(b, i):
    r = s = 0
    while True:
        x = b[i]; i += 1
        r |= (x & 0x7F) << s; s += 7
        if x < 0x80:
            if x & 0x40:
                r -= 1 << s
            return r, i


def skip_init_expr(b, i):
    # i32.const N end, or global.get N end
    op = b[i]; i += 1
    if op == 0x41:
        v, i = sleb(b, i)
    elif op == 0x23:
        v, i = leb(b, i)
        v = 0
    else:
        raise ValueError(f'init expr op {op:#x}')
    assert b[i] == 0x0B
    return v, i + 1


def parse_wasm(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'\0asm'
    i = 8
    nimport_funcs = 0
    table = {}        # table index -> function index
    names = {}        # function index -> name
    bodies = {}       # function index -> body size (bytes)
    while i < len(b):
        sid = b[i]; i += 1
        size, i = leb(b, i)
        end = i + size
        if sid == 2:  # imports
            n, j = leb(b, i)
            for _ in range(n):
                l, j = leb(b, j); j += l
                l, j = leb(b, j); j += l
                kind = b[j]; j += 1
                if kind == 0:
                    _, j = leb(b, j); nimport_funcs += 1
                elif kind == 1:
                    j += 1; fl, j = leb(b, j); _, j = leb(b, j)
                    if fl & 1: _, j = leb(b, j)
                elif kind == 2:
                    fl, j = leb(b, j); _, j = leb(b, j)
                    if fl & 1: _, j = leb(b, j)
                elif kind == 3:
                    j += 2
                elif kind == 4:
                    j += 1; _, j = leb(b, j)
        elif sid == 9:  # elements
            n, j = leb(b, i)
            for _ in range(n):
                fl, j = leb(b, j)
                if fl in (0, 2, 4, 6):
                    if fl in (2, 6):
                        _, j = leb(b, j)  # table index
                    off, j = skip_init_expr(b, j)
                    if fl == 2:
                        j += 1  # elemkind
                    elif fl == 6:
                        j += 1  # reftype
                    cnt, j = leb(b, j)
                    for k in range(cnt):
                        if fl in (0, 2):
                            f, j = leb(b, j)
                        else:  # expr: ref.func f end
                            assert b[j] == 0xD2
                            f, j = leb(b, j + 1)
                            assert b[j] == 0x0B
                            j += 1
                        table[off + k] = f
                else:
                    raise ValueError(f'element segment flags {fl}')
        elif sid == 10:  # code
            n, j = leb(b, i)
            for k in range(n):
                sz, j = leb(b, j)
                bodies[nimport_funcs + k] = sz
                j += sz
        elif sid == 0:
            l, j = leb(b, i)
            nm = b[j:j + l].decode('utf-8', 'replace'); j += l
            if nm == 'name':
                while j < end:
                    sub = b[j]; j += 1
                    ssz, j = leb(b, j)
                    send = j + ssz
                    if sub == 1:
                        cnt, k = leb(b, j)
                        for _ in range(cnt):
                            idx, k = leb(b, k)
                            l, k = leb(b, k)
                            names[idx] = b[k:k + l].decode('utf-8', 'replace'); k += l
                    j = send
        i = end
    return table, names, bodies, nimport_funcs


# --- Itanium -> CodeWarrior mangling ------------------------------------------------
BUILTIN = {'v': 'v', 'b': 'b', 'c': 'c', 'a': 'Sc', 'h': 'Uc', 's': 's', 't': 'Us', 'i': 'i', 'j': 'Ui',
           'l': 'l', 'm': 'Ul', 'x': 'x', 'y': 'Ux', 'f': 'f', 'd': 'd', 'e': 'r', 'w': 'w', 'z': 'e',
           'n': 'n', 'o': 'Un', 'g': 'g'}
OPS = {'nw': '__nw', 'na': '__nwa', 'dl': '__dl', 'da': '__dla', 'ps': '__pl', 'ng': '__mi', 'ad': '__ad',
       'de': '__ml', 'co': '__co', 'pl': '__pl', 'mi': '__mi', 'ml': '__ml', 'dv': '__dv', 'rm': '__md',
       'an': '__ad', 'or': '__or', 'eo': '__er', 'aS': '__as', 'pL': '__apl', 'mI': '__ami', 'mL': '__amu',
       'dV': '__adv', 'rM': '__amd', 'aN': '__aad', 'oR': '__aor', 'eO': '__aer', 'ls': '__ls', 'rs': '__rs',
       'lS': '__als', 'rS': '__ars', 'eq': '__eq', 'ne': '__ne', 'lt': '__lt', 'gt': '__gt', 'le': '__le',
       'ge': '__ge', 'nt': '__nt', 'aa': '__aa', 'oo': '__oo', 'pp': '__pp', 'mm': '__mm', 'cm': '__cm',
       'pm': '__rm', 'pt': '__rf', 'cl': '__cl', 'ix': '__vc'}


class Fail(Exception):
    pass


class Itanium:
    """Just enough of the Itanium C++ ABI demangler to re-mangle function
    symbols the CodeWarrior way. Types are tuples:
      ('b', code) builtin, ('cls', [comp...]) class/enum, ('P'|'R'|'O', t),
      ('cv', 'CV', t), ('fn', ret, [params]), ('arr', n, t), ('mp', cls, t)
    A name component is (identifier, targs or None)."""

    def __init__(self, s):
        self.s = s
        self.i = 0
        self.subs = []
        self.tparams = []

    def peek(self, n=1):
        return self.s[self.i:self.i + n]

    def take(self, n=1):
        r = self.s[self.i:self.i + n]
        if len(r) < n:
            raise Fail('eof')
        self.i += n
        return r

    def num(self):
        m = re.match(r'n?\d+', self.s[self.i:])
        if not m:
            raise Fail('number')
        self.i += m.end()
        return int(m.group().replace('n', '-'))

    def source_name(self):
        n = self.num()
        r = self.take(n)
        if r.startswith('_GLOBAL__N'):
            r = '(anonymous namespace)'
        return r

    def seq_id(self):
        if self.peek() == '_':
            self.i += 1
            return 0
        m = re.match(r'[0-9A-Z]+', self.s[self.i:])
        if not m or self.s[self.i + m.end()] != '_':
            raise Fail('seq-id')
        self.i += m.end() + 1
        return int(m.group(), 36) + 1

    def substitution(self):
        # at 'S'
        self.take()
        c = self.peek()
        std = {'t': [('std', None)], 'a': [('std', None), ('allocator', None)],
               'b': [('std', None), ('basic_string', None)], 's': [('std', None), ('string', None)]}
        if c in std:
            self.i += 1
            return ('name', std[c])
        k = self.seq_id()
        if k >= len(self.subs):
            raise Fail('substitution out of range')
        return self.subs[k]

    def template_param(self):
        self.take()  # T
        k = self.seq_id()
        t = self.tparams[k] if k < len(self.tparams) else ('b', '?')
        return t

    def template_args(self):
        self.take()  # I
        args = []
        while self.peek() != 'E':
            args.append(self.template_arg())
        self.take()
        return args

    def template_arg(self):
        c = self.peek()
        if c == 'L':
            self.take()
            if self.peek(2) == '_Z':
                raise Fail('external-name literal')
            t = self.type()
            v = self.num() if self.peek() != 'E' else 0
            self.take()  # E
            return ('lit', t, v)
        if c == 'X':
            raise Fail('expression template arg')
        if c == 'J':
            self.take()
            out = []
            while self.peek() != 'E':
                out.append(self.template_arg())
            self.take()
            return ('pack', out)
        return self.type()

    def unqualified(self):
        c = self.peek()
        if c.isdigit():
            return (self.source_name(), None)
        if c == 'C' and self.peek(2)[1:] in '123':
            self.i += 2
            return ('__ct', None)
        if c == 'D' and self.peek(2)[1:] in '012':
            self.i += 2
            return ('__dt', None)
        if c == 'L':  # internal linkage
            self.i += 1
            return self.unqualified()
        op = self.peek(2)
        if op == 'cv':
            self.i += 2
            t = self.type()
            return ('__op' + mw_type(t), None)
        if op in OPS:
            self.i += 2
            return (OPS[op], None)
        raise Fail(f'unqualified {self.s[self.i:self.i + 4]}')

    def nested(self):
        self.take()  # N
        quals = ''
        while self.peek() in ('K', 'V', 'r'):
            quals += self.take()
        if self.peek() in ('R', 'O'):
            self.take()
        comps = []
        while self.peek() != 'E':
            c = self.peek()
            if c == 'S':
                sub = self.substitution()
                if sub[0] != 'name':
                    raise Fail('non-name substitution in prefix')
                comps = list(sub[1])
                continue
            if c == 'T':
                t = self.template_param()
                if t[0] != 'cls':
                    raise Fail('template param prefix')
                comps = list(t[1])
                self.subs.append(('name', list(comps)))
                continue
            if c == 'I':
                if not comps:
                    raise Fail('targs without name')
                args = self.template_args()
                comps[-1] = (comps[-1][0], args)
                if self.peek() != 'E':
                    self.subs.append(('name', list(comps)))
                continue
            if c == 'M':  # data-member-prefix (lambdas in initialisers)
                self.i += 1
                continue
            comps.append(self.unqualified())
            if self.peek() != 'E':
                self.subs.append(('name', list(comps)))
        self.take()
        return comps, quals

    def name(self):
        """-> (components, cv-quals of a member function, is_nested)"""
        c = self.peek()
        if c == 'N':
            comps, quals = self.nested()
            return comps, quals
        if c == 'Z':
            raise Fail('local name')
        if c == 'S' and self.peek(2) != 'St':
            sub = self.substitution()
            if sub[0] != 'name':
                raise Fail('substitution as name')
            comps = list(sub[1])
            if self.peek() == 'I':
                args = self.template_args()
                comps[-1] = (comps[-1][0], args)
            return comps, ''
        if self.peek(2) == 'St':
            self.i += 2
            comps = [('std', None), self.unqualified()]
        else:
            comps = [self.unqualified()]
        if self.peek() == 'I':
            self.subs.append(('name', list(comps)))
            args = self.template_args()
            comps[-1] = (comps[-1][0], args)
        return comps, ''

    def type(self):
        c = self.peek()
        if c in BUILTIN:
            self.i += 1
            return ('b', BUILTIN[c])
        if c == 'D':
            c2 = self.peek(2)
            self.i += 2
            m = {'Dn': ('b', 'n'), 'Ds': ('b', 'Us'), 'Di': ('b', 'Ui'), 'Du': ('b', 'Uc')}
            if c2 in m:
                return m[c2]
            raise Fail(f'D type {c2}')
        if c in 'PRO':
            self.i += 1
            t = ('P' if c == 'P' else 'R', self.type())
            self.subs.append(('type', t))
            return t
        if c in 'KVr':
            q = ''
            while self.peek() in ('K', 'V', 'r'):
                q += self.take()
            t = ('cv', q, self.type())
            self.subs.append(('type', t))
            return t
        if c == 'F':
            self.i += 1
            if self.peek() == 'Y':
                self.i += 1
            ret = self.type()
            params = []
            while self.peek() not in ('E', 'R', 'O'):
                params.append(self.type())
            if self.peek() in ('R', 'O'):
                self.i += 1
            self.take()
            t = ('fn', ret, params)
            self.subs.append(('type', t))
            return t
        if c == 'A':
            self.i += 1
            n = self.num() if self.peek().isdigit() else 0
            self.take()  # _
            t = ('arr', n, self.type())
            self.subs.append(('type', t))
            return t
        if c == 'M':
            self.i += 1
            cls = self.type()
            mt = self.type()
            t = ('mp', cls, mt)
            self.subs.append(('type', t))
            return t
        if c == 'T':
            t = self.template_param()
            self.subs.append(('type', t))
            if self.peek() == 'I':
                args = self.template_args()
                if t[0] == 'cls':
                    t = ('cls', t[1][:-1] + [(t[1][-1][0], args)])
                self.subs.append(('type', t))
            return t
        if c == 'S':
            if self.peek(2) == 'St':
                self.i += 2
                comps = [('std', None), self.unqualified()]
                self.subs.append(('name', list(comps)))
            else:
                sub = self.substitution()
                if sub[0] == 'type':
                    if self.peek() == 'I':
                        raise Fail('template on type substitution')
                    return sub[1]
                comps = list(sub[1])
                if self.peek() != 'I':
                    return ('cls', comps)
            if self.peek() == 'I':
                args = self.template_args()
                comps[-1] = (comps[-1][0], args)
                self.subs.append(('name', list(comps)))
            return ('cls', comps)
        if c == 'N':
            comps, _ = self.nested()
            self.subs.append(('name', list(comps)))
            return ('cls', comps)
        if c.isdigit() or c == 'Z':
            if c == 'Z':
                raise Fail('local type')
            comps = [self.unqualified()]
            self.subs.append(('name', list(comps)))
            if self.peek() == 'I':
                args = self.template_args()
                comps[-1] = (comps[-1][0], args)
                self.subs.append(('name', list(comps)))
            return ('cls', comps)
        raise Fail(f'type {self.s[self.i:self.i + 4]}')

    def function(self):
        """_Z<encoding> -> (components, quals, params, is_template_fn)"""
        if not self.s.startswith('_Z'):
            raise Fail('not mangled')
        self.i = 2
        if self.peek() in ('T', 'G') and self.peek(2) in ('TV', 'TI', 'TS', 'Th', 'Tv', 'Tc', 'GV', 'GR', 'TW', 'TH'):
            raise Fail('special name')
        comps, quals = self.name()
        is_tmpl = comps[-1][1] is not None
        if is_tmpl:
            self.tparams = [a for a in comps[-1][1]]
            ret = self.type()
        params = []
        while self.i < len(self.s) and self.peek() != '.':
            params.append(self.type())
        return comps, quals, params, is_tmpl


def mw_targ(a):
    if a[0] == 'lit':
        return str(a[2])
    if a[0] == 'pack':
        return ','.join(mw_targ(x) for x in a[1])
    return mw_type(a)


# CodeWarrior names an anonymous namespace after its file: @unnamed@foo_cpp@
ANON = ['@unnamed@']


def mw_comp(c):
    ident, args = c
    if ident == '(anonymous namespace)':
        ident = ANON[0]
    if args is not None:
        ident = ident + '<' + ','.join(mw_targ(a) for a in args) + '>'
    return ident


def mw_qual(comps):
    parts = [mw_comp(c) for c in comps]
    enc = ''.join(f'{len(p)}{p}' for p in parts)
    return enc if len(parts) == 1 else f'Q{len(parts)}{enc}'


def mw_cv(q):
    return ('C' if 'K' in q else '') + ('V' if 'V' in q else '')


def mw_type(t):
    k = t[0]
    if k == 'b':
        return t[1]
    if k == 'cls':
        return mw_qual(t[1])
    if k == 'P':
        return 'P' + mw_type(t[1])
    if k == 'R':
        return 'R' + mw_type(t[1])
    if k == 'cv':
        return mw_cv(t[1]) + mw_type(t[2])
    if k == 'fn':
        return 'F' + mw_params(t[2]) + '_' + mw_type(t[1])
    if k == 'arr':
        return f'A{t[1]}_' + mw_type(t[2])
    if k == 'mp':
        return 'M' + mw_type(t[1]) + mw_type(t[2])
    raise Fail(f'type kind {k}')


def mw_params(ps):
    if not ps or (len(ps) == 1 and ps[0] == ('b', 'v')):
        return 'v'
    # top-level cv on a by-value parameter is not part of the signature
    return ''.join(mw_type(p[2] if p[0] == 'cv' else p) for p in ps)


def mwcc_mangle(sym, unit=''):
    """clang (Itanium) symbol -> (CodeWarrior symbol, 'Class::name' key)"""
    ANON[0] = '@unnamed@' + os.path.basename(unit).replace('.', '_') + '@'
    p = Itanium(sym)
    comps, quals, params, is_tmpl = p.function()
    base = mw_comp(comps[-1])
    scope = comps[:-1]
    out = base + '__'
    if scope:
        out += mw_qual(scope)
    out += mw_cv(quals) + 'F' + mw_params(params)
    key = '::'.join(mw_comp(c) for c in comps)
    return out, key


def mwcc_key(sym):
    """symbols.txt name -> 'Class::name' (qualified name without parameters)"""
    # the base name may itself start with __ (operators, ctor/dtor: __ct__3FooFv)
    m = re.match(r'^(__[a-z]+|.+?)__(?=Q\d|\d|C?F)(.*)$', sym)
    if not m:
        return None
    base, rest = m.groups()
    scope = []
    if rest.startswith('Q'):
        n = int(rest[1])
        j = 2
        for _ in range(n):
            mm = re.match(r'\d+', rest[j:])
            if not mm:
                return None
            ln = int(mm.group()); j += mm.end()
            scope.append(rest[j:j + ln]); j += ln
    elif rest[:1].isdigit():
        mm = re.match(r'\d+', rest)
        ln = int(mm.group())
        scope.append(rest[mm.end():mm.end() + ln])
    elif not rest.startswith(('F', 'C')):
        return None
    return '::'.join(scope + [base])


# --- symbols.txt -------------------------------------------------------------------
def load_symbols(path):
    syms = {}       # name -> (addr, size, scope)
    byaddr = {}     # addr -> [names] (functions)
    bykey = collections.defaultdict(list)
    for line in open(path):
        m = re.match(r'^(\S+) = \.(\w+):0x([0-9A-Fa-f]+); // type:function size:0x([0-9A-Fa-f]+)(.*)', line)
        if not m:
            continue
        name, sect, addr, size, rest = m.groups()
        addr = int(addr, 16)
        sc = re.search(r'scope:(\w+)', rest)
        syms.setdefault(name, (addr, int(size, 16), sc.group(1) if sc else 'global'))
        byaddr.setdefault(addr, []).append(name)
        k = mwcc_key(name)
        if k:
            bykey[k].append(name)
        else:
            bykey[name].append(name)
    return syms, byaddr, bykey


# --- sources: asm-only bodies, platform stubs ---------------------------------------
def strip_comments(text):
    out = re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group()), text)
    out = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group()), out, flags=re.S)
    out = re.sub(r'"(\\.|[^"\\\n])*"', lambda m: '"' + ' ' * (len(m.group()) - 2) + '"', out)
    return out


HEADER_RE = re.compile(r'([A-Za-z_~][\w:~<>,\s\*&]*?)\s*\(([^()]*(\([^()]*\)[^()]*)*)\)\s*(const)?\s*(noexcept)?\s*$', re.S)


def functions_in(text):
    """Top-level function definitions: [(qualified name, header line, body start, body end)].
    Brace blocks that are namespaces / extern "C" / classes are descended into."""
    t = strip_comments(text)
    # drop preprocessor lines (keep line count)
    t = re.sub(r'^[ \t]*#[^\n]*(\\\n[^\n]*)*', lambda m: re.sub(r'[^\n]', ' ', m.group()), t, flags=re.M)
    res = []
    stack = []  # 'scope' or 'other'
    last = 0
    i = 0
    n = len(t)
    while i < n:
        ch = t[i]
        if ch == '{':
            head = t[last:i]
            if stack and stack[-1] != 'scope':
                stack.append('other')
            else:
                h = head.strip()
                if re.search(r'(\b(namespace|struct|class|union|enum)\b[^;()]*|\bextern\s*"\s*")$', h) or h == '':
                    stack.append('scope')
                elif re.search(r'=\s*$', h) or not h.endswith((')', 'const', 'noexcept')) and not re.search(r'\)\s*(const\s*)?(noexcept\s*)?(:\s*[^{]*)?$', h, re.S):
                    stack.append('other')
                else:
                    # find the matching close brace
                    depth = 1
                    j = i + 1
                    while j < n and depth:
                        if t[j] == '{':
                            depth += 1
                        elif t[j] == '}':
                            depth -= 1
                        j += 1
                    hh = re.sub(r'\)\s*:[^{]*$', ')', h, flags=re.S)  # ctor initialiser list
                    # the declarator name is the last identifier before the parameter list
                    names = re.findall(r'((?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*|operator\s*\S+?)\s*\(', hh)
                    if names:
                        name = names[0] if not names[0] in ('__attribute__', 'alignas', 'decltype') else (names[1] if len(names) > 1 else None)
                        if name:
                            line = t.count('\n', 0, i) + 1
                            res.append((name, line, i, j))
                    i = j
                    last = j
                    continue
            last = i + 1
        elif ch == '}':
            if stack:
                stack.pop()
            last = i + 1
        elif ch == ';':
            last = i + 1
        i += 1
    return res, t


def body_kind(body):
    b = re.sub(r'\s+', ' ', body.strip('{} \n\t'))
    if b == '' or b == 'return;':
        return 'empty'
    if re.fullmatch(r'return\s*(\(\w+\)\s*)?(-?\d[\w.]*|true|false|nullptr|NULL|TRUE|FALSE|[A-Z][A-Z0-9_]+)\s*;', b):
        return 'const'
    return None


def asm_hits(path):
    """Lines of '#ifdef __MWERKS__' blocks with asm and no #else (tools/asm_scan logic)."""
    hits = []
    L = open(path, errors='replace').read().split('\n')
    stack = []
    for i, l in enumerate(L):
        s = l.strip()
        m = re.match(r'#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', s)
        if m:
            k, rest = m.groups()
            if k in ('if', 'ifdef', 'ifndef'):
                mw = k != 'ifndef' and '__MWERKS__' in rest and '!' not in rest
                stack.append([i, mw, False, False])
            elif k in ('else', 'elif') and stack:
                stack[-1][2] = True
            elif k == 'endif' and stack:
                st, mw, hasel, asm = stack.pop()
                if mw and asm and not hasel:
                    hits.append((st + 1, i + 1))
            continue
        if stack and stack[-1][1] and not stack[-1][2] and re.search(r'\basm\b|psq_|ps_|frsqrte', s):
            stack[-1][3] = True
    return hits


def jesc(s):
    return json.dumps(s)[1:-1].replace('\t', '\\t')


def short(qual):
    return re.sub(r'^\(anonymous namespace\)::', '', qual)


def qualname(d):
    # 'A::B<int>::f(int) const' -> 'A::B<int>::f'
    depth = 0
    for i, ch in enumerate(d):
        if ch in '<':
            depth += 1
        elif ch == '>':
            depth -= 1
        elif ch == '(' and depth == 0 and i > 0 and not d[:i].endswith('operator'):
            return d[:i]
    return d


def join_sym(nm, q, file, syms, byaddr, bykey):
    """The join key (docs/DIAGNOSTICS.md): clang symbol nm, its demangled
    qualified name q (short(qualname(demangled))), the unit file ->
    (sym, match, mwcc_guess). Also used by tools/coverage_report.py."""
    # --- join key
    sym, match, mw = '', 'none', None
    if not nm.startswith('_Z'):
        if nm in syms:
            sym, match = nm, 'c'
        # static initialisers: _GLOBAL__sub_I_foo.cpp -> __sinit_foo_cpp
        m = re.match(r'_GLOBAL__sub_I_(.+)$', nm)
        if m and '__sinit_' + m.group(1).replace('.', '_') in syms:
            sym, match = '__sinit_' + m.group(1).replace('.', '_'), 'name'
    else:
        try:
            mw, key = mwcc_mangle(nm, file)
            if mw in syms:
                sym, match = mw, 'exact'
        except Fail:
            mw, key = None, q
        except (IndexError, ValueError, RecursionError):
            mw, key = None, q
        if not sym:
            cands = bykey.get(key) or bykey.get(q) or []
            if len(cands) == 1:
                sym, match = cands[0], 'name'
            elif len(cands) > 1:
                match = 'ambiguous'
    if not sym:
        # The decomp's names carry the original's address (fn_8023FBF0,
        # moveTempo_80458508): accept the function symbols.txt has there
        # if it is a member of the same scope. Not for ctors/dtors (their
        # name is the class's) or inl_* (pieces inlined into that function).
        parts = q.split('::')
        leaf = parts[-1]
        am = re.search(r'_(8[0-9A-Fa-f]{7})(?![0-9A-Fa-f])', leaf)
        if am and not leaf.startswith(('inl_', '~')) and not (len(parts) > 1 and leaf == parts[-2]):
            addr = int(am.group(1), 16)
            scope = '::'.join(parts[:-1])
            for c in byaddr.get(addr, []):
                ck = (mwcc_key(c) or c).split('::')
                if '::'.join(ck[:-1]) == scope:
                    sym, match = c, 'addr'
                    break
    return sym, match, mw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', required=True)
    ap.add_argument('--source', required=True)
    ap.add_argument('--symbols')
    ap.add_argument('--llvm-bin', default='')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    tool = lambda n: os.path.join(a.llvm_bin, n) if a.llvm_bin and os.path.exists(os.path.join(a.llvm_bin, n)) else n

    table, wnames, bodies, nimp = parse_wasm(os.path.join(a.build, 'fsa.wasm'))

    # unit of each symbol: strong (T) definitions name their unit; weak (W) ones are inline/template
    objs = sorted(glob.glob(os.path.join(a.build, 'CMakeFiles', '*.dir', '**', '*.o'), recursive=True))
    strong, weak = {}, {}
    instrumented_units = set()
    for k in range(0, len(objs), 200):
        out = subprocess.run([tool('llvm-nm'), '--defined-only', '-A'] + objs[k:k + 200], capture_output=True, text=True).stdout
        for line in out.splitlines():
            m = re.match(r'^(.*?\.o):\s*\S*\s+([TtWw])\s+(\S+)$', line)
            if not m:
                continue
            obj, typ, sym = m.groups()
            rel = re.sub(r'^.*?CMakeFiles/[^/]+\.dir/', '', obj)[:-2]
            if typ in 'Tt':
                strong.setdefault(sym, rel)
            else:
                weak.setdefault(sym, rel)

    syms, byaddr, bykey = load_symbols(a.symbols) if a.symbols and os.path.exists(a.symbols) else ({}, {}, {})

    # instrumented functions: table entries whose function is defined in our objects
    entries = []
    for tidx, fidx in sorted(table.items()):
        nm = wnames.get(fidx)
        if not nm:
            continue
        unit = strong.get(nm) or (('inline:' + weak[nm]) if nm in weak else None)
        if unit is None:
            continue
        if unit.split(':')[-1].startswith('extern/'):
            continue
        entries.append((tidx, fidx, nm, unit))

    dem = subprocess.run([tool('llvm-cxxfilt')], input='\n'.join(e[2] for e in entries) + '\n',
                         capture_output=True, text=True).stdout.split('\n')

    # kinds from the sources
    src = a.source
    kinds = {}  # (unit, name-or-qualname) -> kind
    unit_funcs = {}

    def funcs_of(rel):
        if rel not in unit_funcs:
            p = os.path.join(src, rel)
            try:
                unit_funcs[rel] = functions_in(open(p, errors='replace').read())
            except OSError:
                unit_funcs[rel] = ([], '')
        return unit_funcs[rel]

    asm_units = {}
    for rel in sorted(set(e[3].split(':')[-1] for e in entries)):
        if rel.endswith(('.c', '.cpp')) and rel.startswith('game/'):
            p = os.path.join(src, rel)
            if os.path.exists(p):
                h = asm_hits(p)
                if h:
                    asm_units[rel] = h

    recs = []
    a_out = a.out
    min_body = min((bodies.get(e[1], 1 << 30) for e in entries), default=0)
    for (tidx, fidx, nm, unit), d in zip(entries, dem):
        d = d.strip() or nm
        q = short(qualname(d))
        file = unit.split(':')[-1]
        sym, match, mw = join_sym(nm, q, file, syms, byaddr, bykey)
        # --- kind
        kind = ''
        if file == 'platform/hw_stubs.cpp':
            kind = 'stub'
        elif file == 'game/src/port/missing_functions.cpp':
            kind = 'missing'
        elif file.startswith('platform/') and not unit.startswith('inline:'):
            kind = 'platform'
            fl, text = funcs_of(file)
            leaf = q.split('::')[-1]
            for (fname, line, b0, b1) in fl:
                if fname.split('::')[-1] == leaf and (fname == q or q.endswith('::' + fname) or fname == leaf):
                    if body_kind(text[b0:b1]):
                        kind = 'stub'
                    break
        if file in asm_units and not unit.startswith('inline:'):
            fl, text = funcs_of(file)
            leaf = q.split('::')[-1]
            for (fname, line, b0, b1) in fl:
                if fname.split('::')[-1] != leaf:
                    continue
                l0 = text.count('\n', 0, b0) + 1
                l1 = text.count('\n', 0, b1) + 1
                inside = [h for h in asm_units[file] if l0 <= h[0] <= l1]
                if inside:
                    # what is left once the asm blocks are removed
                    lines = text[b0:b1].split('\n')
                    keep = [ln for k, ln in enumerate(lines, l0) if not any(h[0] <= k <= h[1] for h in inside)]
                    rest = re.sub(r'\b(register\s+)?[\w:]+\s*\*?\s*\w+\s*;', '', '\n'.join(keep))
                    kind = 'asm-only' if body_kind(rest) else 'asm-partial'
                break
        # A body that compiled to just the instrumentation, where the original's
        # is more than a return. Not ctors/dtors: clang's base-object variant is
        # empty where CodeWarrior's single __dt also does the delete.
        if (not kind and sym and file.startswith('game/') and bodies.get(fidx, 0) <= min_body + 2
                and syms[sym][1] > 8 and not sym.startswith(('__ct', '__dt'))):
            kind = 'empty'
        recs.append({'id': tidx, 'sym': sym, 'name': d, 'kind': kind, 'match': match, 'unit': unit,
                     'clang': nm, 'mwcc_guess': (mw if nm.startswith('_Z') else nm) or ''})
    # An address-derived match claimed by several port functions is a guess at best.
    by = collections.Counter(r['sym'] for r in recs if r['match'] == 'addr')
    for r in recs:
        if r['match'] == 'addr' and by[r['sym']] > 1:
            r['sym'], r['match'] = '', 'ambiguous'
    stats = collections.Counter()
    for r in recs:
        stats[r['match']] += 1
        stats['kind:' + (r['kind'] or 'game')] += 1
    with open(a_out, 'w') as f:
        for r in recs:
            f.write(f"{r['id']}\t{jesc(r['sym'])}\t{jesc(r['name'])}\t{r['kind']}\t{r['match']}\t{jesc(r['unit'])}\n")
    game = [r for r in recs if r['unit'].split(':')[-1].startswith('game/')]
    gstrong = [r for r in game if not r['unit'].startswith('inline:')]
    mapped = set(r['sym'] for r in recs if r['sym'])
    rep = {'functions': len(recs), 'game_functions': len(game), 'min_body_bytes': min_body,
           'match': {k: v for k, v in stats.items() if not k.startswith('kind:')},
           'kind': {k[5:]: v for k, v in stats.items() if k.startswith('kind:')},
           'game_strong_unmapped': sum(1 for r in gstrong if not r['sym']),
           'game_strong': len(gstrong),
           'symbols_txt_functions': len(syms), 'symbols_txt_matched': len(mapped),
           'unmapped': [{k: r[k] for k in ('id', 'name', 'clang', 'unit', 'match', 'mwcc_guess')} for r in recs if not r['sym']]}
    with open(a_out + '.report.json', 'w') as f:
        json.dump(rep, f, indent=1)
    print(f"diag_symtab: {len(recs)} functions ({len(game)} game/JSystem, {len(gstrong)} of them strong, "
          f"{rep['game_strong_unmapped']} of those unmapped); match {rep['match']}; kind {rep['kind']}; "
          f"symbols.txt functions matched {len(mapped)}/{len(syms)}")


if __name__ == '__main__':
    sys.setrecursionlimit(10000)
    main()
