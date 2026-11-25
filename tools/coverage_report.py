#!/usr/bin/env python3
"""Branch coverage of the game code, per decomp function (docs/DIAGNOSTICS.md,
"Branch coverage").

    tools/coverage_report.py RUN [RUN ...] --build build/coverage --out logs/coverage/NAME.tsv
        [--symbols ../fsadecomp/fsa-decomp/config/G4SE01/symbols.txt] [--llvm-bin DIR]
        [--all] [--json SUMMARY.json]

RUN is a run directory of the coverage build (-DFSA_COVERAGE=ON; serve.py
--diag DIR writes DIR/<run>/coverage_profraw.jsonl) or a .profraw file. Each
run's last complete dump is written next to it as coverage.profraw. Several
runs are merged (llvm-profdata merge: counts add up, so a region is hit when
any run hit it). All runs must come from the build given with --build.

The coverage mapping (clang's __llvm_covmap / __llvm_covfun) is in custom
sections of fsa.wasm, which llvm-cov does not read from a wasm file; they are
copied, with the profile's function-name table, into a small ELF object that
llvm-cov reads instead. Then `llvm-cov export` gives every function's regions
and branches with their counts.

One TSV line per decomp function (header line first), sorted by file and line:

  sym            the decomp's symbol (symbols.txt spelling, the join key of
                 tools/diag_symtab.py), or the clang symbol when there is none
                 (match "none"/"ambiguous")
  blocks_hit     code regions executed at least once
  blocks_total   code regions (clang's source regions: a function body, each
                 branch arm, the code after a return/break, each side of && ||
                 and ?: ...)
  untaken        comma-separated, file:line form (game/src paths):
                   F:L       a code region starting at line L never ran
                   F:L(T)    the condition at L was never true
                   F:L(F)    ... never false
                   F:L0-L1   the function never ran (its whole extent)
  entries        times the function was entered
  branches_hit   branch sides taken (each condition has a true and a false side)
  branches_total
  match          how sym was found (exact, c, name, addr; ambiguous, none)
  name           the port function, demangled
  file           where it is defined: file:line

Several port functions with the same sym (constructor/destructor variants)
are merged: a region counts as hit if any of them hit it. Only functions
defined in the game's own code are listed (game/src/game, game/include/game);
--all lists every instrumented function (inline JSystem/SDK/libc++ code
instantiated in the game units too, which only counts the calls from those
units' copies).
"""
import argparse, base64, collections, json, os, re, shutil, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import diag_symtab  # noqa: E402

GAME_RE = re.compile(r'^game/(src/game|include/game)/')


def leb(b, i):
    r = s = 0
    while True:
        c = b[i]; i += 1
        r |= (c & 0x7f) << s; s += 7
        if c < 0x80:
            return r, i


def wasm_custom_sections(path):
    b = open(path, 'rb').read()
    if b[:4] != b'\0asm':
        raise SystemExit(f'{path}: not a wasm file')
    i, secs = 8, collections.defaultdict(list)
    while i < len(b):
        sid = b[i]; i += 1
        size, i = leb(b, i)
        end = i + size
        if sid == 0:
            nl, j = leb(b, i)
            secs[b[j:j + nl].decode()].append(b[j + nl:end])
        i = end
    return secs


def profraw_from_run(run):
    """The last complete dump of a run dir -> RUN/coverage.profraw (path)."""
    if os.path.isfile(run):
        return run, None
    src = os.path.join(run, 'coverage_profraw.jsonl')
    if not os.path.exists(src):
        raise SystemExit(f'{run}: no coverage_profraw.jsonl (not a coverage-build run, or no flush)')
    dumps = collections.defaultdict(dict)
    meta = {}
    for line in open(src):
        try:
            d = json.loads(line)
        except ValueError:
            continue
        dumps[d['dump']][d['part']] = d['b64']
        meta[d['dump']] = (d['parts'], d['size'], d.get('reads'))
    for k in sorted(dumps, reverse=True):
        parts, size, reads = meta[k]
        if len(dumps[k]) == parts:
            data = b''.join(base64.b64decode(dumps[k][p]) for p in range(parts))
            if len(data) != size:
                continue
            out = os.path.join(run, 'coverage.profraw')
            with open(out, 'wb') as f:
                f.write(data)
            return out, reads
    raise SystemExit(f'{run}: no complete dump in coverage_profraw.jsonl')


def profraw_names(path):
    """The (compressed) function-name table of a raw profile (format 10, wasm32)."""
    p = open(path, 'rb').read()
    magic, ver, bid, ndata, padb, ncnt, pada, nbm, padbm, nsz = struct.unpack_from('<10Q', p, 0)
    if ver & 0xffffffff != 10:
        raise SystemExit(f'{path}: raw profile version {ver & 0xffffffff}, expected 10')
    DATA = 48  # sizeof(__llvm_profile_data) on wasm32
    off = 16 * 8 + bid + ndata * DATA + padb + ncnt * 8 + pada + nbm + padbm
    if off + nsz > len(p):
        raise SystemExit(f'{path}: unexpected raw profile layout')
    return p[off:off + nsz]


