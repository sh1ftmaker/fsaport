#!/usr/bin/env python3
"""Resolve and rank the endian trap's "endian" stream (docs/DIAGNOSTICS.md, "Endian trap").

    python3 tools/endian_report.py logs/diag/<run> [--wasm build/web-endian/fsa.wasm] [--all]

Reads <run>/endian.jsonl (cumulative lines; the last one per site wins), maps each
site's wasm offset to file:line and the unit with llvm-symbolizer (the trap build
links with FSA_DEBUG_LINES=ON), joins the CodeWarrior symbol from the decomp's
symbols.txt, and writes <run>/endian_resolved.jsonl (same fields, filled in, plus
"symbol", "helper", "score"), ranked: swapped-looking loads first. Prints the
ranking. Sites whose source line already goes through a byte-order helper
(bswap, be16/be32, THP_RD32, TAG_*) are marked helper=true and ranked last.
"""
import argparse, collections, json, math, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SYMBOLIZER = '/bigdata/agent-tom/projects/fsadecomp/tools/emsdk/upstream/bin/llvm-symbolizer'
SYMBOLS = '/bigdata/agent-tom/projects/fsadecomp/fsa-decomp/config/G4SE01/symbols.txt'
HELPER = re.compile(r'bswap|be_swap|swap16|swap32|\bbe(16|32|16s|32s|f32)\b|THP_RD32|thpBE32|TAG_(S32|U16)|be_read_raw|READU32_BE|BE<|portReadBE|GetTypeFromByteStream')
COPY = re.compile(r'memcpy|bcopy|copy|\*dst\s*=|\*udest|getPrmClr|getEnvClr|Color|Clr\b')


def symbolize(wasm, offsets):
    if not offsets:
        return {}
    out = subprocess.run([SYMBOLIZER, '--obj=' + wasm, '--inlining'],
                         input='\n'.join(offsets) + '\n', capture_output=True, text=True).stdout
    blocks = out.strip('\n').split('\n\n')
    res = {}
    for off, block in zip(offsets, blocks):
        lines = block.split('\n')
        frames = [(lines[i], lines[i + 1]) for i in range(0, len(lines) - 1, 2)]
        res[off] = frames
    return res


def rel(path):
    path = os.path.normpath(path)
    for base in (ROOT, '/bigdata/agent-tom/projects/fsaport'):
        if path.startswith(base + '/'):
            path = path[len(base) + 1:]
    # worktree checkouts
    path = re.sub(r'^\.claude/worktrees/[^/]+/', '', path)
    return path


_line_maps = {}


def physical_line(path, line):
    """DWARF lines follow `#line N` (JSystem keeps the original assert lines): map back."""
    if path not in _line_maps:
        try:
            text = open(os.path.join(ROOT, path), errors='replace').read().split('\n')
        except OSError:
            text = []
        vmap, v = {}, 1
        for i, t in enumerate(text, 1):
            m = re.match(r'\s*#\s*line\s+(\d+)', t)
            if m:
                v = int(m.group(1))
                continue
            vmap.setdefault(v, []).append(i)
            v += 1
        _line_maps[path] = (text, vmap)
    text, vmap = _line_maps[path]
    cands = vmap.get(line, [line])
    return cands[-1], (text[cands[-1] - 1] if 0 < cands[-1] <= len(text) else '')


def cw_symbols():
    by_name = collections.defaultdict(list)
    if not os.path.exists(SYMBOLS):
        return by_name
    for line in open(SYMBOLS):
        m = re.match(r'(\S+) = \.text:(0x[0-9A-Fa-f]+);', line)
        if m:
            name = m.group(1)
            base = name.split('__')[0] if '__' in name[1:] else name
            by_name[base].append(name)
    return by_name


def cw_lookup(by_name, func):
    """'A::b(args)' / 'b(args)' -> the CodeWarrior-mangled symbol, if unambiguous."""
    head = func.split('(')[0].strip()
    parts = head.split('::')
    meth = parts[-1]
    cands = by_name.get(meth, [])
    if len(parts) > 1:
        cls = parts[-2]
        cands = [c for c in cands if f'__{len(cls)}{cls}' in c or f'{len(cls)}{cls}F' in c]
    return cands[0] if len(cands) == 1 else (cands[0] + ' (+%d more)' % (len(cands) - 1) if cands else '')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('run')
    ap.add_argument('--wasm', default=os.path.join(ROOT, 'build/web-endian/fsa.wasm'))
    ap.add_argument('--all', action='store_true', help='print plain/neutral-only sites too')
    a = ap.parse_args()
    src = a.run if a.run.endswith('.jsonl') else os.path.join(a.run, 'endian.jsonl')
    last = {}
    for line in open(src):
        r = json.loads(line)
        last[(r['site'], r['wasm'], r['size'])] = r
    rows = list(last.values())
    syms = symbolize(a.wasm, sorted({r['wasm'] for r in rows}))
    units = {l.split('\t')[0] for l in open(os.path.join(ROOT, 'game/units.txt'))}
    by_name = cw_symbols()
    # merge sites that resolve to the same source line and size
    merged = collections.OrderedDict()
    for r in rows:
        frames = syms.get(r['wasm'], [])
        if frames:
            func, loc = frames[0]
            m = re.match(r'(.*):(\d+):\d+$', loc)
            if m:
                r['file'], r['line'] = rel(m.group(1)), int(m.group(2))
            outer = rel(re.sub(r':\d+:\d+$', '', frames[-1][1]))
            unit = outer[len('game/'):] if outer.startswith('game/') else outer
            r['unit'] = unit if unit in units else outer
            if func and func != '??':
                r['function'] = func
        r['symbol'] = cw_lookup(by_name, r['function'])
        text = ''
        if r['file'] and r['line']:
            r['line'], text = physical_line(r['file'], r['line'])
        r['source'] = text.strip()[:160]
        r['helper'] = bool(HELPER.search(text))
        r['copy'] = bool(COPY.search(text))  # byte data moved as words (colours, memcpy): not an endian read
        key = (r['file'], r['line'], r['size']) if r['file'] else (r['wasm'], r['size'])
        if key in merged:
            m = merged[key]
            for k in ('count', 'swapped', 'plain'):
                m[k] += r[k]
        else:
            merged[key] = dict(r)
    out = []
    for r in merged.values():
        r['detail'] = re.sub(r'\d+ swapped-looking, \d+ plain-looking of \d+',
                             '%d swapped-looking, %d plain-looking of %d' % (r['swapped'], r['plain'], r['count']),
                             r['detail'])
        ratio = r['swapped'] / r['count'] if r['count'] else 0
        r['score'] = round((0 if r['helper'] else 0.2 if r['copy'] else 1) * ratio * math.log2(1 + r['swapped']), 2)
        out.append(r)
    out.sort(key=lambda r: (-r['score'], -r['count']))
    dst = os.path.join(os.path.dirname(src), 'endian_resolved.jsonl')
    with open(dst, 'w') as f:
        for r in out:
            f.write(json.dumps(r) + '\n')
    print(f'{len(rows)} sites -> {len(out)} source lines; {dst}')
    for r in out:
        if not a.all and (r['swapped'] == 0 or r['helper']):
            continue
        print(f"{r['score']:6.2f} {r['swapped']:>7}/{r['count']:<7} u{r['size'] * 8:<2} {r['format']:<5} "
              f"{r['file']}:{r['line']}  {r['function'][:60]}\n{'':16}{r['field'][:60]} | {r['source'][:90]}")


if __name__ == '__main__':
    main()
