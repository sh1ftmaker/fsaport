// --pre-js (FSA_DIAG builds only): the page side of the function traces
// (docs/DIAGNOSTICS.md, "Function traces (fntrace)"; platform/diag_fntrace.cpp).
// The page awaits Module.fsaFntraceInit() before main().
//
//   ?trace=URL        a request file (JSON, below), relative to the page
//   ?trace=1          the same with fntrace.json
//   ?trace_cap=N      calls logged in full per function before sampling (default 200)
//   ?trace_every=N    after that, every N-th call (default 50; 0 = none)
//
// A request: {"functions": [sym | {"sym", "this_bytes", "this_ranges", "this",
// "cap", "every"}], "this_bytes": N, "this_ranges": [[start, end], ...],
// "globals": [name | {"name", "bytes"}], "nested": [sym | {...}],
// "nested_this_bytes": N, "cap": N, "every": N, "scene": "...", "question": "..."}
// or {"requests": [request, ...]} (a batch, all traced in one run). A sym is the
// decomp's CodeWarrior symbol (fsa.diag.tsv column 2), the demangled name, or
// "id:<n>". The resolution goes to the "fntrace_config" stream.
Module.fsaFntraceInit = async function () {
  const q = new URLSearchParams(location.search);
  let url = q.get('trace');
  if (!url) return;
  if (url === '1') url = 'fntrace.json';
  const log = (s) => (window.fsaLog || console.log)('[fntrace] ' + s);
  const diag = (o) => { const d = Module.fsaDiag || window.fsaDiag; if (d) d('fntrace_config', JSON.stringify(o)); };
  const get = async (u) => {
    const r = await fetch(u, { cache: 'no-store' });
    if (!r.ok) throw new Error('fntrace: ' + u + ': HTTP ' + r.status);
    return r.text();
  };
  const body = JSON.parse(await get(url));
  const reqs = Array.isArray(body.requests) ? body.requests : [body];
  const bySym = new Map(), byName = new Map();
  const add = (m, k, id) => { if (!k) return; if (!m.has(k)) m.set(k, []); m.get(k).push(id); };
  const unesc = (s) => { try { return JSON.parse('"' + s + '"'); } catch (e) { return s; } };
  for (const line of (await get('fsa.diag.tsv')).split('\n')) {
    const f = line.split('\t');
    if (f.length < 3) continue;
    const id = Number(f[0]);
    add(bySym, unesc(f[1]), id);
    add(byName, unesc(f[2]), id);
  }
  const ft = new Map();  // id -> {this, params, result, declared}
  try {
    for (const line of (await get('fsa.diag.ft.tsv')).split('\n')) {
      const f = line.split('\t');
      if (f.length >= 4) ft.set(Number(f[0]), { this: Number(f[1]), params: f[2], result: f[3], declared: f[4] || '' });
    }
  } catch (e) { log('no fsa.diag.ft.tsv: ' + e); }
  let data = null;
  const dataSym = async (name) => {
    if (!data) {
      data = new Map();
      for (const line of (await get('fsa.diag.data.tsv')).split('\n')) {
        const f = line.split('\t');
        if (f.length >= 3) data.set(f[2], [Number(f[0]), Number(f[1])]);
      }
    }
    return data.get(name);
  };
  const resolve = (s) => {
    if (/^id:\d+$/.test(s)) return [Number(s.slice(3))];
    return bySym.get(s) || byName.get(s) || [];
  };
  const num = (v, d) => (v === undefined || v === null ? d : Number(v));
  const defCap = num(q.get('trace_cap'), 200), defEvery = num(q.get('trace_every'), 50);
  Module._fsa_ft_defaults(defCap, defEvery);
  const cstr = (s) => { const b = new TextEncoder().encode(s + '\0'); const p = Module._malloc(b.length); HEAPU8.set(b, p); return p; };
  let total = 0;
  for (let ri = 0; ri < reqs.length; ri++) {
    const r = reqs[ri];
    const globals = [];
    for (const g of r.globals || []) {
      const name = typeof g === 'string' ? g : g.name;
      const d = await dataSym(name);
      if (!d) { diag({ req: ri, global: name, error: 'not in fsa.diag.data.tsv' }); continue; }
      const size = Math.min(num(g.bytes, d[1] || 4), 512);
      const k = Module._fsa_ft_add_global(d[0], size, cstr(name));
      globals.push(k);
      diag({ req: ri, global: name, addr: d[0], size, index: k });
    }
    const sel = (spec, flags) => {
      const o = typeof spec === 'string' ? { sym: spec } : spec;
      const ids = resolve(o.sym);
      if (!ids.length) { diag({ req: ri, sym: o.sym, kind: flags & 1 ? 'primary' : 'nested', error: 'no such function in fsa.diag.tsv' }); return; }
      const tb = num(o.this_bytes, flags & 1 ? num(r.this_bytes, 64) : num(r.nested_this_bytes, 0));
      const ranges = o.this_ranges || (flags & 1 ? r.this_ranges : r.nested_this_ranges) || [];
      const thisMode = o.this === true ? 1 : o.this === false ? 0 : -1;
      for (const id of ids) {
        const slot = Module._fsa_ft_select(id, flags, tb, thisMode, num(o.cap, num(r.cap, 0xFFFFFFFF)) >>> 0,
          num(o.every, num(r.every, 0xFFFFFFFF)) >>> 0, ri);
        if (slot < 0) { diag({ req: ri, sym: o.sym, id, error: 'too many functions selected' }); continue; }
        for (const [s, e] of ranges) Module._fsa_ft_add_range(slot, s, e);
        for (const g of globals) Module._fsa_ft_use_global(slot, g);
        const t = ft.get(id);
        diag({ req: ri, sym: o.sym, id, slot, kind: flags & 1 ? 'primary' : 'nested', this_bytes: tb, this_ranges: ranges,
               instrumented: !!t, this_arg: t ? t.this : null, params: t ? t.params : null, result: t ? t.result : null,
               declared: t ? t.declared : null, scene: r.scene, question: r.question });
        if (!t) log(`${o.sym} (id ${id}) is not rewritten (no instrumentation or unsupported signature)`);
        total++;
      }
    };
    for (const f of r.functions || []) sel(f, 1);
    for (const f of r.nested || r.callees || []) sel(f, 2);
  }
  Module._fsa_ft_start();
  log(`${url}: ${reqs.length} request(s), ${total} function id(s) selected`);
};
