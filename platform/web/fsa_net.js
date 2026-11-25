// --pre-js (deluxe): netplay, the page side (docs/NETPLAY.md, platform/netplay.cpp).
//
// Every browser runs the whole game; only inputs travel. The timeline is the
// four pads of every frame n (n = the game's pad read, docs/HARNESS.md). Each
// browser owns one seat (= one controller port = one GBA) and stamps its local
// input D frames ahead; the other seats' inputs come from the relay.
//
//   ?room=CODE&seat=N    join room CODE (relay: ?relay=URL, default the page's /net)
//   ?net=rollback        (default) predict missing inputs, roll back when one arrives
//                        different; the lead over the confirmed frontier is bounded
//                        (?window=W, default 20): past it the game waits, it never
//                        drops an input.
//   ?net=lockstep        wait for every input of frame n (delay ?delay=D, or adaptive)
//   ?rbtest=E,R          no network: every E reads, roll back R frames and re-run them
//                        with the same inputs; the state reached again must be the
//                        same bytes (a self-check of snapshot/restore)
//   ?snapevery=K         a snapshot every K reads (default 1); ?ring=N slots
//
// Everything needs lockstep (platform/web/fsa_harness.js turns it on).
Module.fsaNetInit = async function () {
  const q = new URLSearchParams(location.search);
  // A room session (platform/web/fsa_net_room.js) has already joined and started.
  const sess = Module.fsaNetSession || null;
  const rbtest = q.get('rbtest');
  if (!sess && !rbtest) return false;
  const log = (s) => (window.fsaLog || console.log)('[net] ' + s);
  // The room's options win over the page's (every browser must agree on them).
  const ro = sess ? sess.opts : {};
  const num = (k, d) => (ro[k] !== undefined && ro[k] !== null ? Number(ro[k])
                         : q.get(k) !== null && q.get(k) !== '' ? Number(q.get(k)) : d);

  // --- Snapshot regions (tools/net_regions.py, written next to fsa.wasm) ------
  const rr = await fetch('fsa.netregions.json', { cache: 'no-store' });
  if (!rr.ok) throw new Error('netplay: fsa.netregions.json: HTTP ' + rr.status);
  const regions = await rr.json();
  const mode = rbtest ? 'rbtest' : (ro.mode || q.get('net') || 'rollback');
  const rollback = mode !== 'lockstep';
  const snapEvery = Math.max(1, num('snapevery', 1));
  const window_ = Math.max(1, num('window', 20));
  const ringSlots = Math.max(2, num('ring', Math.ceil((window_ + 4) / snapEvery) + 1));
  const SPARE = ringSlots;  // rbtest: the reference snapshot, then the re-reached state
  const totalSlots = ringSlots + 2;
  {
    const pairs = new Uint32Array(regions.ranges.length * 2);
    regions.ranges.forEach((r, i) => { pairs[2 * i] = r[0]; pairs[2 * i + 1] = r[1]; });
    const ptr = Module._malloc(pairs.byteLength);
    HEAPU8.set(new Uint8Array(pairs.buffer), ptr);
    if (!Module._fsa_net_config(ptr, regions.ranges.length, rollback ? totalSlots : 1))
      throw new Error('netplay: fsa_net_config refused');
  }
  if (q.get('netprobe') === '1') Module._fsa_net_probe(1);
  if (q.get('mem1trim') === '0') Module._fsa_net_set_mem1_trim(0);
  const mem1 = q.get('mem1');  // "lo,hi" offsets into MEM1 (default all of it)
  if (mem1) {
    const [lo, hi] = mem1.split(',').map(Number);
    const base = Module._fsa_net_mem1_start();
    Module._fsa_net_set_mem1(base + lo, base + hi);
  }

  // --- The timeline -----------------------------------------------------------
  // Per port and frame: a, b (packed pad, see netplay.cpp g_io), err, known.
  const PORTS = 4;
  class Column {
    constructor() { this.cap = 1 << 14; this.a = new Uint32Array(this.cap); this.b = new Uint32Array(this.cap);
                    this.e = new Int8Array(this.cap); this.k = new Uint8Array(this.cap); this.max = -1; }
    grow(f) {
      if (f < this.cap) return;
      let cap = this.cap; while (cap <= f) cap *= 2;
      for (const key of ['a', 'b', 'e', 'k']) { const n = new this[key].constructor(cap); n.set(this[key]); this[key] = n; }
      this.cap = cap;
    }
    set(f, a, b, e) { this.grow(f); this.a[f] = a; this.b[f] = b; this.e[f] = e; this.k[f] = 1; if (f > this.max) this.max = f; }
    has(f) { return f < this.cap && this.k[f] === 1; }
  }
  const conf = [], used = [];
  for (let p = 0; p < PORTS; p++) { conf.push(new Column()); used.push(new Column()); }

  const S = {
    mode, rollback, room: sess ? sess.room : null, log,
    seats: 0,            // ports 0..seats-1 are players (GBAs)
    local: [],           // the ports this browser stamps
    delay: Math.max(0, num('delay', rollback ? 2 : 4)),
    adaptive: (ro.delay === null || ro.delay === undefined) && q.get('delay') === null && !rollback,
    startDelay: 8,       // frames 0..startDelay-1 are neutral for every seat
    lastStamped: [],     // per local port
    head: 0,             // reads executed for the first time
    frontier: 0,         // every input of frames < frontier is known
    pendingRollback: Infinity,
    resim: false,
    left: new Map(),     // port -> first frame it is neutral (a seat that left)
    cur: 0,
    stats: { rollbacks: 0, resimFrames: 0, maxDepth: 0, refused: 0, stalls: 0, stallMs: 0, stallMax: 0,
             snapMs: 0, snaps: 0, restoreMs: 0, resimMs: 0, desyncs: 0, mismatches: 0, checks: 0,
             predicted: 0, mispredicted: 0, timeSyncWaits: 0, gatedFrames: 0, hashesSent: 0, hashesCompared: 0 },
    onLocalInputs: null, // (port, first, values[]) -> send
    onHash: null,        // (n, h) -> send
    waiters: [],
    started: false,
    startWait: null,
  };
  Module.fsaNet = S;
  S.wasFast = Module.fsaClock.fast;
  const io = () => new Uint32Array(HEAPU8.buffer, Module._fsa_net_io(), 12);

  S.configure = (seats, local) => {
    S.seats = seats;
    S.local = local.slice();
    S.lastStamped = S.local.map(() => S.startDelay - 1);
  };
  // The neutral pad of port p (a player seat: connected, nothing pressed;
  // otherwise unplugged).
  const neutral = (p) => (p < S.seats ? [0, 0, 0] : [0, 0, -1]);
  const lastKnown = (p, f) => {
    // The prediction for port p at frame f: its latest known input at or before f.
    const c = conf[p];
    for (let g = Math.min(f, c.max); g >= 0 && g >= f - 4096; g--) if (c.k[g]) return [c.a[g], c.b[g], c.e[g]];
    return neutral(p);
  };
  const known = (p, f) => {
    if (p >= S.seats || f < S.startDelay) return neutral(p);
    const l = S.left.get(p);
    if (l !== undefined && f >= l) return neutral(p);
    const c = conf[p];
    return c.has(f) ? [c.a[f], c.b[f], c.e[f]] : null;
  };
  const advanceFrontier = () => {
    let f = S.frontier;
    for (;;) {
      let all = true;
      for (let p = 0; p < S.seats && all; p++) if (!known(p, f)) all = false;
      if (!all) break;
      f++;
    }
    if (f !== S.frontier) {
      S.frontier = f;
      flushDiag();
      const w = S.waiters; S.waiters = [];
      for (const r of w) r();
    }
  };

  // An input for port p, frames first.. (values: [a, b, e] each), from the
  // network or from this browser.
  S.receive = (p, first, values) => {
    for (let i = 0; i < values.length; i++) {
      const f = first + i, [a, b, e] = values[i];
      if (conf[p].has(f)) continue;  // duplicates (a replay after a reconnect)
      conf[p].set(f, a, b, e);
      if (f < S.head && used[p].has(f) && (used[p].a[f] !== a || used[p].b[f] !== b || used[p].e[f] !== e)) {
        S.stats.mispredicted++;
        if (f < S.pendingRollback) S.pendingRollback = f;
      }
    }
    advanceFrontier();
  };
  // A seat left: from frame `at` on it is neutral (a timeline event the relay
  // orders after that seat's last input, so every browser agrees on `at`).
  S.seatLeft = (p, at) => {
    S.left.set(p, at);
    for (let f = at; f < S.head; f++)
      if (used[p].has(f) && (used[p].a[f] || used[p].b[f] || used[p].e[f] !== neutral(p)[2])) {
        if (f < S.pendingRollback) S.pendingRollback = f;
        break;
      }
    advanceFrontier();
  };

  // --- Diagnostics streams: only what is final -----------------------------
  // Lines written while frame r runs are held until every input of frames < r
  // is known (r < frontier): a predicted frame's lines may be re-run. Re-runs of
  // lines already written are dropped (rbtest: compared first).
  const realDiag = Module.fsaDiag;
  let pending = [];
  let flushed = -1;
  const flushedState = new Map();  // rbtest: r -> state line hash
  const fnv = (s) => { let h = 0x811c9dc5; for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 0x01000193); } return h >>> 0; };
  function flushDiag() {
    if (!pending.length) return;
    let k = 0;
    for (; k < pending.length && pending[k].r < S.frontier; k++) {
      const x = pending[k];
      if (x.r <= flushed) continue;
      if (realDiag) realDiag(x.name, x.line);
      if (x.name === 'state') {
        const h = fnv(x.line);
        if (S.onHash) { S.onHash(x.r, h); S.stats.hashesSent++; }
        if (mode === 'rbtest') { flushedState.set(x.r, h); if (flushedState.size > 4000) flushedState.delete(flushedState.keys().next().value); }
      }
    }
    if (k) {
      flushed = Math.max(flushed, pending[k - 1].r);
      pending = pending.slice(k);
    }
  }
  Module.fsaDiag = (name, line) => {
    const r = S.cur;
    if (r <= flushed) {
      if (mode === 'rbtest' && name === 'state' && flushedState.has(r)) {
        S.stats.checks++;
        if (flushedState.get(r) !== fnv(line)) { S.stats.mismatches++; log(`rbtest: state at read ${r} differs after a re-run`); }
      }
      return;
    }
    pending.push({ r, name, line });
    if (pending.length > 20000) pending = pending.slice(-20000);
  };
  const dropPending = (from) => { pending = pending.filter((x) => x.r < from); };

  // --- Snapshots and rollback (called by C at the top of every pad read) -------
  const slotOf = (f) => Math.floor(f / snapEvery) % ringSlots;
  const now = () => performance.now();
  let rbtestNext = 0, rbtestRef = -1, rbtestRefSlot = -1;
  const [rbEvery, rbDepth] = (rbtest || '').split(',').map(Number);
  // A slot diff as text: address(part)=a/b, stacks also as offset below their top.
  function diffText(sa, sb, max) {
    const out = Module._malloc(16 * max);
    const k = Module._fsa_net_slot_diff(sa, sb, out, max);
    const a = new Uint32Array(HEAPU8.buffer, out, 4 * max);
    const where = [];
    let stackOnly = k > 0 && k <= max;
    for (let i = 0; i < Math.min(k, max); i++) {
      const addr = a[4 * i], part = a[4 * i + 1] | 0;
      if (part > -2) stackOnly = false;
      let t = `0x${addr.toString(16)}(${part})=${a[4 * i + 2].toString(16)}/${a[4 * i + 3].toString(16)}`;
      if (part <= -2) t += `[top-0x${(Module._fsa_net_slot_stack(sa, -2 - part, 1) - addr).toString(16)}]`;
      where.push(t);
    }
    Module._free(out);
    return { k, where, stackOnly };
  }
  let resimStart = 0;

  function setResim(on) {
    if (on === S.resim) return;
    S.resim = on;
    Module._fsa_net_set_resim(on ? 1 : 0);
    Module.fsaClock.fast = on || !!S.wasFast;
    if (on) resimStart = now(); else S.stats.resimMs += now() - resimStart;
  }

  // The thread layout: each running coroutine's (id, switched-out sp). A JSPI
  // continuation cannot be restored, so a snapshot is only usable while the
  // coroutines it saw running are still switched out at the same places
  // (platform_threads_restore). Threads that come and go (the game's loading
  // threads live a few reads) would break that if a frame they live through
  // were predicted: so frames run on predicted inputs only while every
  // coroutine has been where it is for STABLE reads; otherwise the frame waits
  // for its inputs, as in lockstep (docs/NETPLAY.md, "Threads").
  const STABLE = Math.max(1, num('stable', 120));
  const layoutBuf = Module._malloc(12 * 64);
  const layout = () => {
    const k = Module._fsa_net_layout(layoutBuf, 64);
    const a = new Uint32Array(HEAPU8.buffer, layoutBuf, 3 * k);
    const out = [];
    for (let i = 0; i < k; i++) out.push([a[3 * i], a[3 * i + 1], a[3 * i + 2]]);
    return out;
  };
  const layoutText = (l) => l.map(([id, sp, top]) => `${id}:${sp.toString(16)}/${top.toString(16)}`).join(' ');
  const since = new Map();  // coroutine id -> [sp, first read seen there]
  S.predictOk = false;
  S.layoutChanges = 0;
  function track(n) {
    const l = layout();
    const seen = new Set();
    let changed = false;
    for (const [id, sp] of l) {
      seen.add(id);
      const e = since.get(id);
      if (!e || e[0] !== sp) { since.set(id, [sp, n]); changed = true; }
    }
    for (const id of [...since.keys()]) if (!seen.has(id)) { since.delete(id); changed = true; }
    if (changed) {
      S.layoutChanges++;
      if (q.get('netlayout') === '1') log(`layout at read ${n}: ${layoutText(l)}`);
    }
    let ok = true;
    for (const [, [, at]] of since) if (n - at < STABLE) { ok = false; break; }
    S.predictOk = ok;
  }
  S.begin = (n) => {
    S.cur = n;
    if (!rollback) return;
    if (n === S.head && S.pendingRollback >= n) track(n);
    // rbtest: force a rollback of rbDepth frames every rbEvery live reads.
    if (mode === 'rbtest' && n === S.head && n >= rbtestNext && n > rbDepth + S.startDelay + 2 && rbtestRef < 0 && S.predictOk) {
      rbtestNext = n + rbEvery;
      // Reference: the state at the top of read n, to compare with the re-run's.
      if (Module._fsa_net_snapshot(SPARE, n) === 1) { rbtestRef = n; rbtestRefSlot = SPARE; }
      S.pendingRollback = Math.min(S.pendingRollback, n - rbDepth);
    }
    if (S.pendingRollback < n) {
      const want = S.pendingRollback;
      S.pendingRollback = Infinity;
      // The newest snapshot at or before `want`.
      let best = -1, bestSlot = -1;
      for (let s = 0; s < ringSlots; s++) {
        const f = Module._fsa_net_slot_frame(s);
        if (f >= 0 && f <= want && f > best) { best = f; bestSlot = s; }
      }
      if (bestSlot < 0) {
        S.stats.refused++;
        S.fatal(`rollback to frame ${want} impossible: no snapshot that old (read ${n}); this browser is now out of sync`);
      } else {
        const t0 = now();
        const r = Module._fsa_net_restore(bestSlot, mode === 'rbtest' ? 1 : 0);
        if (r !== 1) {
          S.stats.refused++;
          log(`refused restore of frame ${best}: threads now ${layoutText(layout())}`);
          S.fatal(`rollback to frame ${best} refused (${r}: the threads are not where they were at the snapshot); out of sync`);
        } else {
          S.stats.restoreMs += now() - t0;
          S.stats.rollbacks++;
          S.stats.maxDepth = Math.max(S.stats.maxDepth, S.head - best);
          dropPending(best);
          S.rbFrom = best; S.rbReported = false;
          n = best;
          S.cur = n;
          setResim(true);
        }
      }
    }
    if (S.resim && n >= S.head) setResim(false);
    if (S.resim) S.stats.resimFrames++;
    // rbtest: the re-run reached the reference read again.
    if (mode === 'rbtest' && rbtestRef >= 0 && n === rbtestRef && !S.resim) {
      const other = SPARE + 1;
      if (Module._fsa_net_snapshot(other, n) === 1) {
        S.stats.checks++;
        const { k, where, stackOnly } = diffText(rbtestRefSlot, other, 16);
        if (stackOnly) S.stats.stackNoise = (S.stats.stackNoise || 0) + 1;
        else if (k !== 0) {
          S.stats.mismatches++;
          log(`rbtest: read ${n}: ${k} pages differ after a ${rbDepth}-frame re-run: ${where.join(' ')}`);
          if (realDiag) realDiag('net', JSON.stringify({ n, rbtest: 'differ', pages: k, where }));
        }
      }
      rbtestRef = -1;
    }
    // rbtest: a re-run frame's state against the first run's snapshot of it.
    if (mode === 'rbtest' && S.resim && n % snapEvery === 0 && Module._fsa_net_slot_frame(slotOf(n)) === n && !S.rbReported) {
      if (Module._fsa_net_snapshot(SPARE + 1, n) === 1) {
        const { k, where, stackOnly } = diffText(slotOf(n), SPARE + 1, 16);
        if (stackOnly) S.stats.stackNoise = (S.stats.stackNoise || 0) + 1;
        else if (k > 0) {
          S.rbReported = true;  // the first frame that differs, once per rollback
          log(`rbtest: re-run read ${n} (rolled back to ${S.rbFrom}): ${k} pages differ from the first run: ${where.join(' ')}`);
          if (realDiag) realDiag('net', JSON.stringify({ n, rbtest: 'rerun-differs', from: S.rbFrom, pages: k, where }));
        }
      }
    }
    if (n % snapEvery === 0) {
      const t0 = now();
      if (Module._fsa_net_snapshot(slotOf(n), n) === 1) { S.stats.snaps++; S.stats.snapMs += now() - t0; }
    }
  };

  // --- Inputs of frame n (called by C after the script; may wait) -------------
  const FRAME_MS = 1001 / 60;
  S.inputs = async (n) => {
    S.cur = n;
    const v = io();
    if (n === S.head) {
      // A new frame: the local inputs go into the timeline, D frames ahead.
      for (let i = 0; i < S.local.length; i++) {
        const p = S.local[i];
        // The source: in script mode (or rbtest) the script's pad of that port,
        // else this browser's own pad (port 0: keyboard, touch, gamepad).
        const src = (S.scriptPads || mode === 'rbtest') ? p : 0;
        const val = [v[3 * src] >>> 0, v[3 * src + 1] >>> 0, (v[3 * src + 2] << 24) >> 24];
        if (val[2] !== 0 && p < S.seats) { val[0] = 0; val[1] = 0; val[2] = 0; }  // a seat is always plugged in
        const target = n + S.delay;
        const first = S.lastStamped[i] + 1;
        if (target < first) continue;  // the delay went down: this read's input is dropped locally
        const vals = [];
        for (let f = first; f <= target; f++) vals.push(val);
        S.lastStamped[i] = target;
        S.receive(p, first, vals);
        if (S.onLocalInputs) S.onLocalInputs(p, first, vals, n);
      }
      S.head = n + 1;
      if (rollback && !S.predictOk) S.stats.gatedFrames++;
      if (S.adaptive && S.adapt) S.adapt();
      // Wait while too far ahead: lockstep waits for every input of frame n;
      // rollback for the frontier to come within the window.
      const behind = () => (rollback ? n - S.frontier >= window_ || (!S.predictOk && !allKnown(n)) : !allKnown(n));
      if (behind()) {
        const t0 = now();
        S.stats.stalls++;
        while (behind() && !S.dead) await new Promise((r) => S.waiters.push(r));
        const ms = now() - t0;
        S.stats.stallMs += ms;
        S.stats.stallMax = Math.max(S.stats.stallMax, ms);
        if (S.onStall) S.onStall(ms);
      } else if (rollback && S.timeSync) {
        // Frame advantage (GGPO-style time sync): when this browser runs ahead
        // of the others, it waits a frame now and then so that it does not
        // carry every rollback alone.
        if (S.timeSync(n)) {
          S.stats.timeSyncWaits++;
          await new Promise((r) => setTimeout(r, FRAME_MS));
        }
      }
    }
    // The four pads of frame n: known, else predicted (rollback; the latest known).
    for (let p = 0; p < PORTS; p++) {
      let x = known(p, n);
      if (!x) {
        if (!rollback) throw new Error('lockstep: input missing at ' + n);
        x = lastKnown(p, n);
        if (n === S.head - 1) S.stats.predicted++;
      }
      used[p].set(n, x[0], x[1], x[2]);
      v[3 * p] = x[0]; v[3 * p + 1] = x[1]; v[3 * p + 2] = x[2];
    }
    if (rollback && S.resim) return;  // no wait between re-simulated frames
  };
  const allKnown = (n) => { for (let p = 0; p < S.seats; p++) if (!known(p, n)) return false; return true; };

  // --- Desync: loud, never silent ----------------------------------------------
  S.fatal = (msg) => {
    log('DESYNC: ' + msg);
    S.stats.desyncs++;
    if (realDiag) realDiag('net', JSON.stringify({ n: S.cur, desync: msg }));
    if (S.onDesync) S.onDesync(msg);
  };
  S.report = () => {
    const st = S.stats;
    return { mode, n: S.cur, head: S.head, frontier: S.frontier, delay: S.delay, seats: S.seats, local: S.local,
             ...st, snapAvgMs: st.snaps ? st.snapMs / st.snaps : 0, restoreAvgMs: st.rollbacks ? st.restoreMs / st.rollbacks : 0,
             resimAvgMs: st.resimFrames ? st.resimMs / st.resimFrames : 0,
             snapBytes: rollback ? Module._fsa_net_snapshot_bytes(0) : 0 };
  };
  window.fsaNetReport = () => JSON.stringify(S.report());
  // How much of MEM1 a snapshot needs (docs/NETPLAY.md, measurements): the
  // highest non-zero page, the non-zero pages, and how many 4 KiB pages of the
  // whole snapshot differ between the latest one and the ones d reads before
  // (needs ?snapevery=1 and a ring of at least 21 slots).
  window.fsaNetMemStats = () => {
    const lo = Module._fsa_net_mem1_start(), hi = Module._fsa_net_mem1_end();
    const m = HEAPU8.subarray(lo, hi);
    let last = 0, nz = 0;
    for (let p = 0; p < m.length; p += 4096) {
      for (let j = p; j < p + 4096; j++) if (m[j]) { nz++; last = p + 4096; break; }
    }
    const hb = Module._malloc(4 * 81);
    Module._fsa_net_heap_info(hb);
    const hv = new Uint32Array(HEAPU8.buffer, hb, 81);
    const parts = [];
    let partBytes = 0;
    for (let i = 0; i < hv[0]; i++) { parts.push(hv[1 + 2 * i].toString(16) + '-' + hv[2 + 2 * i].toString(16)); partBytes += hv[2 + 2 * i] - hv[1 + 2 * i]; }
    Module._free(hb);
    const res = { n: S.head - 1, mem1Parts: parts.length, mem1PartBytes: partBytes, parts: parts.slice(0, 8), mem1: hi - lo, highestNonzero: last, nonzeroPages: nz, pagesTotal: Math.ceil(Module._fsa_net_snapshot_bytes(0) / 4096) };
    const n = S.head - 1;
    const out = Module._malloc(16);
    for (const d of [1, 4, 8, 20]) {
      const a = slotOf(n), b = slotOf(n - d);
      if (Module._fsa_net_slot_frame(a) !== n || Module._fsa_net_slot_frame(b) !== n - d) continue;
      res['changedPages_' + d] = Module._fsa_net_slot_diff(a, b, out, 1);
    }
    Module._free(out);
    return JSON.stringify(res);
  };

  if (sess) {
    S.configure(sess.seats, [sess.seat]);
    S.scriptPads = !!ro.script;
    for (const l of sess.left || []) S.seatLeft(l.seat, l.at);
    sess.attach(S);
    log(`room ${sess.room}: seat ${sess.seat + 1} of ${sess.seats}, ${mode}, delay ${S.delay}${S.adaptive ? ' (adaptive)' : ''}` +
        (rollback ? `, window ${window_}, a snapshot every ${snapEvery} (${ringSlots} slots)` : ''));
    if (realDiag) realDiag('net', JSON.stringify({ t: 'start', room: sess.room, seat: sess.seat, seats: sess.seats, mode, opts: ro }));
    setInterval(() => { if (realDiag) realDiag('net', JSON.stringify({ t: performance.now(), ...S.report(), ...(sess.report ? sess.report() : {}) })); }, 5000);
  }
  if (mode === 'rbtest') {
    const seats = Math.max(1, num('seats', Number(q.get('gba')) || 1));
    S.configure(seats, Array.from({ length: seats }, (_, i) => i));
    S.scriptPads = !!q.get('script');
    log(`rbtest: roll back ${rbDepth} frames every ${rbEvery} reads; ${ringSlots} slots, a snapshot every ${snapEvery}; ` +
        `${regions.ranges.length} static ranges (${regions.bytes} bytes)`);
    setInterval(() => { if (realDiag) realDiag('net', JSON.stringify({ t: performance.now(), ...S.report() })); }, 5000);
  }
  return true;
};
