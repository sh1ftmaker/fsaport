// --pre-js (deluxe): the page side of the native GBA link (platform/gba_link.cpp,
// platform/gba_view.cpp, docs/MULTIPLAYER.md).
//
//   ?gba=N         plug a GBA (local peer) into ports 0..N-1 (N = 1..4)
//   ?gba=0,2       ... or into the listed ports
//   ?gbaview=0     no panel (the GBAs still work)
//   ?gbaview=native|gc|both  how the panel draws each GBA screen (below); ?gbascale=S (or ?gbaview=S): its scale
//   ?gbashots=1    log a native / GameCube side-by-side shot of each screen when it settles (tools/gba_shots.py)
//
// Each GBA's buttons are the pad of its port (keyboard / gamepad / the harness's
// four pads), plus these keys for ports 1-3 in live play:
//   port 1: IJKL, U = A, O = B, P = Start, 8 = Select, 7 = L, 9 = R
//   port 2: TFGH, R = A, Y = B, V = Start
//   port 3: arrows, Numpad1 = A, Numpad2 = B, Numpad3 = Start
// The panel shows each GBA's native view (240 x 160) and its link state.
Module.fsaGbaInit = function () {
  const q = new URLSearchParams(location.search);
  // A netplay room plugs one GBA per seat (docs/NETPLAY.md).
  const arg = Module.fsaNetSession ? String(Module.fsaNetSession.seats) : q.get('gba');
  if (!arg) return;
  let ports = [];
  if (/^[1-4]$/.test(arg)) for (let i = 0; i < Number(arg); i++) ports.push(i);
  else ports = arg.split(',').map(Number).filter((p) => p >= 0 && p < 4);
  const log = (s) => (window.fsaLog || console.log)('[gba] ' + s);
  for (const p of ports) Module._fsa_gba_attach(p, 1);
  log('GBAs in ports ' + ports.join(', '));
  window.fsaGba = {
    attach: (p, on = 1) => Module._fsa_gba_attach(p, on),
    link: (p) => info(Module._fsa_gba_link_info, p, 9),
    view: (p) => decodeView(p),
    memory: (p, region, len) => HEAPU8.slice(Module._fsa_gba_memory(p, region), Module._fsa_gba_memory(p, region) + len),
    // One line per GBA for logs and scripted runs: link state, record kinds seen,
    // the latest record (hex words), the files placed (address:bytes/sent/flag).
    dump: () => JSON.stringify(ports.map((p) => {
      const v = decodeView(p);
      const hex = (x) => (x >>> 0).toString(16);
      return { port: p, reads: Module._fsa_harness_reads(), link: window.fsaGba.link(p), kinds: v.kinds,
               record: v.record.map(hex), files: v.files.map((f) => hex(f.addr) + ':' + f.size + '/' + f.sent + '/' + f.flag),
               message: v.message.length };
    })),
  };

  const scratch = Module._malloc(4 * 2048);
  function info(fn, p, n) {
    const k = fn(p, scratch);
    return Array.from(new Uint32Array(HEAPU8.buffer, scratch, Math.max(k, n)));
  }
  function decodeView(p) {
    const a = info(Module._fsa_gba_view_info, p, 0);
    let k = 0;
    const v = { records: a[k++], kinds: a.slice(k, k + 16) };
    k += 16;
    const rw = a[k++];
    v.record = a.slice(k, k + rw);
    k += 26;
    const fc = a[k++];
    v.files = [];
    for (let i = 0; i < 32; i++, k += 4)
      if (i < fc) v.files.push({ addr: a[k], size: a[k + 1], sent: a[k + 2], flag: a[k + 3] });
    const ml = a[k++];
    v.message = a.slice(k, k + ml);
    return v;
  }

  // Live keys for ports 1-3 (GBA_* bits).
  const GBA = { A: 1, B: 2, SELECT: 4, START: 8, RIGHT: 0x10, LEFT: 0x20, UP: 0x40, DOWN: 0x80, R: 0x100, L: 0x200 };
  const KEYS = {
    1: { KeyI: 'UP', KeyJ: 'LEFT', KeyK: 'DOWN', KeyL: 'RIGHT', KeyU: 'A', KeyO: 'B', KeyP: 'START', Digit8: 'SELECT', Digit7: 'L', Digit9: 'R' },
    2: { KeyT: 'UP', KeyF: 'LEFT', KeyG: 'DOWN', KeyH: 'RIGHT', KeyR: 'A', KeyY: 'B', KeyV: 'START' },
    3: { ArrowUp: 'UP', ArrowLeft: 'LEFT', ArrowDown: 'DOWN', ArrowRight: 'RIGHT', Numpad1: 'A', Numpad2: 'B', Numpad3: 'START' },
  };
  const held = [0, 0, 0, 0];
  const onKey = (down) => (e) => {
    if (Module.fsaCoopOn) return;  // local co-op routes the keyboard itself (fsa_coop.js)
    for (const p in KEYS) {
      const name = KEYS[p][e.code];
      if (!name) continue;
      held[p] = down ? held[p] | GBA[name] : held[p] & ~GBA[name];
      Module._fsa_gba_page_keys(Number(p), held[p]);
    }
  };
  window.addEventListener('keydown', onKey(true));
  window.addEventListener('keyup', onKey(false));

  if (q.get('gbaview') === '0') return;
  // The panel: each GBA's screen, drawn natively from what the game sends it
  // (platform/gba_view.cpp, approach A) or by the game's own GameCube renderer
  // (game/src/port/gba_gcview.cpp, approach B; rooms only, the overworld falls
  // back to the native view), or both side by side.
  //   ?gbaview=native|gc|both   the mode (default native); a button per panel cycles it
  //   ?gbaview=S / ?gbascale=S  the panel's scale (default 2/3; 1 = the GBA's 240 x 160)
  const VIEWMODES = ['native', 'gc', 'both'];
  const arg2 = q.get('gbaview') || '';
  let mode = VIEWMODES.includes(arg2) ? arg2 : 'native';
  const scale = Number(q.get('gbascale')) || Number(arg2) || 2 / 3;
  const setGc = () => Module._fsa_gba_gcview(mode === 'native' ? 0 : ports.reduce((m, p) => m | (1 << p), 0));
  setGc();
  const panel = document.createElement('div');
  panel.id = 'gbapanel';
  panel.style.cssText = 'position:fixed;left:4px;bottom:4px;z-index:50;display:flex;flex-wrap:wrap;gap:4px;' +
    'pointer-events:none;font:9px/1.25 ui-monospace,monospace;color:#ddd;opacity:.92';
  const serialBuf = Module._malloc(4);
  // The GameCube-drawn panel of port p (240 x 160 RGBA) or null (not in a room / not on).
  const gcPixels = (p) => {
    if (mode === 'native') return null;
    const ptr = Module._fsa_gba_gcview_pixels(p, serialBuf);
    return ptr ? HEAPU8.subarray(ptr, ptr + 240 * 160 * 4) : null;
  };
  const nativePixels = (p) => {
    const ptr = Module._fsa_gba_render(p);
    return HEAPU8.subarray(ptr, ptr + 240 * 160 * 4);
  };
  const views = ports.map((p) => {
    const box = document.createElement('div');
    box.style.cssText = 'background:rgba(0,0,0,.75);padding:4px;border-radius:4px';
    const row = document.createElement('div');
    row.style.cssText = 'display:flex;gap:3px';
    const mk = () => {
      const c = document.createElement('canvas');
      c.width = 240;
      c.height = 160;
      c.style.cssText = `display:block;width:${240 * scale}px;height:${160 * scale}px;image-rendering:pixelated`;
      row.append(c);
      return { c, ctx: c.getContext('2d'), img: new ImageData(240, 160) };
    };
    const a = mk(), b = mk();
    const head = document.createElement('div');
    head.style.cssText = 'display:flex;justify-content:space-between;align-items:center;gap:4px';
    const t = document.createElement('div');
    t.style.cssText = `white-space:pre;overflow:hidden`;
    const btn = document.createElement('button');
    btn.style.cssText = 'pointer-events:auto;font:inherit;padding:1px 5px;border-radius:3px;border:1px solid #666;background:#222;color:#ddd;cursor:pointer';
    btn.title = 'GBA screen: native rebuild / GameCube-drawn / both';
    btn.onclick = () => window.fsaGba.mode(VIEWMODES[(VIEWMODES.indexOf(mode) + 1) % VIEWMODES.length]);
    head.append(t, btn);
    box.append(row, head);
    panel.append(box);
    return { p, a, b, text: t, btn };
  });
  document.body.append(panel);
  window.fsaGba.mode = (m) => {
    if (m !== undefined && VIEWMODES.includes(m)) {
      mode = m;
      setGc();
      draw();
    }
    return mode;
  };
  window.fsaGba.native = (p) => nativePixels(p).slice();
  window.fsaGba.gc = (p) => { const px = gcPixels(p); return px ? px.slice() : null; };
  // A PNG (data URL) of port p's screen, 'native' or 'gc' (null when the GameCube drew none).
  window.fsaGba.png = (p, which = 'native') => {
    const px = which === 'gc' ? gcPixels(p) : nativePixels(p);
    if (!px) return null;
    const c = document.createElement('canvas');
    c.width = 240;
    c.height = 160;
    const img = new ImageData(240, 160);
    img.data.set(px);
    c.getContext('2d').putImageData(img, 0, 0);
    return c.toDataURL('image/png');
  };
  // Side by side for comparisons (docs/MULTIPLAYER.md, "GBA screen"): port p's
  // native and GameCube-drawn screens of the same frame and their difference
  // (pixels whose colour differs by more than 24 in a channel, in magenta), at
  // scale 2, as a PNG data URL plus the count of differing pixels. Logged as one
  // "GBASHOT <name> <port> <json>" line by fsaGba.logShot (tools/gba_shots.py
  // turns such lines into files).
  window.fsaGba.shot = (p) => {
    // The GameCube panel arrives a few frames late: compare it with the native
    // screen of its own frame (kept by game/src/port/gba_gcview.cpp).
    const g = gcPixels(p);
    const b = g ? g.slice() : null;
    const nf = b ? Module._fsa_gba_gcview_frame_native(p) : 0;
    const a = nf ? HEAPU8.slice(nf, nf + 240 * 160 * 4) : nativePixels(p).slice();
    const S = 2, W = 240 * S, H = 160 * S, gap = 6;
    const c = document.createElement('canvas');
    c.width = W * 3 + gap * 2;
    c.height = H;
    const ctx = c.getContext('2d');
    ctx.fillStyle = '#111';
    ctx.fillRect(0, 0, c.width, c.height);
    ctx.imageSmoothingEnabled = false;
    const put = (px, x) => {
      const t = document.createElement('canvas');
      t.width = 240;
      t.height = 160;
      const img = new ImageData(240, 160);
      img.data.set(px);
      t.getContext('2d').putImageData(img, 0, 0);
      ctx.drawImage(t, x, 0, W, H);
    };
    put(a, 0);
    let diff = -1;
    if (b) {
      put(b, W + gap);
      const d = new Uint8ClampedArray(240 * 160 * 4);
      diff = 0;
      for (let i = 0; i < 240 * 160; i++) {
        const k = i * 4;
        const off = Math.abs(a[k] - b[k]) > 24 || Math.abs(a[k + 1] - b[k + 1]) > 24 || Math.abs(a[k + 2] - b[k + 2]) > 24;
        if (off) diff++;
        const grey = (a[k] + a[k + 1] + a[k + 2]) / 9;
        d[k] = off ? 255 : grey;
        d[k + 1] = off ? 0 : grey;
        d[k + 2] = off ? 255 : grey;
        d[k + 3] = 255;
      }
      put(d, 2 * (W + gap));
    }
    const single = (px) => {
      if (!px) return null;
      const t = document.createElement('canvas');
      t.width = 240;
      t.height = 160;
      const img = new ImageData(240, 160);
      img.data.set(px);
      t.getContext('2d').putImageData(img, 0, 0);
      return t.toDataURL('image/png');
    };
    return { port: p, reads: Module._fsa_harness_reads(), sameFrame: !!nf, diff, total: 240 * 160,
             png: c.toDataURL('image/png'), native: single(a), gc: single(b) };
  };
  window.fsaGba.logShot = (name, p) => {
    const r = window.fsaGba.shot(p);
    console.log('GBASHOT ' + name + ' ' + p + ' ' + JSON.stringify(r));
    return r.diff;
  };
  const MODES = ['off', 'code', 'disc-id', 'cmd', 'packet', 'frame'];
  function draw() {
    for (const v of views) {
      const gc = gcPixels(v.p);
      const first = mode === 'gc' && gc ? gc : nativePixels(v.p);
      v.a.img.data.set(first);
      v.a.ctx.putImageData(v.a.img, 0, 0);
      v.b.c.style.display = mode === 'both' ? 'block' : 'none';
      if (mode === 'both') {
        if (gc) v.b.img.data.set(gc);
        else v.b.img.data.fill(0);
        v.b.ctx.putImageData(v.b.img, 0, 0);
      }
      const l = window.fsaGba.link(v.p);
      const d = decodeView(v.p);
      const kind = d.record.length ? (d.record[0] >>> 24) & 15 : -1;
      const valid = d.record.length ? (d.record[0] >>> 19) & 7 : 0;
      const kw = l[7] >>> 0, gk = ((kw >>> 16) & 0xFF) | (kw & 0x300);
      const held = Object.keys(GBA).filter((n) => gk & GBA[n]).join(' ') || '-';
      const shown = mode === 'native' ? 'native' : mode === 'both' ? 'native | GameCube' : gc ? 'GameCube-drawn' : 'native (GameCube draws rooms only)';
      v.btn.textContent = mode;
      v.text.textContent =
        `P${v.p + 1} ${l[1] ? 'linked' : 'plugged'} ${MODES[l[2]]} frames ${l[4]} files ${l[5]}\n` +
        `record ${kind}${kind === 5 ? (valid ? ' room' : ' overworld') : ''}, ${d.record.length} words\n` +
        `${shown}; keys ${held}`;
    }
  }
  setInterval(draw, 100);

  // ?gbashots=1: log a side-by-side shot (fsaGba.logShot) of each port once its
  // screen has settled in a new state (overworld / room) for 90 pad reads, so a
  // scripted run records comparable frames by itself (tools/gba_shots.py).
  if (q.get('gbashots')) {
    const seen = ports.map(() => ({ state: '', since: 0, done: '' }));
    setInterval(() => {
      const n = Module._fsa_harness_reads();
      ports.forEach((p, i) => {
        const d = decodeView(p);
        if (!d.record.length) return;
        const kind = (d.record[0] >>> 24) & 15;
        const valid = (d.record[0] >>> 19) & 7;
        if (kind !== 5 && kind !== 14) return;
        const state = valid ? 'room' + valid : 'overworld';
        const s = seen[i];
        if (state !== s.state) { s.state = state; s.since = n; return; }
        if (n - s.since >= 90 && s.done !== state + s.since) {
          s.done = state + s.since;
          window.fsaGba.logShot(state + '-' + n, p);
        }
      });
    }, 250);
  }
};
