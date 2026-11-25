#!/usr/bin/env python3
"""Run a function-trace request end to end (docs/DIAGNOSTICS.md, "Function
traces (fntrace)"): serve it next to the page, drive the page in Chromium,
summarize.

  tools/fntrace_run.py REQUEST.json [--stage S-L | --script FILE.json] [--seconds 90]
      [--keys 'Space@40,Space@44'] [--evals 'Module._fsa_touch_set(0,100,0,0,0)@50']
      [--build build/diag] [--port 8836] [--cdp 9356] [--diag logs/diag]
      [--out DIR] [--cap 200] [--every 50] [--extra 'k=v&k2=v2']

Needs a running tools/serve.py --dir BUILD --diag DIAG --iso ... on --port and
a Chromium with --remote-debugging-port=CDP (docs/DIAGNOSTICS.md shows the sbg
commands). Without --stage or --script the page boots normally (title screen;
use --keys to go further). With --stage S-L and no --keys, the defaults are
'Space@40,Space@44' (the stage-start dialogue) and the stick held right from
50 s ('--evals'), as in the coverage sessions.

Writes the run to DIAG/<run>/ (fntrace*.jsonl, fntrace_summary.md) and, with
--out, one <name>.md + <name>.jsonl per function into DIR (named by the decomp's
symbol, see tools/fntrace_summary.py). Prints the run dir.
"""
import argparse, glob, json, os, shutil, subprocess, sys, time, urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fntrace_layout  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def port_ranges(lay, ranges):
    """Decomp-offset ranges -> merged port-offset ranges."""
    out = []
    for s, e in ranges:
        if e <= s:
            continue
        ps, pe = fntrace_layout.to_port(lay, s), fntrace_layout.to_port(lay, e - 1) + 1
        out.append([ps, max(pe, ps + 1)])
    out.sort()
    merged = []
    for s, e in out:
        if merged and s <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], e)
        else:
            merged.append([s, e])
    return merged


def translate(body, build):
    """Requests give `this` offsets in the decomp's layout; the trace needs the
    port's. Each primary function becomes an object with explicit port ranges
    (unless the request says "offsets": "port"). The default without
    this_bytes is the whole object (at most 4096 bytes)."""
    reqs = body['requests'] if isinstance(body.get('requests'), list) else [body]
    for r in reqs:
        if r.get('offsets') == 'port':
            continue
        funcs = []
        for f in r.get('functions', []):
            o = {'sym': f} if isinstance(f, str) else dict(f)
            lay = fntrace_layout.layout_for_sym(build, o['sym'])
            tb = o.get('this_bytes', r.get('this_bytes'))
            if tb is None:
                tb = min(lay['size'], 4096) if lay and lay.get('size') else 64
            ranges = [[0, tb]] if tb else []
            ranges += o.get('this_ranges', r.get('this_ranges', []))
            if lay:
                pr = port_ranges(lay, ranges)
                if pr != ranges:
                    print(f'fntrace_run: {o["sym"]}: decomp ranges {ranges} -> port {pr} ({lay["cls"]})')
                o.update(this_bytes=0, this_ranges=pr, decomp_ranges=ranges)
            else:
                o.update(this_bytes=tb, this_ranges=ranges[1:] if tb else ranges)
            funcs.append(o)
        r['functions'] = funcs
    return body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('request')
    ap.add_argument('--stage')
    ap.add_argument('--script')
    ap.add_argument('--seconds', type=int, default=90)
    ap.add_argument('--keys')
    ap.add_argument('--evals')
    ap.add_argument('--build', default=os.path.join(ROOT, 'build/diag'))
    ap.add_argument('--port', type=int, default=8836)
    ap.add_argument('--cdp', type=int, default=9356)
    ap.add_argument('--diag', default=os.path.join(ROOT, 'logs/diag'))
    ap.add_argument('--out')
    ap.add_argument('--cap', type=int)
    ap.add_argument('--every', type=int)
    ap.add_argument('--extra', default='')
    ap.add_argument('--name')
    ap.add_argument('--notes', help='JSON {name: branch text} for the one-line summaries (fntrace_summary.py --notes)')
    a = ap.parse_args()

    name = a.name or os.path.splitext(os.path.basename(a.request))[0]
    req_dir = os.path.join(a.build, 'req')
    os.makedirs(req_dir, exist_ok=True)
    req = translate(json.load(open(a.request)), a.build)
    json.dump(req, open(os.path.join(req_dir, name + '.json'), 'w'), indent=1)
    q = ['iso=local.iso', 'nosave=1', f'trace=req/{name}.json']
    keys, evals = a.keys, a.evals
    if a.stage:
        q.append(f'stage={a.stage}')
        if keys is None:
            keys = 'Space@40,Space@44'
        if evals is None:
            evals = 'Module._fsa_touch_set(0,100,0,0,0)@50'
    if a.script:
        shutil.copyfile(a.script, os.path.join(req_dir, name + '.script.json'))
        q.append(f'script=req/{name}.script.json')
    if a.cap is not None:
        q.append(f'trace_cap={a.cap}')
    if a.every is not None:
        q.append(f'trace_every={a.every}')
    if a.extra:
        q.append(a.extra)
    url = f'http://127.0.0.1:{a.port}/?' + '&'.join(q)
    try:
        urllib.request.urlopen(f'http://127.0.0.1:{a.port}/fsa.diag.ft.tsv', timeout=5).close()
    except Exception as e:
        sys.exit(f'no server with an fntrace build on port {a.port} ({e})')
    before = set(glob.glob(os.path.join(a.diag, '*')))
    t0 = time.time()
    cmd = ['node', os.path.join(HERE, 'web_run.mjs'), '--cdp', str(a.cdp), '--url', url,
           '--seconds', str(a.seconds), '--every', str(max(a.seconds // 3, 10)),
           '--out', os.path.join(ROOT, 'build/web-run', 'fntrace-' + name), '--quiet']
    if keys:
        cmd += ['--keys', keys]
    if evals:
        cmd += ['--evals', evals]
    print('url:', url, flush=True)
    subprocess.run(cmd, check=True)
    time.sleep(3)  # the last POSTs
    new = [d for d in glob.glob(os.path.join(a.diag, '*')) if d not in before and os.path.getmtime(d) >= t0 - 5]
    if not new:
        sys.exit('no new run directory in ' + a.diag)
    run = max(new, key=os.path.getmtime)
    if not os.path.exists(os.path.join(run, 'fntrace_config.jsonl')):
        sys.exit(f'{run}: no fntrace_config.jsonl (request not loaded? see build/web-run/fntrace-{name}/console.log)')
    cmd = [sys.executable, os.path.join(HERE, 'fntrace_summary.py'), run, '--build', a.build]
    if a.out:
        cmd += ['--split', a.out]
    if a.notes:
        cmd += ['--notes', a.notes]
    subprocess.run(cmd, check=True)
    if a.out:
        shutil.copyfile(os.path.join(run, 'fntrace_summary.md'), os.path.join(a.out, 'fntrace_summary.md'))
    print(run)


if __name__ == '__main__':
    main()