def cov_object(wasm, names, tmp, tool):
    """An ELF object llvm-cov can read: the wasm's coverage sections + the names."""
    secs = wasm_custom_sections(wasm)
    if '__llvm_covmap' not in secs:
        raise SystemExit(f'{wasm}: no __llvm_covmap section (not a -DFSA_COVERAGE=ON build)')
    files = {}
    for sec, data in (('__llvm_covmap', b''.join(secs['__llvm_covmap'])),
                      ('__llvm_covfun', b''.join(secs['__llvm_covfun'])),
                      ('__llvm_prf_names', names)):
        files[sec] = os.path.join(tmp, sec)
        with open(files[sec], 'wb') as f:
            f.write(data)
    obj = os.path.join(tmp, 'fsa.cov.o')
    subprocess.check_call([tool('clang'), '-c', '-x', 'c', os.devnull, '-o', obj, '--target=x86_64-linux-gnu'])
    cmd = [tool('llvm-objcopy')]
    for sec, f in files.items():
        cmd += ['--add-section', f'{sec}={f}', '--set-section-alignment', f'{sec}=8']
    subprocess.check_call(cmd + [obj])
    return obj


def relpath(path):
    """An absolute source path -> game/... (whatever checkout built it)."""
    m = re.search(r'(?:^|/)((?:game|platform|extern)/.*)$', path)
    return m.group(1) if m else path


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('runs', nargs='+')
    ap.add_argument('--build', default=os.path.join(ROOT, 'build', 'coverage'))
    ap.add_argument('--out', required=True)
    ap.add_argument('--symbols', help="the decomp's symbols.txt (default: the build's FSA_DECOMP_SYMBOLS)")
    ap.add_argument('--llvm-bin', help="emsdk's upstream/bin (default: from the build's compiler)")
    ap.add_argument('--all', action='store_true', help='list every instrumented function, not only the game\'s own')
    ap.add_argument('--json', help='summary JSON (default: OUT with .json)')
    a = ap.parse_args()
    cache = {}
    try:
        for line in open(os.path.join(a.build, 'CMakeCache.txt')):
            m = re.match(r'^(\w+):\w+=(.*)$', line.rstrip('\n'))
            if m:
                cache[m.group(1)] = m.group(2)
    except OSError:
        pass
    if not a.symbols:
        a.symbols = cache.get('FSA_DECOMP_SYMBOLS', '')
    if not a.llvm_bin:
        tc = cache.get('CMAKE_TOOLCHAIN_FILE', '')  # .../upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
        if tc:
            a.llvm_bin = os.path.join(tc.split('/emscripten/cmake/')[0], 'bin')
        elif os.environ.get('EMSDK'):
            a.llvm_bin = os.path.join(os.environ['EMSDK'], 'upstream', 'bin')
    tool = lambda n: os.path.join(a.llvm_bin, n) if a.llvm_bin and os.path.exists(os.path.join(a.llvm_bin, n)) else n

    raws, reads = [], []
    for r in a.runs:
        p, n = profraw_from_run(r)
        raws.append(p)
        reads.append({'run': os.path.basename(os.path.normpath(r)), 'reads': n})
    tmp = tempfile.mkdtemp(prefix='covrep-')
    try:
        prof = os.path.join(tmp, 'merged.profdata')
        subprocess.check_call([tool('llvm-profdata'), 'merge', '-o', prof] + raws)
        obj = cov_object(os.path.join(a.build, 'fsa.wasm'), profraw_names(raws[0]), tmp, tool)
        exp = subprocess.run([tool('llvm-cov'), 'export', '-format=text', '-skip-expansions',
                              '-instr-profile=' + prof, obj], capture_output=True, text=True)
        if exp.returncode:
            sys.stderr.write(exp.stderr)
            raise SystemExit('llvm-cov export failed')
        warn = [l for l in exp.stderr.splitlines() if l.strip() and 'object is newer' not in l]
        if warn:
            sys.stderr.write('llvm-cov: ' + '\n'.join(warn[:10]) + '\n')
        data = json.loads(exp.stdout)['data'][0]
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    funcs = data['functions']
    clang_names = [f['name'].rsplit(':', 1)[-1] for f in funcs]  # local symbols are "file:sym"
    dem = subprocess.run([tool('llvm-cxxfilt')], input='\n'.join(clang_names) + '\n',
                         capture_output=True, text=True).stdout.split('\n')
    syms, byaddr, bykey = diag_symtab.load_symbols(a.symbols) if os.path.exists(a.symbols) else ({}, {}, {})

    rows = {}  # key -> merged record
    skipped = 0
    for f, nm, d in zip(funcs, clang_names, dem):
        fnames = [relpath(x) for x in f['filenames']]
        regs = f['regions']
        main = [r for r in regs if r[5] == 0 and r[7] == 0]
        if not main:
            continue
        file0 = fnames[0]
        if not a.all and not GAME_RE.match(file0):
            skipped += 1
            continue
        d = d.strip() or nm
        q = diag_symtab.short(diag_symtab.qualname(d))
        try:
            sym, match, _ = diag_symtab.join_sym(nm, q, file0, syms, byaddr, bykey)
        except Exception:
            sym, match = '', 'none'
        key = sym or nm
        r = rows.get(key)
        if r is None:
            r = rows[key] = {'sym': key, 'match': match if sym else match, 'name': d, 'clang': nm,
                             'file': file0, 'line0': min(x[0] for x in main), 'line1': max(x[2] for x in main),
                             'entries': 0, 'regions': {}, 'branches': {}}
        r['entries'] += f['count']
        r['line0'] = min(r['line0'], min(x[0] for x in main))
        r['line1'] = max(r['line1'], max(x[2] for x in main))
        for x in regs:
            if x[7] != 0:  # 0 = code region (skipped / gap / branch regions are not blocks)
                continue
            k = (fnames[x[5]], x[0], x[1], x[2], x[3])
            r['regions'][k] = r['regions'].get(k, 0) + x[4]
        for x in f.get('branches', []):
            k = (fnames[x[6]], x[0], x[1], x[2], x[3])
            t, fa = r['branches'].get(k, (0, 0))
            r['branches'][k] = (t + x[4], fa + x[5])

    out_rows = []
    tot = collections.Counter()
    for r in rows.values():
        bh = sum(1 for c in r['regions'].values() if c > 0)
        bt = len(r['regions'])
        sh = sum((t > 0) + (fa > 0) for t, fa in r['branches'].values())
        st = 2 * len(r['branches'])
        if r['entries'] == 0:
            untaken = [f"{r['file']}:{r['line0']}-{r['line1']}"]
        else:
            items = set()
            for (fn, l0, c0, l1, c1), c in r['regions'].items():
                if c == 0:
                    items.add((fn, l0, ''))
            for (fn, l0, c0, l1, c1), (t, fa) in r['branches'].items():
                if t == 0:
                    items.add((fn, l0, '(T)'))
                if fa == 0:
                    items.add((fn, l0, '(F)'))
            untaken = [f'{fn}:{l}{s}' for fn, l, s in sorted(items)]
        out_rows.append((r['file'], r['line0'], r['sym'], bh, bt, ','.join(untaken), r['entries'], sh, st,
                         r['match'], r['name'], f"{r['file']}:{r['line0']}"))
        tot['functions'] += 1
        tot['entered' if r['entries'] else 'never_entered'] += 1
        tot['blocks_hit'] += bh; tot['blocks_total'] += bt
        tot['branches_hit'] += sh; tot['branches_total'] += st
        if r['entries']:
            tot['blocks_hit_in_entered'] += bh; tot['blocks_total_in_entered'] += bt
        tot['mapped' if r['match'] not in ('none', 'ambiguous') else 'unmapped'] += 1
    out_rows.sort()
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, 'w') as f:
        f.write('sym\tblocks_hit\tblocks_total\tuntaken\tentries\tbranches_hit\tbranches_total\tmatch\tname\tfile\n')
        for row in out_rows:
            f.write('\t'.join(str(x) for x in row[2:]) + '\n')
    summary = {'runs': reads, 'build': os.path.abspath(a.build), 'scope': 'all' if a.all else 'game',
               'not_game_functions_skipped': skipped, **tot}
    with open(a.json or os.path.splitext(a.out)[0] + '.json', 'w') as f:
        json.dump(summary, f, indent=1)
    pct = lambda x, y: f'{100.0 * x / y:.1f}%' if y else '-'
    print(f"{a.out}: {tot['functions']} functions ({tot['mapped']} with a decomp symbol); "
          f"entered {tot['entered']}, never entered {tot['never_entered']}; "
          f"blocks {tot['blocks_hit']}/{tot['blocks_total']} ({pct(tot['blocks_hit'], tot['blocks_total'])}), "
          f"in entered functions {tot['blocks_hit_in_entered']}/{tot['blocks_total_in_entered']} "
          f"({pct(tot['blocks_hit_in_entered'], tot['blocks_total_in_entered'])}); "
          f"branch sides {tot['branches_hit']}/{tot['branches_total']} ({pct(tot['branches_hit'], tot['branches_total'])})")


if __name__ == '__main__':
    sys.setrecursionlimit(10000)
    main()
