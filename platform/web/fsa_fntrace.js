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
// "globals": [name | {"name", "bytes", "deref", "type"}], "nested": [sym | {..., "follow"}],
// "nested_this_bytes": N, "ptr_args": [{"arg" (wasm parameter index or "ret"), "bytes", "str", "type"}],
// "nested_ptr_args": [...],
// "cap": N, "every": N, "scene": "...", "question": "..."}
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
  // The decomp's symbol and the port's can differ in the parameter list (a
  // parameter the port declares and the decomp's symbols.txt does not yet):
  // without an exact match, a unique port symbol with the same name and class
  // ("name__<len><class>", up to the F of the parameters) is taken.
  const symPrefix = (s) => {
    const i = s.indexOf('__', 1);
    if (i < 0) return null;
    let j = i + 2;
    const m = /^(Q\d)?(\d+)/.exec(s.slice(j));
    if (m && !m[1]) j += m[2].length + Number(m[2]);
    else if (m && m[1]) return null;
    if (s[j] === 'C') j++;
    return s[j] === 'F' ? s.slice(0, j) : null;
  };
  const resolve = (s) => {
    if (/^id:\d+$/.test(s)) return { ids: [Number(s.slice(3))] };
    const ids = bySym.get(s) || byName.get(s);
    if (ids) return { ids };
    const pre = symPrefix(s);
    if (!pre) return { ids: [] };
    const cand = [...bySym.keys()].filter((k) => symPrefix(k) === pre);
    if (cand.length !== 1) return { ids: [], near: cand.slice(0, 8) };
    return { ids: bySym.get(cand[0]), by_name: cand[0] };
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
      const deref = typeof g === 'object' && !!g.deref;
      const k = Module._fsa_ft_add_global(d[0], size, cstr(name), deref ? 1 : 0);
      globals.push(k);
      diag({ req: ri, global: name, addr: d[0], size, index: k, deref: deref || undefined, type: g.type });
    }
    const sel = (spec, flags) => {
      const o = typeof spec === 'string' ? { sym: spec } : spec;
      const res = resolve(o.sym), ids = res.ids;
      if (!ids.length) {
        diag({ req: ri, sym: o.sym, kind: flags & 1 ? 'primary' : 'nested', error: 'no such function in fsa.diag.tsv',
               candidates: res.near });
        return;
      }
      const ptrArgs = o.ptr_args || (flags & 1 ? r.ptr_args : r.nested_ptr_args) || [];
      const tb = num(o.this_bytes, flags & 1 ? num(r.this_bytes, 64) : num(r.nested_this_bytes, 0));
      const ranges = o.this_ranges || (flags & 1 ? r.this_ranges : r.nested_this_ranges) || [];
      const thisMode = o.this === true ? 1 : o.this === false ? 0 : -1;
      for (const id of ids) {
        const slot = Module._fsa_ft_select(id, flags | (o.follow ? 4 : 0), tb, thisMode, num(o.cap, num(r.cap, 0xFFFFFFFF)) >>> 0,
          num(o.every, num(r.every, 0xFFFFFFFF)) >>> 0, ri);
        if (slot < 0) { diag({ req: ri, sym: o.sym, id, error: 'too many functions selected' }); continue; }
        for (const [s, e] of ranges) Module._fsa_ft_add_range(slot, s, e);
        for (const g of globals) Module._fsa_ft_use_global(slot, g);
        for (const p of ptrArgs) {
          const bytes = num(p.bytes, p.str ? 64 : 4);
          if (Module._fsa_ft_add_ptr_arg(slot, p.arg === 'ret' ? 31 : num(p.arg, 0), bytes, p.str ? 1 : 0) < 0)
            diag({ req: ri, sym: o.sym, id, ptr_arg: p, error: 'pointer argument not added (at most 4, 1024 bytes)' });
        }
        const t = ft.get(id);
        diag({ req: ri, sym: o.sym, id, slot, kind: flags & 1 ? 'primary' : 'nested', this_bytes: tb, this_ranges: ranges,
               instrumented: !!t, this_arg: t ? t.this : null, params: t ? t.params : null, result: t ? t.result : null,
               declared: t ? t.declared : null, scene: r.scene, question: r.question,
               ptr_args: ptrArgs.length ? ptrArgs : undefined, port_sym: res.by_name });
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
